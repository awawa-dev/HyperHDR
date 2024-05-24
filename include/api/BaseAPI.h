#pragma once

#include <base/HyperHdrInstance.h>
#include <base/HyperHdrManager.h>
#include <base/AccessManager.h>
#include <base/GrabberHelper.h>

class QNetworkReply;
class SoundCapture;
class DiscoveryWrapper;

class BaseAPI : public QObject
{
	Q_OBJECT

public:
	BaseAPI(Logger* log, bool localConnection, QObject* parent);

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
		QString imagedata;
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
	typedef QMap<QString, AccessManager::AuthDefinition> MapAuthDefs;

protected:
	virtual void stopDataConnections() = 0;
	virtual void removeSubscriptions() = 0;
	virtual void addSubscriptions() = 0;

	void init();	

	bool clearPriority(int priority, QString& replyMsg, hyperhdr::Components callerComp = hyperhdr::COMP_INVALID);
	std::map<hyperhdr::Components, bool> getAllComponents();
	QVector<QVariantMap> getAllInstanceData();
	QJsonObject getAverageColor(quint8 index);
	quint8 getCurrentInstanceIndex();
	QString installLut(QNetworkReply* reply, QString fileName, int hardware_brightness, int hardware_contrast, int hardware_saturation, qint64 time);
	bool isHyperhdrEnabled();
	void registerInput(int priority, hyperhdr::Components component, const QString& origin, const QString& owner, hyperhdr::Components callerComp);
	void unregisterInput(int priority);

	void setColor(int priority, const std::vector<uint8_t>& ledColors, int timeout_ms = -1, const QString& origin = "API", hyperhdr::Components callerComp = hyperhdr::COMP_INVALID);
	bool setComponentState(const QString& comp, bool& compState, QString& replyMsg, hyperhdr::Components callerComp = hyperhdr::COMP_INVALID);
	bool setEffect(const EffectCmdData& dat, hyperhdr::Components callerComp = hyperhdr::COMP_INVALID);
	void setFlatbufferUserLUT(QString userLUTfile);
	bool setHyperhdrInstance(quint8 inst);
	bool setImage(ImageCmdData& data, hyperhdr::Components comp, QString& replyMsg, hyperhdr::Components callerComp = hyperhdr::COMP_INVALID);
	void setLedMappingType(int type, hyperhdr::Components callerComp = hyperhdr::COMP_INVALID);
	void setSourceAutoSelect(bool state, hyperhdr::Components callerComp = hyperhdr::COMP_INVALID);
	void setVideoModeHdr(int hdr, hyperhdr::Components callerComp = hyperhdr::COMP_INVALID);
	void setVisiblePriority(int priority, hyperhdr::Components callerComp = hyperhdr::COMP_INVALID);
	
	bool startInstance(quint8 index, int tan = 0);
	void stopInstance(quint8 index);

	//////////////////////////////////
	/// AUTH / ADMINISTRATION METHODS
	//////////////////////////////////

	bool deleteInstance(quint8 index, QString& replyMsg);
	QString createInstance(const QString& name);
	QString setInstanceName(quint8 index, const QString& name);
	bool saveSettings(const QJsonObject& data);
	bool isAuthorized();
	bool isAdminAuthorized();
	bool updateHyperhdrPassword(const QString& password, const QString& newPassword);
	QString createToken(const QString& comment, AccessManager::AuthDefinition& def);
	QString renameToken(const QString& id, const QString& comment);
	QString deleteToken(const QString& id);
	void setNewTokenRequest(const QString& comment, const QString& id, const int& tan);
	void cancelNewTokenRequest(const QString& comment, const QString& id);
	bool handlePendingTokenRequest(const QString& id, bool accept);
	bool getTokenList(QVector<AccessManager::AuthDefinition>& def);
	bool getPendingTokenRequests(QVector<AccessManager::AuthDefinition>& map);
	bool isUserTokenAuthorized(const QString& userToken);
	bool getUserToken(QString& userToken);
	bool isTokenAuthorized(const QString& token);
	bool isUserAuthorized(const QString& password);
	bool isUserBlocked();
	bool hasHyperhdrDefaultPw();
	void logout();
	void putSystemInfo(QJsonObject& system);
	
	bool _adminAuthorized;

	Logger* _log;
	std::shared_ptr<HyperHdrManager>	_instanceManager;
	std::shared_ptr<AccessManager>		_accessManager;	
	std::shared_ptr<HyperHdrInstance>	_hyperhdr;
	std::shared_ptr<SoundCapture> _soundCapture;
	std::shared_ptr<GrabberHelper> _videoGrabber;
	std::shared_ptr<GrabberHelper> _systemGrabber;
	std::shared_ptr<PerformanceCounters> _performanceCounters;
	std::shared_ptr<DiscoveryWrapper> _discoveryWrapper;

	struct {
		bool	init = false;
		QString cpuModelType;
		QString cpuModelName;
		QString cpuHardware;
		QString cpuRevision;

		QString kernelType;
		QString kernelVersion;
		QString architecture;
		QString wordSize;
		QString productType;
		QString productVersion;
		QString prettyName;
		QString hostName;
		QString domainName;
	} _sysInfo;

signals:	
	void SignalPendingTokenClientNotification(const QString& id, const QString& comment);
	void SignalPerformClientDisconnection();
	void SignalTokenClientNotification(bool success, const QString& token, const QString& comment, const QString& id, const int& tan);
	void SignalInstanceStartedClientNotification(const int& tan);

private slots:
	void requestActiveRegister(QObject* callerInstance);

private:
	bool _authorized;
	bool _localConnection;
	quint8 _currentInstanceIndex;

	std::map<int, registerData> _activeRegisters;	
};
