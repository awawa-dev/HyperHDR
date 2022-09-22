// project includes
#include <api/API.h>
#include <utils/Logger.h>
#include <utils/Components.h>


#include <utils/ColorRgb.h>
#include <utils/ColorSys.h>

// stl includes
#include <iostream>
#include <iterator>

// Qt includes
#include <QResource>
#include <QCryptographicHash>
#include <QImage>
#include <QBuffer>
#include <QByteArray>
#include <QTimer>
#include <base/GrabberWrapper.h>
#include <base/SystemWrapper.h>
#include <utils/jsonschema/QJsonSchemaChecker.h>
#include <HyperhdrConfig.h>
#include <utils/SysInfo.h>
#include <utils/ColorSys.h>
#include <flatbufserver/FlatBufferServer.h>

// bonjour wrapper
#include <bonjour/bonjourbrowserwrapper.h>

// ledmapping int <> string transform methods
#include <base/ImageProcessor.h>

// api includes
#include <api/JsonCB.h>

using namespace hyperhdr;

API::API(Logger* log, bool localConnection, QObject* parent)
	: QObject(parent)
{
	qRegisterMetaType<int64_t>("int64_t");
	qRegisterMetaType<std::map<int, registerData>>("std::map<int,registerData>");

	// Init
	_log = log;
	_authManager = AuthManager::getInstance();
	_instanceManager = HyperHdrIManager::getInstance();
	_localConnection = localConnection;

	_authorized = false;
	_adminAuthorized = false;

	_hyperhdr = _instanceManager->getHyperHdrInstance(0);
	_currInstanceIndex = 0;
	// TODO FIXME
	// report back current registers when a Hyperhdr instance request it
	//connect(ApiSync::getInstance(), &ApiSync::requestActiveRegister, this, &API::requestActiveRegister, Qt::QueuedConnection);

	// connect to possible token responses that has been requested
	connect(_authManager, &AuthManager::tokenResponse, [=](bool success, QObject* caller, const QString& token, const QString& comment, const QString& id, const int& tan)
		{
			if (this == caller)
				emit onTokenResponse(success, token, comment, id, tan);
		});

	// connect to possible startInstance responses that has been requested
	connect(_instanceManager, &HyperHdrIManager::startInstanceResponse, [=](QObject* caller, const int& tan)
		{
			if (this == caller)
				emit onStartInstanceResponse(tan);
		});
}

void API::init()
{
	bool apiAuthRequired = _authManager->isAuthRequired();

	// For security we block external connections if default PW is set
	if (!_localConnection && API::hasHyperhdrDefaultPw())
	{
		emit forceClose();
	}
	// if this is localConnection and network allows unauth locals, set authorized flag
	if (apiAuthRequired && _localConnection)
		_authorized = !_authManager->isLocalAuthRequired();

	// admin access is allowed, when the connection is local and the option for local admin isn't set. Con: All local connections get full access
	if (_localConnection)
	{
		_adminAuthorized = !_authManager->isLocalAdminAuthRequired();
		// just in positive direction
		if (_adminAuthorized)
			_authorized = true;
	}
}

void API::setColor(int priority, const std::vector<uint8_t>& ledColors, int timeout_ms, const QString& origin, hyperhdr::Components callerComp)
{
	std::vector<ColorRgb> fledColors;
	if (ledColors.size() % 3 == 0)
	{
		for (uint64_t i = 0; i < ledColors.size(); i += 3)
		{
			fledColors.emplace_back(ColorRgb{ ledColors[i], ledColors[i + 1], ledColors[i + 2] });
		}
		QMetaObject::invokeMethod(_hyperhdr, "setColor", Qt::QueuedConnection, Q_ARG(int, priority), Q_ARG(std::vector<ColorRgb>, fledColors), Q_ARG(int, timeout_ms), Q_ARG(QString, origin));
	}
}

