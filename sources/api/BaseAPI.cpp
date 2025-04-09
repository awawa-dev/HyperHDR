#ifndef PCH_ENABLED
	#include <QResource>
	#include <QCryptographicHash>
	#include <QBuffer>
	#include <QByteArray>
	#include <QTimer>
	#include <QNetworkReply>
	#include <QFile>

	#include <iostream>
	#include <iterator>

	#include <image/ColorRgb.h>
	#include <utils/Logger.h>
	#include <utils/Components.h>
#endif

#include <QHostInfo>
#include <QSslSocket>

#include <HyperhdrConfig.h>
#include <api/BaseAPI.h>
#include <base/ImageToLedManager.h>
#include <base/GrabberWrapper.h>
#include <base/SystemWrapper.h>
#include <base/Muxer.h>
#include <db/AuthTable.h>
#include <flatbuffers/server/FlatBuffersServer.h>
#include <json-utils/jsonschema/QJsonSchemaChecker.h>
#include <utils/GlobalSignals.h>
#include <base/GrabberHelper.h>
#include <utils-image/utils-image.h>

#ifdef ENABLE_ZSTD
	#include <utils-zstd/utils-zstd.h>
#endif

#ifdef _WIN32
	#include <QProcess>
#endif

using namespace hyperhdr;

BaseAPI::BaseAPI(Logger* log, bool localConnection, QObject* parent)
	: QObject(parent),
	_adminAuthorized(false),
	_log(log),
	_instanceManager(nullptr),
	_accessManager(nullptr),
	_soundCapture(nullptr),
	_videoGrabber(nullptr),
	_systemGrabber(nullptr),
	_performanceCounters(nullptr),
	_discoveryWrapper(nullptr),
	_authorized(false),
	_localConnection(localConnection),
	_currentInstanceIndex(0)
{
	qRegisterMetaType<int64_t>("int64_t");
	qRegisterMetaType<std::map<int, registerData>>("std::map<int,registerData>");

	emit GlobalSignals::getInstance()->SignalGetInstanceManager(_instanceManager);
	if (_instanceManager == nullptr)
	{
		Error(_log, "Instance manager is already removed");
		return;
	}

	emit GlobalSignals::getInstance()->SignalGetAccessManager(_accessManager);
	if (_accessManager == nullptr)
	{
		Error(_log, "Access manager is already removed");
		return;
	}

	emit GlobalSignals::getInstance()->SignalGetPerformanceCounters(_performanceCounters);
	if (_performanceCounters == nullptr)
	{
		Error(_log, "PerformanceCounters is already removed");
		return;
	}

	emit GlobalSignals::getInstance()->SignalGetSoundCapture(_soundCapture);	
	emit GlobalSignals::getInstance()->SignalGetVideoGrabber(_videoGrabber);
	emit GlobalSignals::getInstance()->SignalGetSystemGrabber(_systemGrabber);
	emit GlobalSignals::getInstance()->SignalGetDiscoveryWrapper(_discoveryWrapper);	
	
	SAFE_CALL_1_RET(_instanceManager.get(), getHyperHdrInstance, std::shared_ptr<HyperHdrInstance>, _hyperhdr, quint8, 0);
	if (_hyperhdr == nullptr)
	{
		Error(_log, "Could not get HyperHDR first instance");
		return;
	}	

	// connect to possible token responses that has been requested
	connect(_accessManager.get(), &AccessManager::SignalTokenNotifyClient, this, [this](bool success, QObject* caller, const QString& token, const QString& comment, const QString& id, const int& tan)
		{
			if (this == caller)
				emit SignalTokenClientNotification(success, token, comment, id, tan);
		});

	// connect to possible startInstance responses that has been requested
	connect(_instanceManager.get(), &HyperHdrManager::SignalStartInstanceResponse, this, [this](QObject* caller, const int& tan)
		{
			if (this == caller)
				emit SignalInstanceStartedClientNotification(tan);
		});
}

