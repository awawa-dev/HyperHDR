#pragma once

// util
#include <utils/Logger.h>
#include <utils/settings.h>
#include <utils/PixelFormat.h>

// qt
#include <QVector>

class BonjourServiceRegister;
class QTcpServer;
class FlatBufferClient;
class NetOrigin;


///
/// @brief A TcpServer to receive images of different formats with Google Flatbuffer
/// Images will be forwarded to all HyperHdr instanceshttp://localhost:8090
///
class FlatBufferServer : public QObject
{
	Q_OBJECT
public:
	FlatBufferServer(const QJsonDocument& config, const QString& configurationPath, QObject* parent = nullptr);
	~FlatBufferServer() override;

	static FlatBufferServer* instance;
	static FlatBufferServer* getInstance(){ return instance; }

signals:
	void hdrToneMappingChanged(bool enabled, uint8_t* lutBuffer);

public slots:
	///
	/// @brief Handle settings update
	/// @param type   The type from enum
	/// @param config The configuration
	///
	void handleSettingsUpdate(settings::type type, const QJsonDocument& config);

	void initServer();

	void setHdrToneMappingEnabled(bool enabled);

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


private:
	QTcpServer* _server;
	NetOrigin* _netOrigin;
	Logger* _log;
	int _timeout;
	quint16 _port;
	const QJsonDocument _config;
	BonjourServiceRegister* _serviceRegister = nullptr;

	QVector<FlatBufferClient*> _openConnections;

	// tone mapping
	bool _hdrToneMappingEnabled;
	uint8_t* _lutBuffer;
	bool _lutBufferInit;
	QString	_configurationPath;
};