bool API::setImage(ImageCmdData& data, hyperhdr::Components comp, QString& replyMsg, hyperhdr::Components callerComp)
{
	// truncate name length
	data.imgName.truncate(16);

	if (data.format == "auto")
	{
		QImage img = QImage::fromData(data.data);
		if (img.isNull())
		{
			replyMsg = "Failed to parse picture, the file might be corrupted";
			return false;
		}

		// check for requested scale
		if (data.scale > 24)
		{
			if (img.height() > data.scale)
			{
				img = img.scaledToHeight(data.scale);
			}
			if (img.width() > data.scale)
			{
				img = img.scaledToWidth(data.scale);
			}
		}

		// check if we need to force a scale
		if (img.width() > 2000 || img.height() > 2000)
		{
			data.scale = 2000;
			if (img.height() > data.scale)
			{
				img = img.scaledToHeight(data.scale);
			}
			if (img.width() > data.scale)
			{
				img = img.scaledToWidth(data.scale);
			}
		}

		data.width = img.width();
		data.height = img.height();

		// extract image
		img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
		data.data.clear();
		data.data.reserve(img.width() * img.height() * 3);
		for (int i = 0; i < img.height(); ++i)
		{
			const QRgb* scanline = reinterpret_cast<const QRgb*>(img.scanLine(i));
			for (int j = 0; j < img.width(); ++j)
			{
				data.data.append((char)qRed(scanline[j]));
				data.data.append((char)qGreen(scanline[j]));
				data.data.append((char)qBlue(scanline[j]));
			}
		}
	}
	else
	{
		// check consistency of the size of the received data
		if (data.data.size() != data.width * data.height * 3)
		{
			replyMsg = "Size of image data does not match with the width and height";
			return false;
		}
	}

	// copy image
	Image<ColorRgb> image(data.width, data.height);
	memcpy(image.memptr(), data.data.data(), data.data.size());

	QMetaObject::invokeMethod(_hyperhdr, "registerInput", Qt::QueuedConnection, Q_ARG(int, data.priority), Q_ARG(hyperhdr::Components, comp), Q_ARG(QString, data.origin), Q_ARG(QString, data.imgName));
	QMetaObject::invokeMethod(_hyperhdr, "setInputImage", Qt::QueuedConnection, Q_ARG(int, data.priority), Q_ARG(Image<ColorRgb>, image), Q_ARG(int64_t, data.duration));

	return true;
}

bool API::clearPriority(int priority, QString& replyMsg, hyperhdr::Components callerComp)
{
	if (priority < 0 || (priority > 0 && priority < PriorityMuxer::LOWEST_EFFECT_PRIORITY))
	{
		QMetaObject::invokeMethod(_hyperhdr, "clear", Qt::QueuedConnection, Q_ARG(int, priority));
	}
	else
	{
		replyMsg = QString("Priority %1 is not allowed to be cleared").arg(priority);
		return false;
	}
	return true;
}

bool API::setComponentState(const QString& comp, bool& compState, QString& replyMsg, hyperhdr::Components callerComp)
{
	QString input(comp);
	if (input == "GRABBER")
		input = "SYSTEMGRABBER";
	if (input == "V4L")
		input = "VIDEOGRABBER";
	Components component = stringToComponent(input);
	if (component == COMP_ALL)
	{
		QMetaObject::invokeMethod(HyperHdrIManager::getInstance(), "toggleStateAllInstances", Qt::QueuedConnection, Q_ARG(bool, compState));

		return true;
	}
	else if (component == COMP_HDR)
	{
		setVideoModeHdr((compState) ? 1 : 0, component);
		return true;
	}
	else if (component != COMP_INVALID)
	{
		QMetaObject::invokeMethod(_hyperhdr, "compStateChangeRequest", Qt::QueuedConnection, Q_ARG(hyperhdr::Components, component), Q_ARG(bool, compState));
		return true;
	}
	replyMsg = QString("Unknown component name: %1").arg(comp);
	return false;
}

void API::setLedMappingType(int type, hyperhdr::Components callerComp)
{
	QMetaObject::invokeMethod(_hyperhdr, "setLedMappingType", Qt::QueuedConnection, Q_ARG(int, type));
}

void API::setVideoModeHdr(int hdr, hyperhdr::Components callerComp)
{
	if (GrabberWrapper::getInstance() != nullptr)
		QMetaObject::invokeMethod(GrabberWrapper::getInstance(), "setHdrToneMappingEnabled", Qt::QueuedConnection, Q_ARG(int, hdr));

	if (FlatBufferServer::getInstance() != nullptr)
		QMetaObject::invokeMethod(FlatBufferServer::getInstance(), "setHdrToneMappingEnabled", Qt::QueuedConnection, Q_ARG(int, hdr));
}