void BaseAPI::init()
{
	bool apiAuthRequired = _accessManager->isAuthRequired();

	// For security we block external connections if default PW is set
	if (!_localConnection && BaseAPI::hasHyperhdrDefaultPw())
	{
		emit SignalPerformClientDisconnection();
	}

	// if this is localConnection and network allows unauth locals, set authorized flag
	if (apiAuthRequired && _localConnection)
		_authorized = !_accessManager->isLocalAuthRequired();

	// admin access is allowed, when the connection is local and the option for local admin isn't set. Con: All local connections get full access
	if (_localConnection)
	{
		_adminAuthorized = !_accessManager->isLocalAdminAuthRequired();
		// just in positive direction
		if (_adminAuthorized)
			_authorized = true;
	}
}

bool BaseAPI::setHyperhdrInstance(quint8 inst)
{
	if (_currentInstanceIndex == inst)
		return true;

	if (_hyperhdr != nullptr)
		disconnect(_hyperhdr.get(), nullptr, this, nullptr);

	removeSubscriptions();

	SAFE_CALL_1_RET(_instanceManager.get(), getHyperHdrInstance, std::shared_ptr<HyperHdrInstance>, _hyperhdr, quint8, inst);
	_currentInstanceIndex = inst;

	addSubscriptions();

	return true;
}

bool BaseAPI::startInstance(quint8 index, int tan)
{
	bool res;

	SAFE_CALL_3_RET(_instanceManager.get(), startInstance, bool, res, quint8, index, QObject*, this, int, tan);

	return res;
}

void BaseAPI::stopInstance(quint8 index)
{
	QUEUE_CALL_1(_instanceManager.get(), stopInstance, quint8, index);
}

bool BaseAPI::deleteInstance(quint8 index, QString& replyMsg)
{
	if (_adminAuthorized)
	{
		QUEUE_CALL_1(_instanceManager.get(), deleteInstance, quint8, index);
		return true;
	}
	replyMsg = NO_AUTH;
	return false;
}

QString BaseAPI::createInstance(const QString& name)
{
	if (_adminAuthorized)
	{
		bool success = false;

		SAFE_CALL_1_RET(_instanceManager.get(), createInstance, bool, success, QString, name);

		if (!success)
			return QString("Instance name '%1' is already in use").arg(name);

		return "";
	}
	return NO_AUTH;
}

QString BaseAPI::setInstanceName(quint8 index, const QString& name)
{
	if (_adminAuthorized)
	{
		QUEUE_CALL_2(_instanceManager.get(), saveName, quint8, index, QString, name);
		return "";
	}
	return NO_AUTH;
}

void BaseAPI::setColor(int priority, const std::vector<uint8_t>& ledColors, int timeout_ms, const QString& origin, hyperhdr::Components callerComp)
{
	std::vector<ColorRgb> fledColors;
	if (ledColors.size() % 3 == 0)
	{
		for (uint64_t i = 0; i < ledColors.size(); i += 3)
		{
			fledColors.emplace_back(ColorRgb{ ledColors[i], ledColors[i + 1], ledColors[i + 2] });
		}
		QUEUE_CALL_4(_hyperhdr.get(), setColor, int, priority, std::vector<ColorRgb>, fledColors, int, timeout_ms, QString, origin);
	}
}

bool BaseAPI::setImage(ImageCmdData& data, hyperhdr::Components comp, QString& replyMsg, hyperhdr::Components callerComp)
{
	Image<ColorRgb> image;
	// truncate name length
	data.imgName.truncate(16);

	auto imageMemory = QByteArray::fromBase64(QByteArray(data.imagedata.toUtf8()));

	if (data.format == "rgb")
	{		
		if (imageMemory.size() != static_cast<long long>(data.width) * data.height * 3 || imageMemory.size() == 0)
		{
			replyMsg = "Size of image data does not match with the width and height";
			return false;
		}
		else if (imageMemory.size() >= 6ll*1024*1024)
		{
			replyMsg = "Image too large (max. 6MB)";
			return false;
		}
		image.resize(data.width, data.height);
		memcpy(image.rawMem(), imageMemory.data(), imageMemory.size());
	}
	else if (data.format == "auto")
	{		
		image = utils_image::load2image(reinterpret_cast<uint8_t*>(imageMemory.data()), imageMemory.size());
		

		if (image.width() == 1)
		{
			replyMsg = "Unsupported image";
			return false;
		}
	}
	else
	{
		replyMsg = "Unsupported image type";
		return false;
	}

	QUEUE_CALL_4(_hyperhdr.get(), registerInput, int, data.priority, hyperhdr::Components, comp, QString, data.origin, QString, data.imgName);
	QUEUE_CALL_3(_hyperhdr.get(), setInputImage, int, data.priority, Image<ColorRgb>, image, int64_t, data.duration);

	return true;
}

