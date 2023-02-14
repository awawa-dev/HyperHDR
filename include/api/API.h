#pragma once

#include <base/HyperHdrInstance.h>
#include <base/HyperHdrIManager.h>
#include <base/AuthManager.h>

class QTimer;
class JsonCB;


const QString NO_AUTH = "No Authorization";

const QString DEFAULT_CONFIG_USER = "Hyperhdr";
const QString DEFAULT_CONFIG_PASSWORD = "hyperhdr";

///
/// @brief API for Hyperhdr to be inherted from a child class with specific protocol implementations
/// Workflow:
/// 1. create the class
/// 2. connect the forceClose signal, as the api might to close the connection for security reasons
/// 3. call Initialize()
/// 4. proceed as usual
///

class API : public QObject
{
	Q_OBJECT

public:		
	API(Logger* log, bool localConnection, QObject* parent);

	struct ImageCmdData
	{
		int priority;
		QString origin;
		int64_t duration;
		int width;
		int height;
		int scale;
		QString format;
		QString imgName;
		QByteArray data;
	};

	struct EffectCmdData
	{
		int priority;
		int duration;
		QString pythonScript;
		QString origin;
		QString effectName;
		QString data;
		QJsonObject args;
	};


	struct registerData
	{
		hyperhdr::Components component;
		QString origin;
		QString owner;
		hyperhdr::Components callerComp;
	};

	typedef std::map<int, registerData> MapRegister;
	typedef QMap<QString, AuthManager::AuthDefinition> MapAuthDefs;

protected:
	///
	/// @brief Initialize the API
	/// This call is REQUIRED!
	///
	void init();

	///
	/// @brief Set a single color
	/// @param[in] priority The priority of the written color
	/// @param[in] ledColor The color to write to the leds
	/// @param[in] timeout_ms The time the leds are set to the given color [ms]
	/// @param[in] origin   The setter
	///
	void setColor(int priority, const std::vector<uint8_t>& ledColors, int timeout_ms = -1, const QString& origin = "API", hyperhdr::Components callerComp = hyperhdr::COMP_INVALID);

	///
	/// @brief Set a image
	/// @param[in]  data      The command data
	/// @param[in]  comp      The component that should be used
	/// @param[out] replyMsg  The replyMsg on failure
	/// @param      callerComp The HYPERHDR COMPONENT that calls this function! e.g. PROT/FLATBUF
	/// @return True on success
	///
	bool setImage(ImageCmdData& data, hyperhdr::Components comp, QString& replyMsg, hyperhdr::Components callerComp = hyperhdr::COMP_INVALID);

	///
	/// @brief Clear a priority in the Muxer, if -1 all priorities are cleared
	/// @param priority   The priority to clear
	/// @param replyMsg   the message on failure
	/// @param callerComp The HYPERHDR COMPONENT that calls this function! e.g. PROT/FLATBUF
	/// @return  True on success
	///
	bool clearPriority(int priority, QString& replyMsg, hyperhdr::Components callerComp = hyperhdr::COMP_INVALID);

	///
	/// @brief Set a new component state
	/// @param comp       The component name
	/// @param compState  The new state of the comp
	/// @param replyMsg   The reply on failure
	/// @param callerComp The HYPERHDR COMPONENT that calls this function! e.g. PROT/FLATBUF
	/// @ return True on success
	///
	bool setComponentState(const QString& comp, bool& compState, QString& replyMsg, hyperhdr::Components callerComp = hyperhdr::COMP_INVALID);

	///
	/// @brief Set a ledToImageMapping type
	/// @param type       mapping type string
	/// @param callerComp The HYPERHDR COMPONENT that calls this function! e.g. PROT/FLATBUF
	///
	void setLedMappingType(int type, hyperhdr::Components callerComp = hyperhdr::COMP_INVALID);

	///
	/// @brief Set the 2D/3D modes type
	/// @param mode       The VideoMode
	/// @param callerComp The HYPERHDR COMPONENT that calls this function! e.g. PROT/FLATBUF
	///
	void setVideoModeHdr(int hdr, hyperhdr::Components callerComp = hyperhdr::COMP_INVALID);

	///
	/// @brief Set user LUT filename for flatbuffers tone mapping
	/// @param userLUTfile	user LUT filename
	///
	void setFlatbufferUserLUT(QString userLUTfile);

	///
	/// @brief Set an effect
	/// @param dat        The effect data
	/// @param callerComp The HYPERHDR COMPONENT that calls this function! e.g. PROT/FLATBUF
	/// REQUIRED dat fields: effectName, priority, duration, origin
	/// @return  True on success else false
	///
	bool setEffect(const EffectCmdData& dat, hyperhdr::Components callerComp = hyperhdr::COMP_INVALID);