void API::setFlatbufferUserLUT(QString userLUTfile)
{
	if (FlatBufferServer::getInstance() != nullptr)
		QMetaObject::invokeMethod(FlatBufferServer::getInstance(), "setUserLut", Qt::QueuedConnection, Q_ARG(QString, userLUTfile));
}

bool API::setEffect(const EffectCmdData& dat, hyperhdr::Components callerComp)
{
	int res;
	if (!dat.args.isEmpty())
	{
		QMetaObject::invokeMethod(_hyperhdr, "setEffect", Qt::BlockingQueuedConnection, Q_RETURN_ARG(int, res), Q_ARG(QString, dat.effectName), Q_ARG(QJsonObject, dat.args), Q_ARG(int, dat.priority), Q_ARG(int, dat.duration), Q_ARG(QString, dat.pythonScript), Q_ARG(QString, dat.origin), Q_ARG(QString, dat.data));
	}
	else
	{
		QMetaObject::invokeMethod(_hyperhdr, "setEffect", Qt::BlockingQueuedConnection, Q_RETURN_ARG(int, res), Q_ARG(QString, dat.effectName), Q_ARG(int, dat.priority), Q_ARG(int, dat.duration), Q_ARG(QString, dat.origin));
	}

	return res >= 0;
}

void API::setSourceAutoSelect(bool state, hyperhdr::Components callerComp)
{
	QMetaObject::invokeMethod(_hyperhdr, "setSourceAutoSelect", Qt::QueuedConnection, Q_ARG(bool, state));
}

void API::setVisiblePriority(int priority, hyperhdr::Components callerComp)
{
	QMetaObject::invokeMethod(_hyperhdr, "setVisiblePriority", Qt::QueuedConnection, Q_ARG(int, priority));
}

void API::registerInput(int priority, hyperhdr::Components component, const QString& origin, const QString& owner, hyperhdr::Components callerComp)
{
	if (_activeRegisters.count(priority))
		_activeRegisters.erase(priority);

	_activeRegisters.insert({ priority, registerData{component, origin, owner, callerComp} });

	QMetaObject::invokeMethod(_hyperhdr, "registerInput", Qt::QueuedConnection, Q_ARG(int, priority), Q_ARG(hyperhdr::Components, component), Q_ARG(QString, origin), Q_ARG(QString, owner));
}

void API::unregisterInput(int priority)
{
	if (_activeRegisters.count(priority))
		_activeRegisters.erase(priority);
}

bool API::setHyperhdrInstance(quint8 inst)
{
	if (_currInstanceIndex == inst)
		return true;
	bool isRunning;
	QMetaObject::invokeMethod(_instanceManager, "IsInstanceRunning", Qt::DirectConnection, Q_RETURN_ARG(bool, isRunning), Q_ARG(quint8, inst));
	if (!isRunning)
		return false;

	disconnect(_hyperhdr, 0, this, 0);
	QMetaObject::invokeMethod(_instanceManager, "getHyperHdrInstance", Qt::DirectConnection, Q_RETURN_ARG(HyperHdrInstance*, _hyperhdr), Q_ARG(quint8, inst));
	_currInstanceIndex = inst;
	return true;
}

std::map<hyperhdr::Components, bool> API::getAllComponents()
{
	std::map<hyperhdr::Components, bool> comps;
	//QMetaObject::invokeMethod(_hyperhdr, "getAllComponents", Qt::BlockingQueuedConnection, Q_RETURN_ARG(std::map<hyperhdr::Components, bool>, comps));
	return comps;
}

bool API::isHyperhdrEnabled()
{
	int res;
	QMetaObject::invokeMethod(_hyperhdr, "isComponentEnabled", Qt::BlockingQueuedConnection, Q_RETURN_ARG(int, res), Q_ARG(hyperhdr::Components, hyperhdr::COMP_ALL));
	return res > 0;
}

QVector<QVariantMap> API::getAllInstanceData()
{
	QVector<QVariantMap> vec;
	QMetaObject::invokeMethod(_instanceManager, "getInstanceData", Qt::DirectConnection, Q_RETURN_ARG(QVector<QVariantMap>, vec));
	return vec;
}

