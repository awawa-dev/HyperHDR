/* ProviderUdpSSL.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2023 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/HyperHDR
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
 */


// STL includes
#include <cstdio>
#include <exception>
#include <algorithm>

// Linux includes
#include <fcntl.h>
#ifndef _WIN32
	#include <sys/ioctl.h>
#endif

#include <QHostInfo>
#include <QUdpSocket>
#include <QThread>

#include "ProviderUdpSSL.h"

#if !defined(MBEDTLS_OLD_CONFIG_FILE)
	#include <mbedtls/build_info.h>
#else
	#include MBEDTLS_OLD_CONFIG_FILE
#endif

#if defined(MBEDTLS_PLATFORM_C)
	#include <mbedtls/platform.h>
#endif

#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl_ciphersuites.h>
#include <mbedtls/entropy.h>
#include <mbedtls/timing.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>
#include <mbedtls/debug.h>

const int MAX_RETRY = 20;
const ushort MAX_PORT_SSL = 65535;

ProviderUdpSSL::ProviderUdpSSL(const QJsonObject& deviceConfig)
	: LedDevice(deviceConfig)
	, client_fd(new mbedtls_net_context())
	, entropy(nullptr)
	, ssl(new mbedtls_ssl_context())
	, conf(new mbedtls_ssl_config())
	, cacert(new mbedtls_x509_crt())
	, ctr_drbg(new mbedtls_ctr_drbg_context())
	, timer(new mbedtls_timing_delay_context())
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
}

ProviderUdpSSL::~ProviderUdpSSL()
{
	closeConnection();

	if (entropy != nullptr)
	{		
		mbedtls_ctr_drbg_free(ctr_drbg);
		mbedtls_entropy_free(entropy);
		delete entropy;
	}

	delete client_fd;	
	delete ssl;
	delete conf;
	delete cacert;
	delete ctr_drbg;
	delete timer;
}

bool ProviderUdpSSL::init(const QJsonObject& deviceConfig)
{
	bool isInitOK = false;

	// Initialise sub-class
	if (LedDevice::init(deviceConfig))
	{
		//PSK Pre Shared Key
		_psk = deviceConfig["psk"].toString();
		_psk_identity = deviceConfig["psk_identity"].toString();
		_port = deviceConfig["sslport"].toInt(2100);
		_server_name = deviceConfig["servername"].toString();

		if (deviceConfig.contains("transport_type")) _transport_type = deviceConfig["transport_type"].toString("DTLS");
		if (deviceConfig.contains("seed_custom"))    _custom = deviceConfig["seed_custom"].toString("dtls_client");
		if (deviceConfig.contains("retry_left"))     _retry_left = deviceConfig["retry_left"].toInt(MAX_RETRY);
		if (deviceConfig.contains("hs_attempts"))    _handshake_attempts = deviceConfig["hs_attempts"].toInt(5);
		if (deviceConfig.contains("hs_timeout_min"))  _handshake_timeout_min = deviceConfig["hs_timeout_min"].toInt(300);
		if (deviceConfig.contains("hs_timeout_max"))  _handshake_timeout_max = deviceConfig["hs_timeout_max"].toInt(1000);

		QString host = deviceConfig["host"].toString(_defaultHost);

		if (_address.setAddress(host))
		{
			Debug(_log, "Successfully parsed %s as an ip address.", QSTRING_CSTR(host));
		}
		else
		{
			Debug(_log, "Failed to parse [%s] as an ip address.", QSTRING_CSTR(host));
			QHostInfo info = QHostInfo::fromName(host);
			if (info.addresses().isEmpty())
			{
				Debug(_log, "Failed to parse [%s] as a hostname.", QSTRING_CSTR(host));
				QString errortext = QString("Invalid target address [%1]!").arg(host);
				this->setInError(errortext);
				isInitOK = false;
			}
			else
			{
				Debug(_log, "Successfully parsed %s as a hostname.", QSTRING_CSTR(host));
				_address = info.addresses().first();
			}
		}

		int config_port = deviceConfig["sslport"].toInt(_port);

		if (config_port <= 0 || config_port > MAX_PORT_SSL)
		{
			QString errortext = QString("Invalid target port [%1]!").arg(config_port);
			this->setInError(errortext);
			isInitOK = false;
		}
		else
		{
			_ssl_port = config_port;
			Debug(_log, "UDP SSL using %s:%u", QSTRING_CSTR(_address.toString()), _ssl_port);
			isInitOK = true;
		}
	}
	return isInitOK;
}



