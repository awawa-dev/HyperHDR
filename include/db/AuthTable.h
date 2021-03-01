#pragma once

#include <db/DBManager.h>
#include <QCryptographicHash>

// qt
#include <QDateTime>
#include <QUuid>

///
/// @brief Authentication table interface
///
class AuthTable : public DBManager
{

public:
	/// construct wrapper with auth table
	AuthTable(const QString& rootPath = "", QObject* parent = nullptr, bool readonlyMode = false);

	///
	/// @brief      Create a user record, if called on a existing user the auth is recreated
	/// @param[in]  user           The username
	/// @param[in]  pw             The password
	/// @return     true on success else false
	///
	bool createUser(const QString& user, const QString& pw);

	///
	/// @brief      Test if user record exists
	/// @param[in]  user   The user id
	/// @return     true on success else false
	///
	bool userExist(const QString& user);

	///
	/// @brief Test if a user is authorized for access with given pw.
	/// @param user   The user name
	/// @param pw     The password
	/// @return       True on success else false
	///
	bool isUserAuthorized(const QString& user, const QString& pw);

	///
	/// @brief Test if a user token is authorized for access.
	/// @param usr   The user name
	/// @param token The token
	/// @return       True on success else false
	///
	bool isUserTokenAuthorized(const QString& usr, const QString& token);

	///
	/// @brief Update token of a user. It's an alternate login path which is replaced on startup. This token is NOT hashed(!)
	/// @param user   The user name
	/// @return       True on success else false
	///
	bool setUserToken(const QString& user);

	///
	/// @brief Get token of a user. This token is NOT hashed(!)
	/// @param user   The user name
	/// @return       The token
	///
	const QByteArray getUserToken(const QString& user);

	///
	/// @brief update password of given user. The user should be tested (isUserAuthorized) to verify this change
	/// @param user   The user name
	/// @param newPw  The new password to set
	/// @return       True on success else false
	///
	bool updateUserPassword(const QString& user, const QString& newPw);

	///
	/// @brief Reset password of HyperHDR user !DANGER! Used in HyperHDR main.cpp
	/// @return       True on success else false
	///
	bool resetHyperhdrUser();

	///
	/// @brief Update 'last_use' column entry for the corresponding user
	/// @param[in]  user   The user to search for
	///
	void updateUserUsed(const QString& user);

	///
	/// @brief      Test if token record exists, updates last_use on success
	/// @param[in]  token       The token id
	/// @return     true on success else false
	///
	bool tokenExist(const QString& token);	

	///
	/// @brief      Create a new token record with comment
	/// @param[in]  token   The token id as plaintext
	/// @param[in]  comment The comment for the token (eg a human readable identifier)
	/// @param[in]  id      The id for the token
	/// @return     true on success else false
	///
	bool createToken(const QString& token, const QString& comment, const QString& id);

	///
	/// @brief      Delete token record by id
	/// @param[in]  id    The token id
	/// @return     true on success else false
	///
	bool deleteToken(const QString& id);

	///
	/// @brief      Rename token record by id
	/// @param[in]  id    The token id
	/// @param[in]  comment The new comment
	/// @return     true on success else false
	///
	bool renameToken(const QString& id, const QString& comment);

	///
	/// @brief Get all 'comment', 'last_use' and 'id' column entries
	/// @return            A vector of all lists
	///
	const QVector<QVariantMap> getTokenList();

	///
	/// @brief      Test if id exists
	/// @param[in]  id      The id
	/// @return     true on success else false
	///
	bool idExist(const QString& id);

	///
	/// @brief Get the passwort hash of a user from db
	/// @param   user  The user name
	/// @return         password as hash
	///
	const QByteArray getPasswordHashOfUser(const QString& user);

	///
	/// @brief Calc the password hash of a user based on user name and password
	/// @param   user  The user name
	/// @param   pw    The password
	/// @return        The calced password hash
	///
	const QByteArray calcPasswordHashOfUser(const QString& user, const QString& pw);

	///
	/// @brief Create a password hash of plaintex password + salt
	/// @param  pw    The plaintext password
	/// @param  salt  The salt
	/// @return       The password hash with salt
	///
	const QByteArray hashPasswordWithSalt(const QString& pw, const QByteArray& salt);

	///
	/// @brief Create a token hash
	/// @param  token The plaintext token
	/// @return The token hash
	///
	const QByteArray hashToken(const QString& token);
};
