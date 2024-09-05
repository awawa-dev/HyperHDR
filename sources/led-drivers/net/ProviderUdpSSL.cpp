/* ProviderUdpSSL.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
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
#include <QSslCipher>
#include <QDtls>
#include <QSslPreSharedKeyAuthenticator>
#include <QSslConfiguration>

#include <led-drivers/net/ProviderUdpSSL.h>

#ifdef USE_STATIC_QT_PLUGINS
	#include <QtPlugin>
	Q_IMPORT_PLUGIN(QTlsBackendOpenSSLPlugin)
#endif

ProviderUdpSSL::ProviderUdpSSL(const QJsonObject& deviceConfig)
	: LedDevice(deviceConfig)
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
	, _handshake_attempts_left(5)
	, _streamReady(false)
	, _handshake_timeout_min(300)
	, _handshake_timeout_max(1000)
	, _dtls(nullptr)
	, _socket(nullptr)

{
}

ProviderUdpSSL::~ProviderUdpSSL()
{
	closeNetwork();
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

		if (config_port <= 0 || config_port > 65535)
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



std::list<QString> ProviderUdpSSL::getCiphersuites()
{
	std::list<QString> ret{ "PSK-AES128-GCM-SHA256" };
	return ret;
}

bool ProviderUdpSSL::initNetwork()
{
	closeNetwork();

	_dtls = new QDtls(QSslSocket::SslClientMode, this);
	_socket = new QUdpSocket(this);
	_handshake_attempts_left = _handshake_attempts;

	QSslConfiguration config = QSslConfiguration::defaultDtlsConfiguration();
	QList<QSslCipher> allowedCiphers;
	auto useCiphers = getCiphersuites();
	for (auto& name : useCiphers)
	{
		auto cipher = QSslCipher(name);
		if (cipher.isNull())
		{
			Error(_log, "You system is missing support for %s cipher. Please install OpenSSL 1.1.1 or higher (3.x).", QSTRING_CSTR(name));
			this->setInError("Cannot initialize the neccesery ciphers. Please install OpenSSL 1.1.1 or higher (3.x)");
			return false;
		}
	};
	config.setCiphers(allowedCiphers);

	config.setPeerVerifyMode(QSslSocket::QueryPeer);
	_dtls->setDtlsConfiguration(config);
	_dtls->setPeer(_address, _ssl_port);	

	connect(_dtls, &QDtls::pskRequired, this, &ProviderUdpSSL::pskRequired);
	connect(_dtls, &QDtls::handshakeTimeout, this, &ProviderUdpSSL::handshakeTimeout);
	connect(_socket, &QUdpSocket::connected, this, [&]() {
		if (_dtls != nullptr && _socket != nullptr)
		{
			Debug(_log, "Connected to the host. Initiating a handshake");
			_dtls->doHandshake(_socket);
		}
	});

	connect(_socket, &QUdpSocket::errorOccurred, this, [&](QAbstractSocket::SocketError socketError) {
		QString message = QString("Socket error nr: %1").arg(QString::number(socketError));
		QUEUE_CALL_1(this, errorHandling, QString, message);
	});

	connect(_socket, &QUdpSocket::readyRead, this, [&](){
		if (_dtls ==nullptr || _socket == nullptr ||  _socket->pendingDatagramSize() <= 0)
			return;

		QByteArray dgram(_socket->pendingDatagramSize(), Qt::Uninitialized);
		const qint64 bytesRead = _socket->readDatagram(dgram.data(), dgram.size());
		if (_dtls->isConnectionEncrypted() || bytesRead <= 0)
			return;

		Debug(_log, "Welcome datagram received. Proceeding with a handshake");

		dgram.resize(bytesRead);

		if (!_dtls->doHandshake(_socket, dgram))
		{
			QString message = "Failed to continue the handshake";
			QUEUE_CALL_1(this, errorHandling, QString, message);
			return;
		}

		if (_dtls->isConnectionEncrypted())
		{
			Debug(_log, "Established encrypted connection");
			_streamReady = true;
		}
	});

	if (!_dtls->doHandshake(_socket))
		errorHandling("Failed to initiate the handshake");
	else
		Debug(_log, "Starting the handshake");

	return true;
}

void ProviderUdpSSL::pskRequired(QSslPreSharedKeyAuthenticator* authenticator)
{
	QByteArray pskRawArray = QByteArray::fromHex(_psk.toUtf8());
	QByteArray pskIdRawArray = _psk_identity.toUtf8();

	Debug(_log, "The server requested our PSK identity");

	authenticator->setIdentity(pskIdRawArray);
	authenticator->setPreSharedKey(pskRawArray);
}

int ProviderUdpSSL::closeNetwork()
{
	_streamReady = false;

	if (_dtls != nullptr)
	{
		_dtls->shutdown(_socket);
		_dtls->deleteLater();
		_dtls = nullptr;
	}

	if (_socket != nullptr)
	{
		if (_socket->state() == QAbstractSocket::SocketState::ConnectedState)
		{
			_socket->close();
		}
		_socket->deleteLater();
		_socket = nullptr;
	}

	return 0;
}

void ProviderUdpSSL::errorHandling(QString message)
{
	closeNetwork();
	this->setInError(message);
	setupRetry(3000);
}

void ProviderUdpSSL::handshakeTimeout()
{
	if (_handshake_attempts_left > 0)
	{
		Debug(_log, "Timeout. Resuming handshake (%i/%i)", (_handshake_attempts - _handshake_attempts_left + 1), _handshake_attempts);
		_handshake_attempts_left--;

		if (!_dtls->handleTimeout(_socket))
		{
			QString message = "Failed to resume the handshake after timeout";
			QUEUE_CALL_1(this, errorHandling, QString, message);
		}
	}
	else
	{
		QString message = "Another timeout. Give up";
		QUEUE_CALL_1(this, errorHandling, QString, message);
	}
}

void ProviderUdpSSL::writeBytes(unsigned int size, const uint8_t* data, bool flush)
{
	if (_dtls != nullptr && _socket != nullptr && (_streamReady || _dtls->isConnectionEncrypted()))
	{
		_streamReady = true;
		
		QByteArray dgram = QByteArray::fromRawData(reinterpret_cast<const char*>(data), size);
		if (_dtls->writeDatagramEncrypted(_socket, dgram) < 0)
		{
			errorHandling(_dtls->dtlsErrorString());
		}
	}
}

