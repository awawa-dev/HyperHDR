
// STL includes
#include <cstdio>
#include <exception>
#include <algorithm>

// Linux includes
#include <fcntl.h>
#ifndef _WIN32
	#include <sys/ioctl.h>
#endif

// Local HyperHDR includes
#include "ProviderUdpSSL.h"

const int MAX_RETRY = 5;
const ushort MAX_PORT_SSL = 65535;

ProviderUdpSSL::ProviderUdpSSL(const QJsonObject &deviceConfig)
	: LedDevice(deviceConfig)
	, client_fd()
	, entropy()
	, ssl()
	, conf()
	, ctr_drbg()
	, timer()
	, _transport_type("DTLS")
	, _custom("dtls_client")
	, _address("127.0.0.1")
	, _defaultHost("127.0.0.1")
	, _port(1)
	, _ssl_port(1)
	, _server_name()
	, _psk()
	, _psk_identity()
	, _handshake_attempts(5)
	, _retry_left(MAX_RETRY)
	, _streamReady(false)
	, _streamPaused(false)
	, _handshake_timeout_min(300)
	, _handshake_timeout_max(1000)
{
	_latchTime_ms = 1;

	bool error = false;

	try
	{
		mbedtls_ctr_drbg_init(&ctr_drbg);
		error = !seedingRNG();
	}
	catch (...)
	{
		error = true;
	}

	if (error)
		Error(_log, "Failed to initialize mbedtls seed");
}

ProviderUdpSSL::~ProviderUdpSSL()
{
	mbedtls_ctr_drbg_free(&ctr_drbg);
	mbedtls_entropy_free(&entropy);
}

bool ProviderUdpSSL::init(const QJsonObject &deviceConfig)
{
	bool isInitOK = false;

	// Initialise sub-class
	if ( LedDevice::init(deviceConfig) )
	{
		//PSK Pre Shared Key
		_psk           = deviceConfig["psk"].toString();
		_psk_identity  = deviceConfig["psk_identity"].toString();
		_port          = deviceConfig["sslport"].toInt(2100);
		_server_name   = deviceConfig["servername"].toString();

		if( deviceConfig.contains("transport_type") ) _transport_type        = deviceConfig["transport_type"].toString("DTLS");
		if( deviceConfig.contains("seed_custom") )    _custom                = deviceConfig["seed_custom"].toString("dtls_client");
		if( deviceConfig.contains("retry_left") )     _retry_left            = deviceConfig["retry_left"].toInt(MAX_RETRY);
		if( deviceConfig.contains("hs_attempts") )    _handshake_attempts    = deviceConfig["hs_attempts"].toInt(5);
		if (deviceConfig.contains("hs_timeout_min"))  _handshake_timeout_min = deviceConfig["hs_timeout_min"].toInt(300);
		if (deviceConfig.contains("hs_timeout_max"))  _handshake_timeout_max = deviceConfig["hs_timeout_max"].toInt(1000);

		QString host = deviceConfig["host"].toString(_defaultHost);

		if ( _address.setAddress(host) )
		{
			Debug( _log, "Successfully parsed %s as an ip address.", QSTRING_CSTR( host ) );
		}
		else
		{
			Debug( _log, "Failed to parse [%s] as an ip address.", QSTRING_CSTR( host ) );
			QHostInfo info = QHostInfo::fromName(host);
			if ( info.addresses().isEmpty() )
			{
				Debug( _log, "Failed to parse [%s] as a hostname.", QSTRING_CSTR( host ) );
				QString errortext = QString("Invalid target address [%1]!").arg(host);
				this->setInError( errortext );
				isInitOK = false;
			}
			else
			{
				Debug( _log, "Successfully parsed %s as a hostname.", QSTRING_CSTR( host ) );
				_address = info.addresses().first();
			}
		}

		int config_port = deviceConfig["sslport"].toInt(_port);

		if ( config_port <= 0 || config_port > MAX_PORT_SSL )
		{
			QString errortext = QString ("Invalid target port [%1]!").arg(config_port);
			this->setInError( errortext );
			isInitOK = false;
		}
		else
		{
			_ssl_port = config_port;
			Debug( _log, "UDP SSL using %s:%u", QSTRING_CSTR( _address.toString() ), _ssl_port );
			isInitOK = true;
		}
	}
	return isInitOK;
}

int ProviderUdpSSL::open()
{
	_streamReady = false;

	if ( !initNetwork() )
	{
		this->setInError( "UDP SSL Network error!" );
		return -1;
	}

	return 0;
}

int ProviderUdpSSL::close()
{
	QMutexLocker locker(&_hueMutex);

	closeConnection();


	return 0;
}

void ProviderUdpSSL::closeConnection()
{
	if (_streamReady)
	{
		closeSSLNotify();
		freeSSLConnection();
	}

	_streamReady = false;
}

const int *ProviderUdpSSL::getCiphersuites() const
{
	return mbedtls_ssl_list_ciphersuites();
}

bool ProviderUdpSSL::initNetwork()
{
	QMutexLocker locker(&_hueMutex);

	if (!initConnection())
		return false;

	_streamReady = true;

	_streamPaused = false;

	_isDeviceReady = true;

	return true;
}

bool ProviderUdpSSL::initConnection()
{	
	mbedtls_net_init(&client_fd);
	mbedtls_ssl_init(&ssl);
	mbedtls_ssl_config_init(&conf);	
	return setupStructure();
}

