#ifndef PCH_ENABLED
	#include <QJsonObject>
	#include <QTimer>
#endif

#include <base/AccessManager.h>
#include <db/AuthTable.h>
#include <db/MetaTable.h>

using namespace hyperhdr;

AccessManager::AccessManager(QObject* parent)
	: QObject(parent)
	, _authTable(new AuthTable())
	, _metaTable(new MetaTable())
	, _pendingRequests()
	, _authRequired(true)
	, _timer(new QTimer(this))
	, _authBlockTimer(new QTimer(this))
{
	// get uuid
	_uuid = _metaTable->getUUID();

	// Register meta
	qRegisterMetaType<QVector<AccessManager::AuthDefinition>>("QVector<AccessManager::AuthDefinition>");

	// setup timer
	_timer->setInterval(1000);
	connect(_timer, &QTimer::timeout, this, &AccessManager::checkTimeout);

	// setup authBlockTimer
	_authBlockTimer->setInterval(60000);
	connect(_authBlockTimer, &QTimer::timeout, this, &AccessManager::checkAuthBlockTimeout);

	// init with default user and password
	if (!_authTable->userExist(DEFAULT_CONFIG_USER))
	{
		_authTable->createUser(DEFAULT_CONFIG_USER, DEFAULT_CONFIG_PASSWORD);
	}

	// update HyperHDR user token on startup
	_authTable->setUserToken(DEFAULT_CONFIG_USER);
}

AccessManager::~AccessManager()
{
}

const QString AccessManager::loadPipewire()
{
	return _authTable->loadPipewire();
}

AccessManager::AuthDefinition AccessManager::createToken(const QString& comment)
{
	const QString token = QUuid::createUuid().toString().mid(1, 36);
	const QString id = QUuid::createUuid().toString().mid(1, 36).left(5);

	_authTable->createToken(token, comment, id);

	AuthDefinition def;
	def.comment = comment;
	def.token = token;
	def.id = id;

	emit SignalTokenUpdated(getTokenList());
	return def;
}

void AccessManager::savePipewire(const QString& wToken)
{
	_authTable->savePipewire(wToken);
}

QVector<AccessManager::AuthDefinition> AccessManager::getTokenList() const
{
	QVector<QVariantMap> vector = _authTable->getTokenList();
	QVector<AccessManager::AuthDefinition> finalVec;
	for (const auto& entry : vector)
	{
		AuthDefinition def;
		def.comment = entry["comment"].toString();
		def.id = entry["id"].toString();
		def.lastUse = entry["last_use"].toString();

		// don't add empty ids
		if (!entry["id"].toString().isEmpty())
			finalVec.append(def);
	}
	return finalVec;
}

QString AccessManager::getUserToken(const QString& usr) const
{
	QString tok = _authTable->getUserToken(usr);
	return QString(_authTable->getUserToken(usr));
}

void AccessManager::setAuthBlock(bool user)
{
	// current timestamp +10 minutes
	if (user)
		_userAuthAttempts.append(InternalClock::now() + 600000);
	else
		_tokenAuthAttempts.append(InternalClock::now() + 600000);

	_authBlockTimer->start();
}

bool AccessManager::isUserAuthorized(const QString& user, const QString& pw)
{
	if (isUserAuthBlocked())
		return false;

	if (!_authTable->isUserAuthorized(user, pw))
	{
		setAuthBlock(true);
		return false;
	}
	return true;
}

bool AccessManager::isTokenAuthorized(const QString& token)
{
	if (isTokenAuthBlocked())
		return false;

	if (!_authTable->tokenExist(token))
	{
		setAuthBlock();
		return false;
	}
	// timestamp update
	SignalTokenUpdated(getTokenList());
	return true;
}

bool AccessManager::isUserTokenAuthorized(const QString& usr, const QString& token)
{
	if (isUserAuthBlocked())
		return false;

	if (!_authTable->isUserTokenAuthorized(usr, token))
	{
		setAuthBlock(true);
		return false;
	}
	return true;
}

bool AccessManager::updateUserPassword(const QString& user, const QString& pw, const QString& newPw)
{
	if (isUserAuthorized(user, pw))
		return _authTable->updateUserPassword(user, newPw);

	return false;
}

bool AccessManager::resetHyperhdrUser()
{
	return _authTable->resetHyperhdrUser();
}