const int* ProviderUdpSSL::getCiphersuites() const
{
	return mbedtls_ssl_list_ciphersuites();
}

bool ProviderUdpSSL::initNetwork()
{
	if ((!_isDeviceReady || _streamPaused) && _streamReady)
		closeConnection();

	if (!initConnection())
		return false;

	return true;
}

int ProviderUdpSSL::closeNetwork()
{
	closeConnection();

	return 0;
}

bool ProviderUdpSSL::initConnection()
{
	if (_streamReady)
		return true;

	if (!createEntropy())
	{
		Error(_log, "Failed to generate initial entropy");
		return false;
	}

	mbedtls_net_init(client_fd);
	mbedtls_ssl_init(ssl);
	mbedtls_ssl_config_init(conf);
	mbedtls_x509_crt_init(cacert);

	if (setupStructure())
	{
		_streamReady = true;
		_streamPaused = false;
		_isDeviceReady = true;
		return true;
	}
	else
	{
		mbedtls_net_free(client_fd);
		mbedtls_ssl_free(ssl);
		mbedtls_ssl_config_free(conf);
		mbedtls_x509_crt_free(cacert);

		return false;
	}
}

void ProviderUdpSSL::closeConnection()
{
	if (_streamReady)
	{
		closeSSLNotify();
		freeSSLConnection();
		_streamReady = false;
	}
}

bool ProviderUdpSSL::createEntropy()
{
	if (entropy != nullptr)
		return true;

	mbedtls_ctr_drbg_init(ctr_drbg);
	entropy = new mbedtls_entropy_context();
	mbedtls_entropy_init(entropy);

	const QByteArray& customDataArray = _custom.toLocal8Bit();
	const char* customData = customDataArray.constData();

	int ret = mbedtls_ctr_drbg_seed(ctr_drbg, mbedtls_entropy_func,
									entropy, reinterpret_cast<const unsigned char*>(customData),
									std::min(strlen(customData), (size_t)MBEDTLS_CTR_DRBG_MAX_SEED_INPUT));

	if (ret != 0)
	{		
		mbedtls_ctr_drbg_free(ctr_drbg);
		mbedtls_entropy_free(entropy);
		delete entropy;
		entropy = nullptr;		

		Error(_log, "%s", QSTRING_CSTR(QString("mbedtls_ctr_drbg_seed FAILED %1").arg(errorMsg(ret))));
		return false;
	}

	return true;
}

bool ProviderUdpSSL::setupStructure()
{
	int transport = (_transport_type == "DTLS") ? MBEDTLS_SSL_TRANSPORT_DATAGRAM : MBEDTLS_SSL_TRANSPORT_STREAM;

	int ret = mbedtls_ssl_config_defaults(conf, MBEDTLS_SSL_IS_CLIENT, transport, MBEDTLS_SSL_PRESET_DEFAULT);

	if (ret != 0)
	{
		Error(_log, "%s", QSTRING_CSTR(QString("mbedtls_ssl_config_defaults FAILED %1").arg(errorMsg(ret))));
		return false;
	}

	const int* ciphersuites = getCiphersuites();

	mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_REQUIRED);
	mbedtls_ssl_conf_ca_chain(conf, cacert, NULL);

	mbedtls_ssl_conf_handshake_timeout(conf, _handshake_timeout_min, _handshake_timeout_max);

	mbedtls_ssl_conf_ciphersuites(conf, ciphersuites);
	mbedtls_ssl_conf_rng(conf, mbedtls_ctr_drbg_random, ctr_drbg);

	if ((ret = mbedtls_ssl_setup(ssl, conf)) != 0)
	{
		Error(_log, "%s", QSTRING_CSTR(QString("mbedtls_ssl_setup FAILED %1").arg(errorMsg(ret))));
		return false;
	}

	return startUPDConnection();
}

bool ProviderUdpSSL::startUPDConnection()
{
	mbedtls_ssl_session_reset(ssl);

	if (!setupPSK())
		return false;

	int ret = mbedtls_net_connect(client_fd, _address.toString().toUtf8(), std::to_string(_ssl_port).c_str(), MBEDTLS_NET_PROTO_UDP);

	if (ret != 0)
	{
		Error(_log, "%s", QSTRING_CSTR(QString("mbedtls_net_connect FAILED %1").arg(errorMsg(ret))));
		return false;
	}

	mbedtls_ssl_set_bio(ssl, client_fd, mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);
	mbedtls_ssl_set_timer_cb(ssl, timer, mbedtls_timing_set_delay, mbedtls_timing_get_delay);

	return startSSLHandshake();
}

