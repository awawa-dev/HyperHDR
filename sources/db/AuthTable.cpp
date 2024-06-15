#include <QCryptographicHash>

#include <db/AuthTable.h>
using namespace hyperhdr;

AuthTable::AuthTable()
	: DBManager()
{
	// init Auth table
	setTable("auth");
	// create table columns
	createTable(QStringList() << "user TEXT" << "password BLOB" << "token BLOB" << "salt BLOB" << "comment TEXT" << "portal_token TEXT" << "id TEXT" << "created_at TEXT" << "last_use TEXT");
};

bool AuthTable::createUser(const QString& user, const QString& pw)
{
	// new salt
	QByteArray salt = QCryptographicHash::hash(QUuid::createUuid().toByteArray(), QCryptographicHash::Sha512).toHex();
	QVariantMap map;
	map["user"] = user;
	map["salt"] = salt;
	map["password"] = hashPasswordWithSalt(pw, salt);
	map["created_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

	VectorPair cond;
	cond.append(CPair("user", user));
	return createRecord(cond, map);
}

void AuthTable::savePipewire(const QString& wToken)
{
	QVariantMap map;
	map["portal_token"] = wToken;

	VectorPair cond;
	updateRecord(cond, map);
}


bool AuthTable::userExist(const QString& user)
{
	VectorPair cond;
	cond.append(CPair("user", user));
	return recordExists(cond);
}


bool AuthTable::isUserAuthorized(const QString& user, const QString& pw)
{
	if (userExist(user) && (calcPasswordHashOfUser(user, pw) == getPasswordHashOfUser(user)))
	{
		updateUserUsed(user);
		return true;
	}
	return false;
}

const QString AuthTable::loadPipewire()
{
	QVariantMap results;
	VectorPair cond;

	getRecord(cond, results, QStringList() << "portal_token");

	return results["portal_token"].toString();
}

bool AuthTable::isUserTokenAuthorized(const QString& usr, const QString& token)
{
	if (getUserToken(usr) == token.toUtf8())
	{
		updateUserUsed(usr);
		return true;
	}
	return false;
}


bool AuthTable::setUserToken(const QString& user)
{
	QVariantMap map;
	map["token"] = QCryptographicHash::hash(QUuid::createUuid().toByteArray(), QCryptographicHash::Sha512).toHex();

	VectorPair cond;
	cond.append(CPair("user", user));
	return updateRecord(cond, map);
}


const QByteArray AuthTable::getUserToken(const QString& user)
{
	QVariantMap results;
	VectorPair cond;
	cond.append(CPair("user", user));
	getRecord(cond, results, QStringList() << "token");

	return results["token"].toByteArray();
}


bool AuthTable::updateUserPassword(const QString& user, const QString& newPw)
{
	QVariantMap map;
	map["password"] = calcPasswordHashOfUser(user, newPw);

	VectorPair cond;
	cond.append(CPair("user", user));
	return updateRecord(cond, map);
}


bool AuthTable::resetHyperhdrUser()
{
	QVariantMap map;
	map["password"] = calcPasswordHashOfUser(DEFAULT_CONFIG_USER, DEFAULT_CONFIG_PASSWORD);

	VectorPair cond;
	cond.append(CPair("user", DEFAULT_CONFIG_USER));
	return updateRecord(cond, map);
}


void AuthTable::updateUserUsed(const QString& user)
{
	QVariantMap map;
	map["last_use"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

	VectorPair cond;
	cond.append(CPair("user", user));
	updateRecord(cond, map);
}


bool AuthTable::tokenExist(const QString& token)
{
	QVariantMap map;
	map["last_use"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

	VectorPair cond;
	cond.append(CPair("token", hashToken(token)));
	if (recordExists(cond))
	{
		// update it
		createRecord(cond, map);
		return true;
	}
	return false;
}


bool AuthTable::createToken(const QString& token, const QString& comment, const QString& id)
{
	QVariantMap map;
	map["comment"] = comment;
	map["id"] = idExist(id) ? QUuid::createUuid().toString().remove("{").remove("}").left(5) : id;
	map["created_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

	VectorPair cond;
	cond.append(CPair("token", hashToken(token)));
	return createRecord(cond, map);
}


bool AuthTable::deleteToken(const QString& id)
{
	VectorPair cond;
	cond.append(CPair("id", id));
	return deleteRecord(cond);
}


bool AuthTable::renameToken(const QString& id, const QString& comment)
{
	QVariantMap map;
	map["comment"] = comment;

	VectorPair cond;
	cond.append(CPair("id", id));
	return updateRecord(cond, map);
}


const QVector<QVariantMap> AuthTable::getTokenList()
{
	QVector<QVariantMap> results;
	getRecords(results, QStringList() << "comment" << "id" << "last_use");

	return results;
}


bool AuthTable::idExist(const QString& id)
{

	VectorPair cond;
	cond.append(CPair("id", id));
	return recordExists(cond);
}


const QByteArray AuthTable::getPasswordHashOfUser(const QString& user)
{
	QVariantMap results;
	VectorPair cond;
	cond.append(CPair("user", user));
	getRecord(cond, results, QStringList() << "password");

	return results["password"].toByteArray();
}


const QByteArray AuthTable::calcPasswordHashOfUser(const QString& user, const QString& pw)
{
	// get salt
	QVariantMap results;
	VectorPair cond;
	cond.append(CPair("user", user));
	getRecord(cond, results, QStringList() << "salt");

	// calc
	return hashPasswordWithSalt(pw, results["salt"].toByteArray());
}


const QByteArray AuthTable::hashPasswordWithSalt(const QString& pw, const QByteArray& salt)
{
	return QCryptographicHash::hash(pw.toUtf8().append(salt), QCryptographicHash::Sha512).toHex();
}


const QByteArray AuthTable::hashToken(const QString& token)
{
	return QCryptographicHash::hash(token.toUtf8(), QCryptographicHash::Sha512).toHex();
}

