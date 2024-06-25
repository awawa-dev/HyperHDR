#pragma once

#ifndef PCH_ENABLED
	#include <QMap>
	#include <QVector>
#endif

#include <utils/Logger.h>
#include <utils/settings.h>

class AuthTable;
class MetaTable;
class QTimer;

class AccessManager : public QObject
{
	Q_OBJECT
private:
	friend class HyperHdrDaemon;
	AccessManager(QObject* parent);

public:
	~AccessManager();

	struct AuthDefinition
	{
		QString id;
		QString comment;
		QObject* caller;
		int      tan;
		uint64_t timeoutTime;
		QString token;
		QString lastUse;
	};

	QString getID() const;
	bool isAuthRequired() const;
	bool isLocalAuthRequired() const;
	bool isLocalAdminAuthRequired() const;
	bool resetHyperhdrUser();
	bool isTokenAuthBlocked() const;

public slots:

	bool isUserAuthorized(const QString& user, const QString& pw);
	bool isUserAuthBlocked() const;
	bool isTokenAuthorized(const QString& token);
	bool isUserTokenAuthorized(const QString& usr, const QString& token);

	AccessManager::AuthDefinition createToken(const QString& comment);
	bool renameToken(const QString& id, const QString& comment);
	bool deleteToken(const QString& id);
	bool updateUserPassword(const QString& user, const QString& pw, const QString& newPw);
	void setNewTokenRequest(QObject* caller, const QString& comment, const QString& id, const int& tan = 0);
	void cancelNewTokenRequest(QObject* caller, const QString&, const QString& id);

	void handlePendingTokenRequest(const QString& id, bool accept);

	QVector<AccessManager::AuthDefinition> getPendingRequests() const;
	QString getUserToken(const QString& usr = "Hyperhdr") const;
	QVector<AccessManager::AuthDefinition> getTokenList() const;
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);
	void savePipewire(const QString& wToken);
	const QString loadPipewire();

signals:
	void newPendingTokenRequest(const QString& id, const QString& comment);	
	void SignalTokenUpdated(QVector<AccessManager::AuthDefinition>);
	void SignalTokenNotifyClient(bool success, QObject* caller, const QString& token, const QString& comment, const QString& id, const int& tan);

private:
	void setAuthBlock(bool user = false);

	std::unique_ptr<AuthTable> _authTable;
	std::unique_ptr<MetaTable> _metaTable;
	QString _uuid;
	QMap<QString, AuthDefinition> _pendingRequests;
	bool _authRequired;
	bool _localAuthRequired;
	bool _localAdminAuthRequired;
	QTimer* _timer;
	QTimer* _authBlockTimer;
	QVector<uint64_t> _userAuthAttempts;
	QVector<uint64_t> _tokenAuthAttempts;

private slots:
	void checkTimeout();
	void checkAuthBlockTimeout();
};