bool BaseAPI::clearPriority(int priority, QString& replyMsg, hyperhdr::Components callerComp)
{
	if (priority < 0 || (priority > 0 && priority < Muxer::LOWEST_EFFECT_PRIORITY))
	{
		QUEUE_CALL_1(_hyperhdr.get(), clear, int, priority);
	}
	else
	{
		replyMsg = QString("Priority %1 is not allowed to be cleared").arg(priority);
		return false;
	}
	return true;
}

bool BaseAPI::setComponentState(const QString& comp, bool& compState, QString& replyMsg, hyperhdr::Components callerComp)
{
	QString input(comp);
	if (input == "GRABBER")
		input = "SYSTEMGRABBER";
	if (input == "V4L")
		input = "VIDEOGRABBER";
	Components component = stringToComponent(input);
	if (component == COMP_ALL)
	{
		QUEUE_CALL_1(_instanceManager.get(), toggleStateAllInstances, bool, compState);

		return true;
	}
	else if (component == COMP_HDR)
	{
		setVideoModeHdr((compState) ? 1 : 0, component);
		return true;
	}
	else if (component != COMP_INVALID)
	{
		QUEUE_CALL_2(_hyperhdr.get(), SignalRequestComponent, hyperhdr::Components, component, bool, compState);
		return true;
	}
	replyMsg = QString("Unknown component name: %1").arg(comp);
	return false;
}

void BaseAPI::setLedMappingType(int type, hyperhdr::Components callerComp)
{
	QUEUE_CALL_1(_hyperhdr.get(), setLedMappingType, int, type);
}

void BaseAPI::setVideoModeHdr(int hdr, hyperhdr::Components callerComp)
{
	emit GlobalSignals::getInstance()->SignalRequestComponent(hyperhdr::Components::COMP_HDR, -1, hdr);
}

void BaseAPI::setFlatbufferUserLUT(QString userLUTfile)
{
	QJsonObject obj;	
	obj[BASEAPI_FLATBUFFER_USER_LUT_FILE] = userLUTfile;
	QJsonDocument updateSettings(obj);
	emit _instanceManager->SignalSettingsChanged(settings::type::FLATBUFSERVER, updateSettings);
}

bool BaseAPI::setEffect(const EffectCmdData& dat, hyperhdr::Components callerComp)
{
	int res = -1;

	if (dat.args.isEmpty())
	{
		SAFE_CALL_4_RET(_hyperhdr.get(), setEffect, int, res, QString, dat.effectName, int, dat.priority, int, dat.duration, QString, dat.origin);
	}

	return res >= 0;
}

void BaseAPI::setSourceAutoSelect(bool state, hyperhdr::Components callerComp)
{
	QUEUE_CALL_1(_hyperhdr.get(), setSourceAutoSelect, bool, state);
}

void BaseAPI::setVisiblePriority(int priority, hyperhdr::Components callerComp)
{
	QUEUE_CALL_1(_hyperhdr.get(), setVisiblePriority, int, priority);
}

void BaseAPI::registerInput(int priority, hyperhdr::Components component, const QString& origin, const QString& owner, hyperhdr::Components callerComp)
{
	if (_activeRegisters.count(priority))
		_activeRegisters.erase(priority);

	_activeRegisters.insert({ priority, registerData{component, origin, owner, callerComp} });
	
	QUEUE_CALL_4(_hyperhdr.get(), registerInput, int, priority, hyperhdr::Components, component, QString, origin, QString, owner);
}

