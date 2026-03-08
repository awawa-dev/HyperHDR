/* DriverNetWiz.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2026 awawa-dev
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

#include <led-drivers/net/DriverNetWiz.h>

#include <QUdpSocket>
#include <QNetworkInterface>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QThread>

#include <algorithm>

namespace
{
	constexpr int DEFAULT_WIZ_PORT = 38899;
	constexpr int DISCOVERY_TIME_MS = 2000;
	constexpr int DISCOVERY_POLL_MS = 50;
}

DriverNetWiz::DriverNetWiz(const QJsonObject& deviceConfig)
	: ProviderUdp(deviceConfig)
	, _dimming(100)
{
}

QByteArray DriverNetWiz::buildSetPilotPacket(int r, int g, int b) const
{
	QJsonObject params;
	params.insert("r", r);
	params.insert("g", g);
	params.insert("b", b);

	if (_dimming >= 0 && _dimming <= 100)
		params.insert("dimming", _dimming);

	QJsonObject root;
	root.insert("method", "setPilot");
	root.insert("params", params);

	return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray DriverNetWiz::buildPowerPacket(bool on) const
{
	QJsonObject params;
	params.insert("state", on);

	if (on && _dimming >= 0 && _dimming <= 100)
		params.insert("dimming", _dimming);

	QJsonObject root;
	root.insert("method", "setPilot");
	root.insert("params", params);

	return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QJsonObject DriverNetWiz::discover(const QJsonObject& /*params*/)
{
	QJsonObject devicesDiscovered;
	QJsonArray wizDevices;
	devicesDiscovered.insert("ledDeviceType", _activeDeviceType);

	auto socket = std::make_unique<QUdpSocket>();

	if (!socket->bind(QHostAddress::AnyIPv4, 0, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
	{
		Error(_log, "Could not bind the socket for discovery");
		devicesDiscovered.insert("devices", wizDevices);
		return devicesDiscovered;
	}

	socket->setSocketOption(QAbstractSocket::MulticastTtlOption, 1);

	const QByteArray probe = QJsonDocument(QJsonObject{
		{"method", "getPilot"},
		{"params", QJsonObject{}}
	}).toJson(QJsonDocument::Compact);

	for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces())
	{
		for (const QNetworkAddressEntry& entry : iface.addressEntries())
		{
			if (entry.broadcast().isNull())
				continue;

			socket->writeDatagram(probe, entry.broadcast(), DEFAULT_WIZ_PORT);
		}
	}

	QElapsedTimer timer;
	timer.start();

	struct SeenDevice
	{
		QString ip;
		QString mac;
	};
	std::vector<SeenDevice> seen;

	while (timer.elapsed() < DISCOVERY_TIME_MS)
	{
		if (!socket->waitForReadyRead(DISCOVERY_POLL_MS))
			continue;

		while (socket->hasPendingDatagrams())
		{
			QHostAddress sender;
			quint16 senderPort = 0;
			QByteArray datagram;
			datagram.resize(socket->pendingDatagramSize());
			socket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

			QJsonParseError err{};
			const QJsonDocument doc = QJsonDocument::fromJson(datagram, &err);
			if (err.error != QJsonParseError::NoError || !doc.isObject())
				continue;

			const QJsonObject obj = doc.object();
			const QJsonObject result = obj.value("result").toObject();
			const QString mac = result.value("mac").toString("");

			const QString ip = sender.toString();

			const bool already = std::any_of(seen.begin(), seen.end(), [&](const SeenDevice& d) {
				return d.ip == ip || (!mac.isEmpty() && d.mac == mac);
			});

			if (already)
				continue;

			seen.push_back({ ip, mac });

			QJsonObject lamp;
			lamp["name"] = QString("Bulb #%1").arg(wizDevices.size());
			lamp["ipAddress"] = ip;
			lamp["macAddress"] = mac;
			wizDevices.append(lamp);
		}
	}

	devicesDiscovered.insert("devices", wizDevices);
	Debug(_log, "devicesDiscovered: [{:s}]", QString(QJsonDocument(devicesDiscovered).toJson(QJsonDocument::Compact)).toUtf8().constData());
	return devicesDiscovered;
}

bool DriverNetWiz::init(QJsonObject deviceConfig)
{
	_port = static_cast<quint16>(deviceConfig.value("port").toInt(DEFAULT_WIZ_PORT));

	const int dimming = deviceConfig.value("dimming").toInt(100);
	_dimming = std::clamp(dimming, 0, 100);
	Debug(_log, "Dimming: {:d}", _dimming);

	bool isInitOK = ProviderUdp::init(deviceConfig);

	const auto arr = deviceConfig.value("lamps").toArray();
	_lamps.clear();
	_lamps.reserve(arr.size());
	for (const auto& value : arr)
	{
		const auto jObject = value.toObject();

		WizLamp lamp;
		lamp.name = jObject.value("name").toString();
		lamp.macAddress = jObject.value("macAddress").toString();

		const QString ipString = jObject.value("ipAddress").toString();
		if (!lamp.ipAddress.setAddress(ipString))
		{
			Error(_log, "Could not parse IP address: {:s}", (ipString));
			continue;
		}

		Debug(_log, "Added {:s}: {:s}, MAC: {:s}", (lamp.name), (ipString), (lamp.macAddress));
		_lamps.push_back(std::move(lamp));
	}

	return isInitOK && (!_lamps.empty());
}

int DriverNetWiz::writeFiniteColors(const std::vector<ColorRgb>& ledValues)
{
	if (ledValues.empty() || _lamps.empty() || _udpSocket == nullptr)
		return 0;

	const size_t n = std::min(ledValues.size(), _lamps.size());
	int totalSize = 0;
	int rc = 0;

	for (size_t i = 0; i < n; ++i)
	{
		const auto& c = ledValues[i];
		const QByteArray packet = buildSetPilotPacket(static_cast<int>(c.red), static_cast<int>(c.green), static_cast<int>(c.blue));
		totalSize += packet.size();

		const qint64 written = _udpSocket->writeDatagram(packet, _lamps[i].ipAddress, _port);
		if (written != packet.size())
		{
			rc = -1;
			Warning(_log, "({:s}:{:d}) Write Error: ({:d}) {:s}",
				(_lamps[i].ipAddress.toString()),
				static_cast<int>(_port),
				static_cast<int>(_udpSocket->error()),
				(_udpSocket->errorString()));
		}
	}

	return (rc == 0) ? totalSize : -1;
}

bool DriverNetWiz::powerOn()
{
	if (_udpSocket == nullptr || _lamps.empty())
		return true;

	const QByteArray packet = buildPowerPacket(true);
	for (const auto& lamp : _lamps)
		_udpSocket->writeDatagram(packet, lamp.ipAddress, _port);

	return true;
}

bool DriverNetWiz::powerOff()
{
	if (_udpSocket == nullptr || _lamps.empty())
		return true;

	const QByteArray packet = buildPowerPacket(false);
	for (const auto& lamp : _lamps)
		_udpSocket->writeDatagram(packet, lamp.ipAddress, _port);

	return true;
}

void DriverNetWiz::identify(const QJsonObject& params)
{
	if (!(params.contains("name") && params.contains("ipAddress")))
		return;

	QHostAddress address;
	if (!address.setAddress(params.value("ipAddress").toString()))
		return;

	const int port = params.value("port").toInt(DEFAULT_WIZ_PORT);

	auto socket = std::make_unique<QUdpSocket>();
	if (!socket->bind(QHostAddress::AnyIPv4, 0, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
	{
		Error(_log, "Could not bind the socket for identify");
		return;
	}

	auto sendSetPilot = [&](int r, int g, int b, int dimming, bool state = true) {
		QJsonObject root;
		QJsonObject p;
		p.insert("state", state);
		p.insert("r", r);
		p.insert("g", g);
		p.insert("b", b);
		p.insert("dimming", dimming);
		root.insert("method", "setPilot");
		root.insert("params", p);
		const QByteArray packet = QJsonDocument(root).toJson(QJsonDocument::Compact);
		socket->writeDatagram(packet, address, static_cast<quint16>(port));
	};

	// Turn on the lamp
	sendSetPilot(0, 0, 255, 0, true);
	QThread::msleep(400);

	// Start the pulse effect
	QJsonObject pulseRoot;
	QJsonObject pulseParams;
	pulseParams.insert("delta", 100);
	pulseParams.insert("duration", 1000);
	pulseRoot.insert("method", "pulse");
	pulseRoot.insert("params", pulseParams);
	socket->writeDatagram(QJsonDocument(pulseRoot).toJson(QJsonDocument::Compact), address, static_cast<quint16>(port));
}

LedDevice* DriverNetWiz::construct(const QJsonObject& deviceConfig)
{
	return new DriverNetWiz(deviceConfig);
}

bool DriverNetWiz::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("wiz", "leds_group_2_network", DriverNetWiz::construct);