	///
	/// @brief Set source auto select enabled or disabled
	/// @param sate       The new state
	/// @param callerComp The HYPERHDR COMPONENT that calls this function! e.g. PROT/FLATBUF
	///
	void setSourceAutoSelect(bool state, hyperhdr::Components callerComp = hyperhdr::COMP_INVALID);

	///
	/// @brief Set the visible priority to given priority
	/// @param priority   The priority to set
	/// @param callerComp The HYPERHDR COMPONENT that calls this function! e.g. PROT/FLATBUF
	///
	void setVisiblePriority(int priority, hyperhdr::Components callerComp = hyperhdr::COMP_INVALID);

	///
	/// @brief Register a input or update the meta data of a previous register call
	/// ATTENTION: Check unregisterInput() method description !!!
	/// @param[in] priority    The priority of the channel
	/// @param[in] component   The component of the channel
	/// @param[in] origin      Who set the channel (CustomString@IP)
	/// @param[in] owner       Specific owner string, might be empty
	/// @param[in] callerComp  The component that call this (e.g. PROTO/FLAT)
	///
	void registerInput(int priority, hyperhdr::Components component, const QString& origin, const QString& owner, hyperhdr::Components callerComp);

	///
	/// @brief Revoke a registerInput() call by priority. We maintain all registered priorities in this scope
	/// ATTENTION: This is MANDATORY if you change (priority change) or stop(clear/timeout) DURING lifetime. If this class destructs it's not needed
	/// @param priority  The priority to unregister
	///
	void unregisterInput(int priority);

	///
	/// @brief Handle the instance switching
	/// @param inst  The requested instance
	/// @return True on success else false
	///
	bool setHyperhdrInstance(quint8 inst);

	///
	/// @brief Get all contrable components and their state
	///
	std::map<hyperhdr::Components, bool> getAllComponents();

	///
	/// @brief Check if Hyperhdr ist enabled
	/// @return True when enabled else false
	///
	bool isHyperhdrEnabled();

	///
	/// @brief Get all instances data
	/// @return The instance data
	///
	QVector<QVariantMap> getAllInstanceData();

	///
	/// @brief Start instance
	/// @param index  The instance index
	/// @param tan    The tan
	/// @return  True on success else false
	///
	bool startInstance(quint8 index, int tan = 0);

	///
	/// @brief Stop instance
	/// @param index  The instance index
	///
	void stopInstance(quint8 index);

	//////////////////////////////////
	/// AUTH / ADMINISTRATION METHODS
	//////////////////////////////////

	///
	/// @brief Delete instance. Requires ADMIN ACCESS
	/// @param index  The instance index
	/// @param replyMsg The reply Msg
	/// @return False with reply
	///
	bool deleteInstance(quint8 index, QString& replyMsg);

	///
	/// @brief Create instance. Requires ADMIN ACCESS
	/// @param name  With given name
	/// @return False with reply
	///
	QString createInstance(const QString& name);

	///
	/// @brief Rename an instance. Requires ADMIN ACCESS
	/// @param index The instance index
	/// @param name  With given name
	/// @return False with reply
	///
	QString setInstanceName(quint8 index, const QString& name);

	///
	/// @brief Delete an effect. Requires ADMIN ACCESS
	/// @param name The effect name
	/// @return  True on success else false
	///
	QString deleteEffect(const QString& name);

	///
	/// @brief Delete an effect. Requires ADMIN ACCESS
	/// @param name The effect name
	/// @return  True on success else false
	///
	QString saveEffect(const QJsonObject& data);

	///
	/// @brief Save settings object. Requires ADMIN ACCESS
	/// @param data  The data object
	///
	bool saveSettings(const QJsonObject& data);

	///
	/// @brief Test if we are authorized to use the interface
	/// @return The result
	///
	bool isAuthorized();

	///
	/// @brief Test if we are authorized to use the admin interface
	/// @return The result
	///
	bool isAdminAuthorized();

	///
	/// @brief Update the Password of Hyperhdr. Requires ADMIN ACCESS
	/// @param password    Old password
	/// @param newPassword New password
	/// @return True on success else false
	///
	bool updateHyperhdrPassword(const QString& password, const QString& newPassword);

	///
	/// @brief Get a new token from AuthManager. Requires ADMIN ACCESS
	/// @param comment   The comment of the request
	/// @param def       The final definition
	/// @return Empty string on success else error message
	///
	QString createToken(const QString& comment, AuthManager::AuthDefinition& def);