void BaseAPI::unregisterInput(int priority)
{
	if (_activeRegisters.count(priority))
		_activeRegisters.erase(priority);
}

std::map<hyperhdr::Components, bool> BaseAPI::getAllComponents()
{
	std::map<hyperhdr::Components, bool> comps;
	//QMetaObject::invokeMethod(_hyperhdr, "getAllComponents", Qt::BlockingQueuedConnection, Q_RETURN_ARG(std::map<hyperhdr::Components, bool>, comps));
	return comps;
}

bool BaseAPI::isHyperhdrEnabled()
{
	int res = false;

	SAFE_CALL_1_RET(_hyperhdr.get(), isComponentEnabled, int, res, hyperhdr::Components, hyperhdr::COMP_ALL);

	return res > 0;
}

QVector<QVariantMap> BaseAPI::getAllInstanceData()
{
	QVector<QVariantMap> vec;

	SAFE_CALL_0_RET(_instanceManager.get(), getInstanceData, QVector<QVariantMap>, vec);

	return vec;
}

QJsonObject BaseAPI::getAverageColor(quint8 index)
{
	QJsonObject res;

	SAFE_CALL_1_RET(_instanceManager.get(), getAverageColor, QJsonObject, res, quint8, index);

	return res;
}

void BaseAPI::requestActiveRegister(QObject* callerInstance)
{
	// TODO FIXME
	//if (_activeRegisters.size())
	//   QMetaObject::invokeMethod(ApiSync::getInstance(), "answerActiveRegister", Qt::QueuedConnection, Q_ARG(QObject *, callerInstance), Q_ARG(MapRegister, _activeRegisters));
}

bool BaseAPI::saveSettings(const QJsonObject& data)
{
	bool rc = false;
	if (_adminAuthorized)
	{
		SAFE_CALL_2_RET(_hyperhdr.get(), saveSettings, bool, rc, QJsonObject, data, bool, true);
	}
	return rc;
}

QString BaseAPI::installLut(QNetworkReply* reply, QString fileName, int hardware_brightness, int hardware_contrast, int hardware_saturation, qint64 time)
{
#ifdef ENABLE_ZSTD
	QString error = nullptr;

	if (reply->error() == QNetworkReply::NetworkError::NoError)
	{
		QByteArray downloadedData = reply->readAll();

		error = DecompressZSTD(downloadedData.size(), reinterpret_cast<uint8_t*>(downloadedData.data()), QSTRING_CSTR(fileName));
	}
	else
		error = "Could not download LUT file";

	return error;
#else
	return "ZSTD support was disabled in the build configuration";
#endif
}

quint8 BaseAPI::getCurrentInstanceIndex()
{
	return _currentInstanceIndex;
}

bool BaseAPI::isAuthorized()
{
	return _authorized;
};

bool BaseAPI::isAdminAuthorized()
{
	return _adminAuthorized;
};

bool BaseAPI::updateHyperhdrPassword(const QString& password, const QString& newPassword)
{
	if (!_adminAuthorized)
		return false;
	bool res;
	SAFE_CALL_3_RET(_accessManager.get(), updateUserPassword, bool, res, QString, DEFAULT_CONFIG_USER, QString, password, QString, newPassword);
	return res;
}

QString BaseAPI::createToken(const QString& comment, AccessManager::AuthDefinition& def)
{
	if (!_adminAuthorized)
		return NO_AUTH;
	if (comment.isEmpty())
		return "comment is empty";
	SAFE_CALL_1_RET(_accessManager.get(), createToken, AccessManager::AuthDefinition, def, QString, comment);
	return "";
}

QString BaseAPI::renameToken(const QString& id, const QString& comment)
{
	if (!_adminAuthorized)
		return NO_AUTH;
	if (comment.isEmpty() || id.isEmpty())
		return "Empty comment or id";

	QUEUE_CALL_2(_accessManager.get(), renameToken, QString, id, QString, comment);
	return "";
}

