#pragma once

// util
#include <utils/Logger.h>
#include <utils/settings.h>
#include <utils/Image.h>

// qt
#include <QVector>

class BonjourServiceRegister;
class QTcpServer;
class QLocalServer;
class FlatBufferClient;
class NetOrigin;

#define HYPERHDR_DOMAIN_SERVER QStringLiteral("hyperhdr-domain")

///
/// @brief A TcpServer to receive images of different formats with Google Flatbuffer
/// Images will be forwarded to all HyperHdr instances
///
class FlatBufferServer : public QObject
{
	Q_OBJECT
public:
	FlatBufferServer(const QJsonDocument& config, const QString& configurationPath, QObject* parent = nullptr);
	~FlatBufferServer() override;

	static FlatBufferServer* instance;
	static FlatBufferServer* getInstance() { return instance; }

signals:
	void hdrToneMappingChanged(int mode, uint8_t* lutBuffer);
	void HdrChanged(int mode);

public slots:
	///
	/// @brief Handle settings update
	/// @param type   The type from enum
	/// @param config The configuration
	///
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);

	void initServer();

	void setHdrToneMappingEnabled(int mode);

	int getHdrToneMappingEnabled();

	void importFromProtoHandler(int priority, int duration, const Image<ColorRgb>& image);

private slots:
	///
	/// @brief Is called whenever a new socket wants to connect
	///
	void newConnection();

	///
	/// @brief is called whenever a client disconnected
	///
	void clientDisconnected();

private:
	///
	/// @brief Start the server with current _port
	///
	void startServer();

	///
	/// @brief Stop server
	///
	void stopServer();


	///
	/// @brief Get shared LUT file folder
	///
	QString GetSharedLut();

	///
	/// @brief Load LUT file
	///
	void loadLutFile();

	void setupClient(FlatBufferClient* client);


private:
	QTcpServer*		_server;
	QLocalServer*	_domain;
	NetOrigin*		_netOrigin;
	Logger*			_log;
	int				_timeout;
	quint16			_port;

	const QJsonDocument		_config;
	BonjourServiceRegister* _serviceRegister = nullptr;

	QVector<FlatBufferClient*> _openConnections;

	// tone mapping
	int			_hdrToneMappingMode;
	int			_realHdrToneMappingMode;
	uint8_t*	_lutBuffer;
	bool		_lutBufferInit;
	QString		_configurationPath;
};