bool API::startInstance(quint8 index, int tan)
{
	bool res;
	(_instanceManager->thread() != this->thread())
		? QMetaObject::invokeMethod(_instanceManager, "startInstance", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, res), Q_ARG(quint8, index), Q_ARG(bool, false), Q_ARG(QObject*, this), Q_ARG(int, tan))
		: res = _instanceManager->startInstance(index, false, this, tan);

	return res;
}

void API::stopInstance(quint8 index)
{
	QMetaObject::invokeMethod(_instanceManager, "stopInstance", Qt::QueuedConnection, Q_ARG(quint8, index));
}

void API::requestActiveRegister(QObject* callerInstance)
{
	// TODO FIXME
	//if (_activeRegisters.size())
	//   QMetaObject::invokeMethod(ApiSync::getInstance(), "answerActiveRegister", Qt::QueuedConnection, Q_ARG(QObject *, callerInstance), Q_ARG(MapRegister, _activeRegisters));
}

bool API::deleteInstance(quint8 index, QString& replyMsg)
{
	if (_adminAuthorized)
	{
		QMetaObject::invokeMethod(_instanceManager, "deleteInstance", Qt::QueuedConnection, Q_ARG(quint8, index));
		return true;
	}
	replyMsg = NO_AUTH;
	return false;
}

QString API::createInstance(const QString& name)
{
	if (_adminAuthorized)
	{
		bool success;
		QMetaObject::invokeMethod(_instanceManager, "createInstance", Qt::DirectConnection, Q_RETURN_ARG(bool, success), Q_ARG(QString, name));
		if (!success)
			return QString("Instance name '%1' is already in use").arg(name);

		return "";
	}
	return NO_AUTH;
}

QString API::setInstanceName(quint8 index, const QString& name)
{
	if (_adminAuthorized)
	{
		QMetaObject::invokeMethod(_instanceManager, "saveName", Qt::QueuedConnection, Q_ARG(quint8, index), Q_ARG(QString, name));
		return "";
	}
	return NO_AUTH;
}

QString API::deleteEffect(const QString& name)
{
	if (_adminAuthorized)
	{
		QString res;
		QMetaObject::invokeMethod(_hyperhdr, "deleteEffect", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QString, res), Q_ARG(QString, name));
		return res;
	}
	return NO_AUTH;
}

QString API::saveEffect(const QJsonObject& data)
{
	if (_adminAuthorized)
	{
		QString res;
		QMetaObject::invokeMethod(_hyperhdr, "saveEffect", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QString, res), Q_ARG(QJsonObject, data));
		return res;
	}
	return NO_AUTH;
}

bool API::saveSettings(const QJsonObject& data)
{
	bool rc = true;
	if (!_adminAuthorized)
	{
		rc = false;
	}
	else
	{
		QMetaObject::invokeMethod(_hyperhdr, "saveSettings", Qt::DirectConnection, Q_RETURN_ARG(bool, rc), Q_ARG(QJsonObject, data), Q_ARG(bool, true));
	}
	return rc;
}

bool API::isAuthorized()
{
	return _authorized;
};

bool API::isAdminAuthorized()
{
	return _adminAuthorized;
};

bool API::updateHyperhdrPassword(const QString& password, const QString& newPassword)
{
	if (!_adminAuthorized)
		return false;
	bool res;
	QMetaObject::invokeMethod(_authManager, "updateUserPassword", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, res), Q_ARG(QString, DEFAULT_CONFIG_USER), Q_ARG(QString, password), Q_ARG(QString, newPassword));
	return res;
}

QString API::createToken(const QString& comment, AuthManager::AuthDefinition& def)
{
	if (!_adminAuthorized)
		return NO_AUTH;
	if (comment.isEmpty())
		return "comment is empty";
	QMetaObject::invokeMethod(_authManager, "createToken", Qt::BlockingQueuedConnection, Q_RETURN_ARG(AuthManager::AuthDefinition, def), Q_ARG(QString, comment));
	return "";
}

QString API::renameToken(const QString& id, const QString& comment)
{
	if (!_adminAuthorized)
		return NO_AUTH;
	if (comment.isEmpty() || id.isEmpty())
		return "Empty comment or id";

	QMetaObject::invokeMethod(_authManager, "renameToken", Qt::QueuedConnection, Q_ARG(QString, id), Q_ARG(QString, comment));
	return "";
}