QString BaseAPI::deleteToken(const QString& id)
{
	if (!_adminAuthorized)
		return NO_AUTH;
	if (id.isEmpty())
		return "Empty id";

	QUEUE_CALL_1(_accessManager.get(), deleteToken, QString, id);
	return "";
}

void BaseAPI::setNewTokenRequest(const QString& comment, const QString& id, const int& tan)
{
	QUEUE_CALL_4(_accessManager.get(), setNewTokenRequest, QObject*, this, QString, comment, QString, id, int, tan);
}

void BaseAPI::cancelNewTokenRequest(const QString& comment, const QString& id)
{
	QUEUE_CALL_3(_accessManager.get(), cancelNewTokenRequest, QObject*, this, QString, comment, QString, id);
}

bool BaseAPI::handlePendingTokenRequest(const QString& id, bool accept)
{
	if (!_adminAuthorized)
		return false;
	QUEUE_CALL_2(_accessManager.get(), handlePendingTokenRequest, QString, id, bool, accept);
	return true;
}

bool BaseAPI::getTokenList(QVector<AccessManager::AuthDefinition>& def)
{
	if (!_adminAuthorized)
		return false;
	SAFE_CALL_0_RET(_accessManager.get(), getTokenList, QVector<AccessManager::AuthDefinition>, def);
	return true;
}

bool BaseAPI::getPendingTokenRequests(QVector<AccessManager::AuthDefinition>& map)
{
	if (!_adminAuthorized)
		return false;
	SAFE_CALL_0_RET(_accessManager.get(), getPendingRequests, QVector<AccessManager::AuthDefinition>, map);
	return true;
}

bool BaseAPI::isUserTokenAuthorized(const QString& userToken)
{
	bool res;
	SAFE_CALL_2_RET(_accessManager.get(), isUserTokenAuthorized, bool, res, QString, DEFAULT_CONFIG_USER, QString, userToken);
	if (res)
	{
		_authorized = true;
		_adminAuthorized = true;
		// Listen for ADMIN ACCESS protected signals
		connect(_accessManager.get(), &AccessManager::newPendingTokenRequest, this, &BaseAPI::SignalPendingTokenClientNotification, Qt::UniqueConnection);
	}
	return res;
}

bool BaseAPI::getUserToken(QString& userToken)
{
	if (!_adminAuthorized)
		return false;
	SAFE_CALL_0_RET(_accessManager.get(), getUserToken, QString, userToken);
	return true;
}

bool BaseAPI::isTokenAuthorized(const QString& token)
{
	SAFE_CALL_1_RET(_accessManager.get(), isTokenAuthorized, bool, _authorized, QString, token);
	return _authorized;
}

bool BaseAPI::isUserAuthorized(const QString& password)
{
	bool res = false;
	SAFE_CALL_2_RET(_accessManager.get(), isUserAuthorized, bool, res, QString, DEFAULT_CONFIG_USER, QString, password);
	if (res)
	{
		_authorized = true;
		_adminAuthorized = true;
		// Listen for ADMIN ACCESS protected signals
		connect(_accessManager.get(), &AccessManager::newPendingTokenRequest, this, &BaseAPI::SignalPendingTokenClientNotification, Qt::UniqueConnection);
	}
	return res;
}

bool BaseAPI::isUserBlocked()
{
	bool res;
	SAFE_CALL_0_RET(_accessManager.get(), isUserAuthBlocked, bool, res);
	return res;
}

bool BaseAPI::hasHyperhdrDefaultPw()
{
	bool res = false;
	SAFE_CALL_2_RET(_accessManager.get(), isUserAuthorized, bool, res, QString, DEFAULT_CONFIG_USER, QString, DEFAULT_CONFIG_PASSWORD);
	return res;
}

