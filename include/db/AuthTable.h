#pragma once

#include <db/DBManager.h>

namespace hyperhdr {
	const QString NO_AUTH = "No Authorization";
	const QString DEFAULT_CONFIG_USER = "Hyperhdr";
	const QString DEFAULT_CONFIG_PASSWORD = "hyperhdr";
}

class AuthTable : public DBManager
{

public:
	AuthTable();
	bool createUser(const QString& user, const QString& pw);
	void savePipewire(const QString& wToken);
	bool userExist(const QString& user);
	bool isUserAuthorized(const QString& user, const QString& pw);
	const QString loadPipewire();
	bool isUserTokenAuthorized(const QString& usr, const QString& token);
	bool setUserToken(const QString& user);
	const QByteArray getUserToken(const QString& user);
	bool updateUserPassword(const QString& user, const QString& newPw);
	bool resetHyperhdrUser();
	void updateUserUsed(const QString& user);
	bool tokenExist(const QString& token);
	bool createToken(const QString& token, const QString& comment, const QString& id);
	bool deleteToken(const QString& id);
	bool renameToken(const QString& id, const QString& comment);
	const QVector<QVariantMap> getTokenList();
	bool idExist(const QString& id);
	const QByteArray getPasswordHashOfUser(const QString& user);
	const QByteArray calcPasswordHashOfUser(const QString& user, const QString& pw);
	const QByteArray hashPasswordWithSalt(const QString& pw, const QByteArray& salt);
	const QByteArray hashToken(const QString& token);
};
