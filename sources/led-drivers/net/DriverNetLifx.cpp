#include <led-drivers/net/DriverNetLifx.h>
#include <infinite-color-engine/ColorSpace.h>

#include <QUdpSocket>
#include <QByteArray>
#include <bit>
#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <cmath>
#include <linalg.h>

namespace
{
	constexpr uint16_t DEFAULT_FLIX_PORT = 56700;

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

	int sendLifxColors(QUdpSocket& socket,
		const std::vector<std::pair<DriverNetLifx::IpMacAddress, linalg::vec<float, 3>>>& lights,
		uint16_t kelvin,
		uint32_t duration_ms = 0)
	{
		int totalSize = 0;
		for (const auto& [ipmac, color] : lights) {
			QByteArray packet = buildLifxSetColorPacket(ipmac.second, color, kelvin, duration_ms);
			totalSize += packet.size();
			socket.writeDatagram(packet, ipmac.first, DEFAULT_FLIX_PORT);
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
	: ProviderUdp(deviceConfig)
{
}

bool DriverNetLifx::init(QJsonObject deviceConfig)
{
	_port = DEFAULT_FLIX_PORT;

	// Initialise sub-class
	bool isInitOK = ProviderUdp::init(deviceConfig);

	// Parse configuration	
	const auto arr = deviceConfig["addresses"].toArray();
	lamps.clear();
	lamps.reserve(arr.size());
	for (const auto& value : arr) {
		auto jObject = value.toObject();
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

		Debug(_log, "Added lamp's IP: %s, MAC: %s", QSTRING_CSTR(ipString), QSTRING_CSTR(macString));
		lamps.push_back(std::pair<QHostAddress,MacAddress>(address, mac.value()));
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

	return { true, sendLifxColors(*_udpSocket, lights, 6500, 0) };
}

LedDevice* DriverNetLifx::construct(const QJsonObject& deviceConfig)
{
	return new DriverNetLifx(deviceConfig);
}

bool DriverNetLifx::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("lifx", "leds_group_2_network", DriverNetLifx::construct);