void BaseAPI::logout()
{
	_authorized = false;
	_adminAuthorized = false;
	// Stop listenig for ADMIN ACCESS protected signals
	disconnect(_accessManager.get(), &AccessManager::newPendingTokenRequest, this, &BaseAPI::SignalPendingTokenClientNotification);
	stopDataConnections();
}

void BaseAPI::putSystemInfo(QJsonObject& system)
{	
	if (!_sysInfo.init)
	{
		_sysInfo.init = true;

		_sysInfo.kernelType = QSysInfo::kernelType();
		_sysInfo.kernelVersion = QSysInfo::kernelVersion();
		_sysInfo.architecture = QSysInfo::currentCpuArchitecture();
		_sysInfo.wordSize = QString::number(QSysInfo::WordSize);
		_sysInfo.productType = QSysInfo::productType();
		_sysInfo.productVersion = QSysInfo::productVersion();
		_sysInfo.prettyName = QSysInfo::prettyProductName();
		_sysInfo.hostName = QHostInfo::localHostName();
		_sysInfo.domainName = QHostInfo::localDomainName();

		#ifdef _WIN32
			QString cpucorp = "wmic cpu get name";
			QProcess windowscpu;
			windowscpu.startCommand(cpucorp);
			windowscpu.waitForFinished();
			QString result = windowscpu.readAllStandardOutput().trimmed();
			if (result.startsWith("Name", Qt::CaseInsensitive))
				result = result.right(result.size()-4).trimmed();
			_sysInfo.cpuModelName = result;
		#else
			QString cpuString;
			QFile file("/proc/cpuinfo");

			if (file.exists() && file.open(QFile::ReadOnly | QFile::Text))
			{
				QTextStream in(&file);

				while (in.readLineInto(&cpuString))
				{
					bool more = false;

					if (_sysInfo.cpuModelType.isEmpty())
					{
						QString match = "model\t";
						if (cpuString.startsWith(match, Qt::CaseInsensitive))
							_sysInfo.cpuModelType = cpuString.right(cpuString.length() - match.length() - 2).trimmed();
						else
							more = true;
					}

					if (_sysInfo.cpuModelName.isEmpty())
					{
						QString match = "model name\t";

						if (cpuString.startsWith(match, Qt::CaseInsensitive))
							_sysInfo.cpuModelName = cpuString.right(cpuString.length() - match.length() - 2).trimmed();
						else
							more = true;
					}

					if (_sysInfo.cpuHardware.isEmpty())
					{
						QString match = "hardware\t";

						if (cpuString.startsWith(match, Qt::CaseInsensitive))
							_sysInfo.cpuHardware = cpuString.right(cpuString.length() - match.length() - 2).trimmed();
						else
							more = true;
					}

					if (_sysInfo.cpuRevision.isEmpty())
					{
						QString match = "revision\t";

						if (cpuString.startsWith(match, Qt::CaseInsensitive))
							_sysInfo.cpuHardware = cpuString.right(cpuString.length() - match.length() - 2).trimmed();
						else
							more = true;
					}

					if (!more)
						break;
				}

				file.close();
			}
		#endif
	};

	system["kernelType"] = _sysInfo.kernelType;
	system["kernelVersion"] = _sysInfo.kernelVersion;
	system["architecture"] = _sysInfo.architecture;
	system["cpuModelName"] = _sysInfo.cpuModelName;
	system["cpuModelType"] = _sysInfo.cpuModelType;
	system["cpuHardware"] = _sysInfo.cpuHardware;
	system["cpuRevision"] = _sysInfo.cpuRevision;
	system["wordSize"] = _sysInfo.wordSize;
	system["productType"] = _sysInfo.productType;
	system["productVersion"] = _sysInfo.productVersion;
	system["prettyName"] = _sysInfo.prettyName;
	system["hostName"] = _sysInfo.hostName;
	system["domainName"] = _sysInfo.domainName;
	system["qtVersion"] = QT_VERSION_STR;
	system["openssl"] = (QSslSocket::supportsSsl()) ? QSslSocket::sslLibraryVersionString() : "unsupported";
}