	///
	/// @brief Rename a token by given id. Requires ADMIN ACCESS
	/// @param id  The id of the token
	/// @param comment The new comment
	/// @return Empty string on success else error message
	///
	QString renameToken(const QString& id, const QString& comment);

	///
	/// @brief Delete a token by given id. Requires ADMIN ACCESS
	/// @param id  The id of the token
	/// @return Empty string on success else error message
	///
	QString deleteToken(const QString& id);

	///
	/// @brief Set a new token request
	/// @param comment  The comment
	/// @param id       The id
	/// @param tan      The tan
	///
	void setNewTokenRequest(const QString& comment, const QString& id, const int& tan);

	///
	/// @brief Cancel new token request
	/// @param comment  The comment
	/// @param id       The id
	///
	void cancelNewTokenRequest(const QString& comment, const QString& id);

	///
	/// @brief Handle a pending token request. Requires ADMIN ACCESS
	/// @param id     The id fo the request
	/// @param accept True when it should be accepted, else false
	/// @return True on success
	bool handlePendingTokenRequest(const QString& id, bool accept);

	///
	/// @brief Get the current List of Tokens. Requires ADMIN ACCESS
	/// @param def returns the defintions
	/// @return True on success
	///
	bool getTokenList(QVector<AuthManager::AuthDefinition>& def);

	///
	/// @brief Get all current pending token requests. Requires ADMIN ACCESS
	/// @return True on success
	///
	bool getPendingTokenRequests(QVector<AuthManager::AuthDefinition>& map);

	///
	/// @brief Is User Token Authorized. On success this will grant acces to API and ADMIN API
	/// @param userToken   The user Token
	/// @return True on succes
	///
	bool isUserTokenAuthorized(const QString& userToken);

	///
	/// @brief Get the current User Token (session token). Requires ADMIN ACCESS
	/// @param userToken   The user Token
	/// @return True on success
	///
	bool getUserToken(QString& userToken);

	///
	/// @brief Is a token authrized. On success this will grant acces to the API (NOT ADMIN API)
	/// @param token   The user Token
	/// @return True on succes
	///
	bool isTokenAuthorized(const QString& token);

	///
	/// @brief Is User authorized. On success this will grant acces to the API and ADMIN API
	/// @param password  The password of the User
	/// @return True if authorized
	///
	bool isUserAuthorized(const QString& password);

	///
	/// @brief Test if Hyperhdr has the default PW
	/// @return The result
	///
	bool hasHyperhdrDefaultPw();

	///
	/// @brief Logout revokes all authorizations
	///
	void logout();

	/// Reflect auth status of this client
	bool _authorized;
	bool _adminAuthorized;

	/// Is this a local connection
	bool _localConnection;

	AuthManager* _authManager;
	HyperHdrIManager* _instanceManager;

	Logger* _log;
	HyperHdrInstance* _hyperhdr;

signals:
	///
	/// @brief The API might decide to block connections for security reasons, this emitter should close the socket
	///
	void forceClose();

	///
	/// @brief Emits whenever a new Token request is pending. This signal is just active when ADMIN ACCESS has been granted
	/// @param id      The id of the request
	/// @param comment The comment of the request; If the commen is EMPTY the request has been revoked by the caller. So remove it from the pending list
	///
	void onPendingTokenRequest(const QString& id, const QString& comment);

	///
	/// @brief Handle emits from AuthManager of accepted/denied/timeouts token request, just if QObject matches with this instance it will emit.
	/// @param  success If true the request was accepted else false and no token was created
	/// @param  token   The new token that is now valid
	/// @param  comment The comment that was part of the request
	/// @param  id      The id that was part of the request
	/// @param  tan     The tan that was part of the request
	///
	void onTokenResponse(bool success, const QString& token, const QString& comment, const QString& id, const int& tan);

	///
	/// @brief Handle emits from HyperhdrIManager of startInstance request, just if QObject matches with this instance it will emit.
	/// @param  tan     The tan that was part of the request
	///
	void onStartInstanceResponse(const int& tan);

private slots:
	///
	/// @brief Is called whenever a Hyperhdr instance wants the current register list
	/// @param callerInstance  The instance should be returned in the answer call
	///
	void requestActiveRegister(QObject* callerInstance);

private:
	void stopDataConnectionss();

	// Contains all active register call data
	std::map<int, registerData> _activeRegisters;

	// current instance index
	quint8 _currInstanceIndex;
};