void AccessManager::setNewTokenRequest(QObject* caller, const QString& comment, const QString& id, const int& tan)
{
	if (!_pendingRequests.contains(id))
	{
		AuthDefinition newDef{ id, comment, caller, tan, uint64_t(InternalClock::now() + 180000) };
		_pendingRequests[id] = newDef;
		_timer->start();
		emit newPendingTokenRequest(id, comment);
	}
}

void AccessManager::cancelNewTokenRequest(QObject* caller, const QString&, const QString& id)
{
	if (_pendingRequests.contains(id))
	{
		AuthDefinition def = _pendingRequests.value(id);
		if (def.caller == caller)
			_pendingRequests.remove(id);
		emit newPendingTokenRequest(id, "");
	}
}

void AccessManager::handlePendingTokenRequest(const QString& id, bool accept)
{
	if (_pendingRequests.contains(id))
	{
		AuthDefinition def = _pendingRequests.take(id);

		if (accept)
		{
			const QString token = QUuid::createUuid().toString().remove("{").remove("}");
			_authTable->createToken(token, def.comment, id);
			emit SignalTokenNotifyClient(true, def.caller, token, def.comment, id, def.tan);
			emit SignalTokenUpdated(getTokenList());
		}
		else
		{
			emit SignalTokenNotifyClient(false, def.caller, QString(), def.comment, id, def.tan);
		}
	}
}

QVector<AccessManager::AuthDefinition> AccessManager::getPendingRequests() const
{
	QVector<AccessManager::AuthDefinition> finalVec;
	for (const auto& entry : _pendingRequests)
	{
		AuthDefinition def;
		def.comment = entry.comment;
		def.id = entry.id;
		def.timeoutTime = entry.timeoutTime - InternalClock::now();
		finalVec.append(def);
	}
	return finalVec;
}

bool AccessManager::renameToken(const QString& id, const QString& comment)
{
	if (_authTable->renameToken(id, comment))
	{
		emit SignalTokenUpdated(getTokenList());
		return true;
	}
	return false;
}

bool AccessManager::deleteToken(const QString& id)
{
	if (_authTable->deleteToken(id))
	{
		emit SignalTokenUpdated(getTokenList());
		return true;
	}
	return false;
}

void AccessManager::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if (type == settings::type::NETWORK)
	{
		const QJsonObject& obj = config.object();
		_authRequired = obj["apiAuth"].toBool(true);
		_localAuthRequired = obj["localApiAuth"].toBool(false);
		_localAdminAuthRequired = obj["localAdminAuth"].toBool(true);
	}
}

void AccessManager::checkTimeout()
{
	const uint64_t now = InternalClock::now();

	QMapIterator<QString, AuthDefinition> i(_pendingRequests);
	while (i.hasNext())
	{
		i.next();

		const AuthDefinition& def = i.value();
		if (def.timeoutTime <= now)
		{
			emit SignalTokenNotifyClient(false, def.caller, QString(), def.comment, def.id, def.tan);
			_pendingRequests.remove(i.key());
		}
	}
	// abort if empty
	if (_pendingRequests.isEmpty())
		_timer->stop();
}

void AccessManager::checkAuthBlockTimeout()
{
	// handle user auth block	
	QVector<uint64_t>::iterator itu = _userAuthAttempts.begin();
	while (itu != _userAuthAttempts.end()) {
		if (*itu < (uint64_t)InternalClock::now())
			itu = _userAuthAttempts.erase(itu);
		else
			++itu;
	}

	// handle token auth block	
	QVector<uint64_t>::iterator it = _tokenAuthAttempts.begin();
	while (it != _tokenAuthAttempts.end()) {
		if (*it < (uint64_t)InternalClock::now())
			it = _tokenAuthAttempts.erase(it);
		else
			++it;
	}

	// if the lists are empty we stop
	if (_userAuthAttempts.empty() && _tokenAuthAttempts.empty())
		_authBlockTimer->stop();
}

QString AccessManager::getID() const
{
	return _uuid;
}

bool AccessManager::isAuthRequired() const
{
	return _authRequired;
}

bool AccessManager::isLocalAuthRequired() const
{
	return _localAuthRequired;
}

bool AccessManager::isLocalAdminAuthRequired() const
{
	return _localAdminAuthRequired;
}

bool AccessManager::isUserAuthBlocked() const
{
	return (_userAuthAttempts.length() >= 10);
}

bool AccessManager::isTokenAuthBlocked() const
{
	return (_tokenAuthAttempts.length() >= 25);
}
