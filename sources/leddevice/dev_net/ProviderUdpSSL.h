#pragma once

#ifndef PCH_ENABLED
	#include <QThread>
	#include <QHostAddress>

	#include <string.h>
	#include <cstring>
	#include <chrono>
#endif


#include <leddevice/LedDevice.h>
#include <utils/Logger.h>

struct mbedtls_net_context;
struct mbedtls_entropy_context;
struct mbedtls_ssl_context;
struct mbedtls_ssl_config;
struct mbedtls_x509_crt;
struct mbedtls_ctr_drbg_context;
struct mbedtls_timing_delay_context;


class ProviderUdpSSL : public LedDevice
{
	Q_OBJECT

public:
	ProviderUdpSSL(const QJsonObject& deviceConfig);
	virtual ~ProviderUdpSSL();

protected:
	int get_MBEDTLS_TLS_PSK_WITH_AES_128_GCM_SHA256() const;
	bool init(const QJsonObject& deviceConfig) override;
	int closeNetwork();
	bool initNetwork();
	void writeBytes(unsigned int size, const uint8_t* data, bool flush = false);
	virtual const int* getCiphersuites() const;

private:

	bool initConnection();
	void closeConnection();
	bool createEntropy();
	bool setupStructure();
	bool startUPDConnection();
	bool setupPSK();
	bool startSSLHandshake();
	QString errorMsg(int ret);
	void closeSSLNotify();
	void freeSSLConnection();

	mbedtls_net_context*          client_fd;
	mbedtls_entropy_context*      entropy;
	mbedtls_ssl_context*          ssl;
	mbedtls_ssl_config*           conf;
	mbedtls_x509_crt*             cacert;
	mbedtls_ctr_drbg_context*     ctr_drbg;
	mbedtls_timing_delay_context* timer;

	QString      _transport_type;
	QString      _custom;
	QHostAddress _address;
	QString      _defaultHost;
	int          _port;
	int          _ssl_port;
	QString      _server_name;
	QString      _psk;
	QString      _psk_identity;
	unsigned int _handshake_attempts;
	int          _retry_left;
	bool         _streamReady;
	bool         _streamPaused;
	uint32_t     _handshake_timeout_min;
	uint32_t     _handshake_timeout_max;
};
