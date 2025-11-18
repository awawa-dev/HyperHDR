/* DriverNetLifx.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2025 awawa-dev
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

#include <led-drivers/net/DriverNetLifx.h>
#include <infinite-color-engine/ColorSpace.h>

#include <QUdpSocket>
#include <QNetworkInterface>
#include <QByteArray>
#include <bit>
#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <cmath>
#include <tuple>
#include <linalg.h>

namespace
{
	constexpr uint16_t DEFAULT_FLIX_PORT = 56700;
	constexpr uint16_t WHITE_COLOR_TEMPERATURE = 6500; //D65
	constexpr uint16_t POWER_ON = 0xFFFF;
	constexpr uint16_t POWER_OFF = 0x0;

#pragma pack(push, 1)
	struct LifxHeader {
		uint16_t size;

		uint16_t frameFlags;
		uint32_t source;

		std::array<uint8_t, 8> target;
		std::array<uint8_t, 6> reserved1{};

		uint8_t flags;

		uint8_t sequence;

		std::array<uint8_t, 8> reserved2{};
		uint16_t pkt_type;
		std::array<uint8_t, 2> reserved3{};
	};
	static_assert(sizeof(LifxHeader) == 36, "LifxHeader must be 36 bytes");

	struct LifxSetColorPayload {
		uint8_t  reserved{};
		uint16_t hue{};
		uint16_t saturation{};
		uint16_t brightness{};
		uint16_t kelvin{};
		uint32_t duration{};  // ms
	};
	static_assert(sizeof(LifxSetColorPayload) == 13, "LifxSetColorPayload must be 13 bytes");

	struct LifxSetPowerPayload {
		uint16_t level;
	};
	static_assert(sizeof(LifxSetPowerPayload) ==2, "LifxSetColorPayload must be 2 bytes");

#pragma pack(pop)

	QByteArray buildLifxSetColorPacket(const DriverNetLifx::MacAddress& mac,
		const linalg::vec<float, 3>& rgb,
		uint16_t kelvin,
		uint32_t duration_ms)
	{
		LifxHeader header{};
		LifxSetColorPayload payload{};

		header.size = qToLittleEndian<uint16_t>(sizeof(LifxHeader) + sizeof(LifxSetColorPayload));
		uint16_t frameFlags = (1024 & 0x0FFF) | (1 << 12);
		header.frameFlags = qToLittleEndian<uint16_t>(frameFlags);
		header.source = qToLittleEndian<uint32_t>(0x12345678);

		header.target.fill(0);
		std::copy(mac.begin(), mac.end(), header.target.begin());

		header.flags = 0;
		header.sequence = 0;
		header.pkt_type = qToLittleEndian<uint16_t>(102);

		auto hsv = ColorSpaceMath::rgb2hsv(rgb);

		const uint16_t hue16 = qToLittleEndian<uint16_t>(static_cast<uint16_t>((hsv.x / 360.0f) * 65535.0f));
		const uint16_t sat16 = qToLittleEndian<uint16_t>(static_cast<uint16_t>(hsv.y * 65535.0f));
		const uint16_t bri16 = qToLittleEndian<uint16_t>(static_cast<uint16_t>(hsv.z * 65535.0f));

		payload.hue = hue16;
		payload.saturation = sat16;
		payload.brightness = bri16;
		payload.kelvin = qToLittleEndian<uint16_t>(kelvin);
		payload.duration = qToLittleEndian<uint32_t>(duration_ms);

		QByteArray packet;
		packet.resize(sizeof(header) + sizeof(payload));
		std::memcpy(packet.data(), &header, sizeof(header));
		std::memcpy(packet.data() + sizeof(header), &payload, sizeof(payload));

		return packet;
	}

	QByteArray buildLifxSetPower(const DriverNetLifx::MacAddress& mac, uint16_t level)
	{
		LifxHeader header{};
		LifxSetPowerPayload payload{};

		header.size = qToLittleEndian<uint16_t>(sizeof(LifxHeader) + sizeof(LifxSetPowerPayload));
		uint16_t frameFlags = (1024 & 0x0FFF) | (1 << 12);
		header.frameFlags = qToLittleEndian<uint16_t>(frameFlags);
		header.source = qToLittleEndian<uint32_t>(0x12345678);

		header.target.fill(0);
		std::copy(mac.begin(), mac.end(), header.target.begin());

		header.flags = 0;
		header.sequence = 0;
		header.pkt_type = qToLittleEndian<uint16_t>(21);

		payload.level = qToLittleEndian<uint16_t>(level);

		QByteArray packet;
		packet.resize(sizeof(header) + sizeof(payload));
		std::memcpy(packet.data(), &header, sizeof(header));
		std::memcpy(packet.data() + sizeof(header), &payload, sizeof(payload));

		return packet;
	}

	void broadcastLifxGetService(QUdpSocket& socket)
	{
		LifxHeader header{};

		header.size = qToLittleEndian<uint16_t>(sizeof(LifxHeader));
		uint16_t frameFlags = (1024 & 0x0FFF) | (1 << 12) | (1 << 13);
		header.frameFlags = qToLittleEndian<uint16_t>(frameFlags);
		header.source = qToLittleEndian<uint32_t>(0x11);

		header.target.fill(0);

		const char magic[] = "LIFXV2";
		std::memcpy(header.reserved1.data(), magic, sizeof(header.reserved1));
		header.flags = 0x04;
		header.sequence = 0;
		header.pkt_type = qToLittleEndian<uint16_t>(2);

		QByteArray packet;
		packet.resize(sizeof(header));
		std::memcpy(packet.data(), &header, sizeof(header));

		for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
			for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
				if (entry.broadcast().isNull()) continue;
				socket.writeDatagram(packet, entry.broadcast(), DEFAULT_FLIX_PORT);
			}
		}
	}

	int sendLifxColors(QUdpSocket& socket,
		const std::vector<std::pair<DriverNetLifx::IpMacAddress, linalg::vec<float, 3>>>& lights,
		uint16_t kelvin,
		uint32_t duration_ms = 0)
	{
		int totalSize = 0;
		for (const auto& [ipmac, color] : lights) {
			QByteArray packet = buildLifxSetColorPacket(std::get<2>(ipmac), color, kelvin, duration_ms);
			totalSize += packet.size();
			socket.writeDatagram(packet, std::get<1>(ipmac), DEFAULT_FLIX_PORT);
		}
		return totalSize;
	}

	int sendLifxSetPower(QUdpSocket& socket,
		const std::vector<DriverNetLifx::IpMacAddress>& lights,
		uint16_t power)
	{
		int totalSize = 0;
		for (const auto& ipmac : lights) {
			QByteArray packet = buildLifxSetPower(std::get<2>(ipmac), power);
			totalSize += packet.size();
			socket.writeDatagram(packet, std::get<1>(ipmac), DEFAULT_FLIX_PORT);
		}
		return totalSize;
	}

	std::optional<DriverNetLifx::MacAddress> macFromString(const QString& macStr)
	{
		static const QRegularExpression regex("^([0-9A-Fa-f]{2}([-:])){5}([0-9A-Fa-f]{2})$");

		if (!regex.match(macStr).hasMatch())
			return std::nullopt;

		DriverNetLifx::MacAddress mac{};
		auto parts = macStr.split(QRegularExpression("[:-]"));
		for (int i = 0; i < 6; ++i) {
			bool ok = false;
			mac[i] = static_cast<uint8_t>(parts[i].toUInt(&ok, 16));
			if (!ok) return std::nullopt;
		}
		return mac;
	}
}

DriverNetLifx::DriverNetLifx(const QJsonObject& deviceConfig)
	: ProviderUdp(deviceConfig),
	transition(0)
{
}

QJsonObject DriverNetLifx::discover(const QJsonObject& /*params*/)
{
	QJsonObject devicesDiscovered;
	QJsonArray lifxDevices;
	devicesDiscovered.insert("ledDeviceType", _activeDeviceType);

	auto socket = std::make_unique<QUdpSocket>();

	if (!socket->bind(QHostAddress::AnyIPv4, 0,  QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
	{
		Error(_log, "Could not bind the socket");
	}
	else
	{
		broadcastLifxGetService(*socket);

		QElapsedTimer timer;
		timer.start();

		while (timer.elapsed() < 3000) {
			if (!socket->waitForReadyRead(50))
				continue;

			Debug(_log, "Received a datagram...");

			while (socket->hasPendingDatagrams()) {
				QHostAddress sender;
				quint16 senderPort;
				QByteArray datagram;
				datagram.resize(socket->pendingDatagramSize());
				socket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

				if (datagram.size() < sizeof(LifxHeader))
				{
					Debug(_log, "The response datagram is too small. Got: %i bytes", static_cast<int>(datagram.size()));
					continue;
				}

				const LifxHeader* hdr = reinterpret_cast<const LifxHeader*>(datagram.constData());
				uint16_t pktType = qFromLittleEndian<uint16_t>(hdr->pkt_type);
				if (pktType != 3)
				{
					Debug(_log, "Unexpected pktType != 3. Got: %i", pktType);
					continue;
				}

				QByteArray macBytes(reinterpret_cast<const char*>(hdr->target.data()), 6);
				QStringList parts;
				for (uint8_t c : macBytes)
				{
					parts << QString("%1").arg(static_cast<unsigned int>(c), 2, 16, QLatin1Char('0'));
				}

				QString macStr = parts.join(":").toUpper();
				QString ipStr = sender.toString();

				QJsonObject lamp;
				lamp["name"] = QString("Lamp #%1").arg(lifxDevices.size());
				lamp["ipAddress"] = ipStr;
				lamp["macAddress"] = macStr;

				if (std::any_of(lifxDevices.begin(), lifxDevices.end(), [&](const QJsonValue& val) {
					return val.toObject()["macAddress"].toString() == macStr;
					}))
				{
					Debug(_log, "Mac address already exists: %s", QSTRING_CSTR(macStr));
				}
				else
				{
					lifxDevices.append(lamp);
				}
			}
		}
	}

	devicesDiscovered.insert("devices", lifxDevices);
	Debug(_log, "devicesDiscovered: [%s]", QString(QJsonDocument(devicesDiscovered).toJson(QJsonDocument::Compact)).toUtf8().constData());

	return devicesDiscovered;
}

bool DriverNetLifx::init(QJsonObject deviceConfig)
{
	_port = DEFAULT_FLIX_PORT;

	// Initialise sub-class
	bool isInitOK = ProviderUdp::init(deviceConfig);

	// Parse configuration
	transition = deviceConfig["transition"].toInt(0);
	Debug(_log, "Transition: %i", transition);

	const auto arr = deviceConfig["lamps"].toArray();
	lamps.clear();
	lamps.reserve(arr.size());
	for (const auto& value : arr) {
		auto jObject = value.toObject();

		auto lampName = jObject["name"].toString();

		auto macString = jObject["macAddress"].toString();
		auto mac = macFromString(macString);
		if (!mac.has_value())
		{
			Error(_log, "Could not parse MAC address: %s", QSTRING_CSTR(macString));
			continue;
		}

		auto ipString = jObject["ipAddress"].toString();
		QHostAddress address;
		bool validAddress = address.setAddress(ipString);
		if (!validAddress)
		{
			Error(_log, "Could not parse IP address: %s", QSTRING_CSTR(ipString));
			continue;
		}

		Debug(_log, "Added %s: %s, MAC: %s", QSTRING_CSTR(lampName), QSTRING_CSTR(ipString), QSTRING_CSTR(macString));
		lamps.push_back(std::tuple<QString, QHostAddress, MacAddress>(lampName, address, mac.value()));
	}

	return isInitOK && (lamps.size() > 0);
}

std::pair<bool, int> DriverNetLifx::writeInfiniteColors(SharedOutputColors nonlinearRgbColors)
{
	std::vector<std::pair<IpMacAddress, linalg::vec<float, 3>>> lights;

	const size_t n = std::min(nonlinearRgbColors->size(), lamps.size());

	for (size_t i = 0; i < n; ++i) {
		lights.push_back({lamps[i], (*nonlinearRgbColors)[i] });
	}

	return { true, sendLifxColors(*_udpSocket, lights, WHITE_COLOR_TEMPERATURE, transition) };
}

void DriverNetLifx::setPower(uint16_t power)
{
	Debug(_log, "setPower: %i", power);
	sendLifxSetPower(*_udpSocket, lamps, power);	
}

bool DriverNetLifx::powerOn()
{
	setPower(POWER_ON);
	return true;
}

bool DriverNetLifx::powerOff()
{
	setPower(POWER_OFF);
	return true;
}

void DriverNetLifx::identify(const QJsonObject& params)
{
	QString jsonString = QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact));
	Debug(_log, "Request to identify the lamp %s", QSTRING_CSTR(jsonString));
	if (params.contains("name") && params.contains("ipAddress") && params.contains("macAddress"))
	{
		auto socket = std::make_unique<QUdpSocket>();

		if (!socket->bind(QHostAddress::AnyIPv4, 0, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
		{
			Error(_log, "Could not bind the socket");
		}
		else
		{
			auto name = params["name"].toString();
			auto ipAddress = params["ipAddress"].toString();
			auto macAddress = params["macAddress"].toString();

			QHostAddress address;
			bool validAddress = address.setAddress(ipAddress);
			if (!validAddress)
			{
				Error(_log, "Could not parse IP address: %s", QSTRING_CSTR(ipAddress));
				return;
			}

			auto mac = macFromString(macAddress);
			if (!mac.has_value())
			{
				Error(_log, "Could not parse MAC address: %s", QSTRING_CSTR(macAddress));
				return;
			}

			if (!name.isEmpty() && !macAddress.isEmpty() && !ipAddress.isEmpty())
			{
				Debug(_log, "Testing lamp %s: %s - %s", QSTRING_CSTR(name) , QSTRING_CSTR(ipAddress), QSTRING_CSTR(macAddress));

				std::vector<std::pair<IpMacAddress, linalg::vec<float, 3>>> lights;				
				lights.push_back({ std::tuple<QString, QHostAddress, MacAddress>(name, address, mac.value()),  { 0.0, 0.0, 1.0 } });
				sendLifxSetPower(*socket, { lights.front().first }, POWER_ON);
				sendLifxColors(*socket, lights, WHITE_COLOR_TEMPERATURE, 250);
				QThread::msleep(250);
				lights.clear();
				lights.push_back({ std::tuple<QString, QHostAddress, MacAddress>(name, address, mac.value()),  { 0.0, 0.0, 0.0 } });
				sendLifxColors(*socket, lights, WHITE_COLOR_TEMPERATURE, 2000);
			}
		}
	}
}

LedDevice* DriverNetLifx::construct(const QJsonObject& deviceConfig)
{
	return new DriverNetLifx(deviceConfig);
}

bool DriverNetLifx::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("lifx", "leds_group_2_network", DriverNetLifx::construct);