bool ProviderUdpSSL::setupPSK()
{
	QByteArray pskRawArray = QByteArray::fromHex(_psk.toUtf8());
	QByteArray pskIdRawArray = _psk_identity.toUtf8();

	int ret = mbedtls_ssl_conf_psk(conf,
									reinterpret_cast<const unsigned char*> (pskRawArray.constData()),
									pskRawArray.length(),
									reinterpret_cast<const unsigned char*> (pskIdRawArray.constData()),
									pskIdRawArray.length());

	if (ret != 0)
	{
		Error(_log, "%s", QSTRING_CSTR(QString("mbedtls_ssl_conf_psk FAILED %1").arg(errorMsg(ret))));
		return false;
	}

	return true;
}

bool ProviderUdpSSL::startSSLHandshake()
{
	int ret = 0;

	for (unsigned int attempt = 1; attempt <= _handshake_attempts; ++attempt)
	{
		if (_signalTerminate)
			return false;

		do
		{
			ret = mbedtls_ssl_handshake(ssl);
		} while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

		if (ret == 0)
		{
			break;
		}
		else
		{
			Warning(_log, "%s", QSTRING_CSTR(QString("mbedtls_ssl_handshake attempt %1/%2 FAILED. Reason: %3").arg(attempt).arg(_handshake_attempts).arg(errorMsg(ret))));
		}

		if (!_signalTerminate)
			QThread::msleep(200);
	}

	if (ret != 0)
	{
		Error(_log, "%s", QSTRING_CSTR(QString("mbedtls_ssl_handshake FAILED %1").arg(errorMsg(ret))));
		return false;
	}
	else
	{
		if (mbedtls_ssl_get_verify_result(ssl) != 0)
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
		Warning(_log, "Release mbedtls");
		mbedtls_ssl_session_reset(ssl);
		mbedtls_net_free(client_fd);
		mbedtls_ssl_free(ssl);
		mbedtls_ssl_config_free(conf);
		mbedtls_x509_crt_free(cacert);
	}
	catch (std::exception& e)
	{
		Error(_log, "%s", QSTRING_CSTR(QString("SSL Connection clean-up Error: %s").arg(e.what())));
	}
	catch (...)
	{
		Error(_log, "SSL Connection clean-up Error: <unknown>");
	}
}

void ProviderUdpSSL::writeBytes(unsigned int size, const uint8_t* data, bool flush)
{
	if (!_streamReady || _streamPaused)
		return;

	if (!_streamReady || _streamPaused)
		return;

	_streamPaused = flush;

	int ret = 0;

	do
	{
		ret = mbedtls_ssl_write(ssl, data, size);
	} while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

	if (ret <= 0)
	{
		Error(_log, "Error while writing UDP SSL stream updates. mbedtls_ssl_write returned: %s", QSTRING_CSTR(errorMsg(ret)));

		if (_streamReady)
		{
			closeConnection();

			// look for the host
			QUdpSocket socket;

			for (int i = 1; i <= _retry_left; i++)
			{
				if (_signalTerminate)
					return;

				Warning(_log, "Searching the host: %s (trial %i/%i)", QSTRING_CSTR(this->_address.toString()), i, _retry_left);

				socket.connectToHost(_address, _ssl_port);

				if (socket.waitForConnected(1000))
				{
					Warning(_log, "Found host: %s", QSTRING_CSTR(this->_address.toString()));
					socket.close();
					break;
				}
				else if (!_signalTerminate)
					QThread::msleep(1000);
			}

			Warning(_log, "Hard restart of the LED device (host: %s).", QSTRING_CSTR(this->_address.toString()));

			// hard reset
			Warning(_log, "Disabling...");
			this->disableDevice(false);

			Warning(_log, "Enabling...");
			this->enableDevice(false);

			if (!_isOn)
				emit SignalEnableStateChanged(false);
		}
	}
}

QString ProviderUdpSSL::errorMsg(int ret)
{
	char error_buf[1024];
	mbedtls_strerror(ret, error_buf, 1024);

	return QString("Last error was: code = %1, description = %2").arg(ret).arg(error_buf);
}

void ProviderUdpSSL::closeSSLNotify()
{
	/* No error checking, the connection might be closed already */
	while (mbedtls_ssl_close_notify(ssl) == MBEDTLS_ERR_SSL_WANT_WRITE);
}

int ProviderUdpSSL::get_MBEDTLS_TLS_PSK_WITH_AES_128_GCM_SHA256() const
{
	return MBEDTLS_TLS_PSK_WITH_AES_128_GCM_SHA256;
}