QString API::deleteToken(const QString& id)
{
	if (!_adminAuthorized)
		return NO_AUTH;
	if (id.isEmpty())
		return "Empty id";

	QMetaObject::invokeMethod(_authManager, "deleteToken", Qt::QueuedConnection, Q_ARG(QString, id));
	return "";
}

void API::setNewTokenRequest(const QString& comment, const QString& id, const int& tan)
{
	QMetaObject::invokeMethod(_authManager, "setNewTokenRequest", Qt::QueuedConnection, Q_ARG(QObject*, this), Q_ARG(QString, comment), Q_ARG(QString, id), Q_ARG(int, tan));
}

void API::cancelNewTokenRequest(const QString& comment, const QString& id)
{
	QMetaObject::invokeMethod(_authManager, "cancelNewTokenRequest", Qt::QueuedConnection, Q_ARG(QObject*, this), Q_ARG(QString, comment), Q_ARG(QString, id));
}

bool API::handlePendingTokenRequest(const QString& id, bool accept)
{
	if (!_adminAuthorized)
		return false;
	QMetaObject::invokeMethod(_authManager, "handlePendingTokenRequest", Qt::QueuedConnection, Q_ARG(QString, id), Q_ARG(bool, accept));
	return true;
}

bool API::getTokenList(QVector<AuthManager::AuthDefinition>& def)
{
	if (!_adminAuthorized)
		return false;
	QMetaObject::invokeMethod(_authManager, "getTokenList", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QVector<AuthManager::AuthDefinition>, def));
	return true;
}

bool API::getPendingTokenRequests(QVector<AuthManager::AuthDefinition>& map)
{
	if (!_adminAuthorized)
		return false;
	QMetaObject::invokeMethod(_authManager, "getPendingRequests", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QVector<AuthManager::AuthDefinition>, map));
	return true;
}

bool API::isUserTokenAuthorized(const QString& userToken)
{
	bool res;
	QMetaObject::invokeMethod(_authManager, "isUserTokenAuthorized", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, res), Q_ARG(QString, DEFAULT_CONFIG_USER), Q_ARG(QString, userToken));
	if (res)
	{
		_authorized = true;
		_adminAuthorized = true;
		// Listen for ADMIN ACCESS protected signals
		connect(_authManager, &AuthManager::newPendingTokenRequest, this, &API::onPendingTokenRequest, Qt::UniqueConnection);
	}
	return res;
}

bool API::getUserToken(QString& userToken)
{
	if (!_adminAuthorized)
		return false;
	QMetaObject::invokeMethod(_authManager, "getUserToken", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QString, userToken));
	return true;
}

bool API::isTokenAuthorized(const QString& token)
{
	(_authManager->thread() != this->thread())
		? QMetaObject::invokeMethod(_authManager, "isTokenAuthorized", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, _authorized), Q_ARG(QString, token))
		: _authorized = _authManager->isTokenAuthorized(token);

	return _authorized;
}

bool API::isUserAuthorized(const QString& password)
{
	bool res;
	QMetaObject::invokeMethod(_authManager, "isUserAuthorized", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, res), Q_ARG(QString, DEFAULT_CONFIG_USER), Q_ARG(QString, password));
	if (res)
	{
		_authorized = true;
		_adminAuthorized = true;
		// Listen for ADMIN ACCESS protected signals
		connect(_authManager, &AuthManager::newPendingTokenRequest, this, &API::onPendingTokenRequest, Qt::UniqueConnection);
	}
	return res;
}

bool API::hasHyperhdrDefaultPw()
{
	bool res;
	QMetaObject::invokeMethod(_authManager, "isUserAuthorized", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, res), Q_ARG(QString, DEFAULT_CONFIG_USER), Q_ARG(QString, DEFAULT_CONFIG_PASSWORD));
	return res;
}

void API::logout()
{
	_authorized = false;
	_adminAuthorized = false;
	// Stop listenig for ADMIN ACCESS protected signals
	disconnect(_authManager, &AuthManager::newPendingTokenRequest, this, &API::onPendingTokenRequest);
	stopDataConnectionss();
}

void API::stopDataConnectionss()
{
}