bool ProviderUdpSSL::seedingRNG()
{	
	mbedtls_entropy_init(&entropy);	

	QByteArray customDataArray = _custom.toLocal8Bit();
	const char* customData = customDataArray.constData();
	
	int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func,
		                            &entropy, reinterpret_cast<const unsigned char*>(customData),
		                            std::min(strlen(customData), (size_t)MBEDTLS_CTR_DRBG_MAX_SEED_INPUT));

	if (ret != 0)
	{
		Error(_log, "%s", QSTRING_CSTR(QString("mbedtls_ctr_drbg_seed FAILED %1").arg(errorMsg(ret))));
		return false;
	}

	return true;
}

bool ProviderUdpSSL::setupStructure()
{	
	int transport = ( _transport_type == "DTLS" ) ? MBEDTLS_SSL_TRANSPORT_DATAGRAM : MBEDTLS_SSL_TRANSPORT_STREAM;

	int ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, transport, MBEDTLS_SSL_PRESET_DEFAULT);

	if (ret != 0)
	{
		Error(_log, "%s", QSTRING_CSTR(QString("mbedtls_ssl_config_defaults FAILED %1").arg( errorMsg( ret ) )));
		return false;
	}

	const int * ciphersuites = getCiphersuites();	

	mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);

	mbedtls_ssl_conf_handshake_timeout(&conf, _handshake_timeout_min, _handshake_timeout_max);

	mbedtls_ssl_conf_ciphersuites(&conf, ciphersuites);
	mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

	if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0)
	{
		Error(_log, "%s", QSTRING_CSTR(QString("mbedtls_ssl_setup FAILED %1").arg( errorMsg( ret ) )));
		return false;
	}

	return startUPDConnection();
}

bool ProviderUdpSSL::startUPDConnection()
{	
	mbedtls_ssl_session_reset(&ssl);

	if(!setupPSK())
		return false;

	int ret = mbedtls_net_connect(&client_fd, _address.toString().toUtf8(), std::to_string(_ssl_port).c_str(), MBEDTLS_NET_PROTO_UDP);

	if (ret != 0)
	{
		Error(_log, "%s", QSTRING_CSTR(QString("mbedtls_net_connect FAILED %1").arg( errorMsg( ret ) )));
		return false;
	}

	mbedtls_ssl_set_bio(&ssl, &client_fd, mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);
	mbedtls_ssl_set_timer_cb(&ssl, &timer, mbedtls_timing_set_delay, mbedtls_timing_get_delay);

	return startSSLHandshake();
}

bool ProviderUdpSSL::setupPSK()
{	
	QByteArray pskRawArray = QByteArray::fromHex(_psk.toUtf8());
	QByteArray pskIdRawArray = _psk_identity.toUtf8();

	int ret = mbedtls_ssl_conf_psk( &conf,
									reinterpret_cast<const unsigned char*> (pskRawArray.constData()),
									pskRawArray.length(),
									reinterpret_cast<const unsigned char*> (pskIdRawArray.constData()),
									pskIdRawArray.length());

	if (ret != 0)
	{
		Error(_log, "%s", QSTRING_CSTR(QString("mbedtls_ssl_conf_psk FAILED %1").arg( errorMsg( ret ) )));
		return false;
	}

	return true;
}

bool ProviderUdpSSL::startSSLHandshake()
{	
	int ret = 0;
	
	for (unsigned int attempt = 1; attempt <= _handshake_attempts; ++attempt)
	{
		do
		{
			ret = mbedtls_ssl_handshake(&ssl);
		}
		while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

		if (ret == 0)
		{
			break;
		}
		else
		{
			Warning(_log, "%s", QSTRING_CSTR(QString("mbedtls_ssl_handshake attempt %1/%2 FAILED. Reason: %3").arg( attempt ).arg( _handshake_attempts ).arg( errorMsg( ret ) ) ));
		}

		QThread::msleep(200);
	}

	if (ret != 0)
	{
		Error(_log, "%s", QSTRING_CSTR(QString("mbedtls_ssl_handshake FAILED %1").arg( errorMsg( ret ) )));		
		return false;
	}
	else
	{
		if (mbedtls_ssl_get_verify_result( &ssl ) != 0 )
		{
			Error(_log, "SSL certificate verification failed!");
			return false;
		}
	}

	return true;
}

void ProviderUdpSSL::freeSSLConnection()
{	
	try
	{
		mbedtls_ssl_session_reset(&ssl);
		mbedtls_net_free(&client_fd);
		mbedtls_ssl_free(&ssl);
		mbedtls_ssl_config_free(&conf);
	}
	catch (std::exception &e)
	{
		Error(_log, "%s", QSTRING_CSTR(QString("SSL Connection clean-up Error: %s").arg( e.what() ) ));
	}
	catch (...)
	{
		Error(_log, "SSL Connection clean-up Error: <unknown>" );
	}
}

void ProviderUdpSSL::writeBytes(unsigned int size, const uint8_t* data, bool flush)
{
	if (!_streamReady || _streamPaused)
		return;

	QMutexLocker locker(&_hueMutex);

	if (!_streamReady || _streamPaused)
		return;

	_streamPaused = flush;

	int ret = 0;

	do
	{
		ret = mbedtls_ssl_write(&ssl, data, size);
	}
	while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

	if (ret <= 0)
	{
		Warning(_log, "mbedtls_ssl_read returned: %s", QSTRING_CSTR(errorMsg(ret)));

		if (_retry_left > 0)
		{
			_retry_left--;
			closeConnection();
			initConnection();
		}
	}
}

QString ProviderUdpSSL::errorMsg(int ret)
{
	char error_buf[1024];
	mbedtls_strerror(ret, error_buf, 1024);

	return QString("Last error was: code = %1, description = %2").arg( ret ).arg( error_buf );
}

void ProviderUdpSSL::closeSSLNotify()
{
	/* No error checking, the connection might be closed already */
	while (mbedtls_ssl_close_notify(&ssl) == MBEDTLS_ERR_SSL_WANT_WRITE);
}


