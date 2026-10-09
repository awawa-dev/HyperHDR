#include <led-drivers/hid/DriverHidLightpack.h>
#include <linalg.h>

#ifndef PCH_ENABLED
	#include <QJsonObject>
	#include <algorithm>
	#include <cmath>
#endif

namespace
{
	constexpr uint16_t LIGHTPACK_VENDOR_ID = 0x1d50;
	constexpr uint16_t LIGHTPACK_PRODUCT_ID = 0x6022;
	constexpr uint16_t LIGHTPACK_OLD_VENDOR_ID = 0x03eb;
	constexpr uint16_t LIGHTPACK_OLD_PRODUCT_ID = 0x204f;
	constexpr uint8_t CMD_UPDATE_LEDS = 0x01;
	constexpr uint8_t CMD_SET_SMOOTH_SLOWDOWN = 0x05;
	constexpr size_t LEDS_PER_DEVICE = 10;
	constexpr size_t BYTES_PER_LED = 6;
	constexpr std::array<size_t, LEDS_PER_DEVICE> LED_REMAP = { 4, 3, 0, 1, 2, 5, 6, 7, 8, 9 };

	bool isCurrentLightpack(const ProviderHid::HidDeviceInfo& device)
	{
		return device.vendorId == LIGHTPACK_VENDOR_ID && device.productId == LIGHTPACK_PRODUCT_ID;
	}

	bool isOldLightpack(const ProviderHid::HidDeviceInfo& device)
	{
		return device.vendorId == LIGHTPACK_OLD_VENDOR_ID && device.productId == LIGHTPACK_OLD_PRODUCT_ID;
	}
}

DriverHidLightpack::DriverHidLightpack(const QJsonObject& deviceConfig)
	: ProviderHid(deviceConfig)
{
}

bool DriverHidLightpack::init(QJsonObject deviceConfig)
{
	if (!ProviderHid::init(deviceConfig)) {
		return false;
	}

	_output = deviceConfig["output"].toString(deviceConfig["path"].toString("auto")).trimmed();

	// ProviderHid opens at most _maxDevices: as many chained Lightpacks as the LED count needs
	_maxDevices = std::max<size_t>(1, (static_cast<size_t>(_ledCount) + LEDS_PER_DEVICE - 1) / LEDS_PER_DEVICE);

	Debug(_log, "DeviceType: {:s}, LedCount: {:d}, HID output: {:s}, devices needed: {:d}",
		this->getActiveDeviceType(), this->getLedCount(), _output, static_cast<int>(_maxDevices));

	return true;
}

bool DriverHidLightpack::initDevice()
{
	const size_t devices = deviceCount();

	if (static_cast<size_t>(_ledCount) > devices * LEDS_PER_DEVICE)
	{
		Error(_log, "Configured for {:d} LEDs, but {:d} Lightpack device(s) provide only {:d}",
			static_cast<int>(_ledCount), static_cast<int>(devices), static_cast<int>(devices * LEDS_PER_DEVICE));
		return false;
	}

	// switch off the smoothing of the device itself
	Command cmd{};
	cmd[1] = CMD_SET_SMOOTH_SLOWDOWN;
	for (size_t index = 0; index < devices; ++index)
	{
		if (!writeReport(cmd.data(), cmd.size(), index))
		{
			Error(_log, "Could not configure Lightpack {:s}", describeDevice(index));
			return false;
		}
	}

	QString pluralSuffix;
	if (devices != 1) {
		pluralSuffix = QStringLiteral("s");
	}

	_customInfo = QString(" (%1 device%2)").arg(devices).arg(pluralSuffix);
	Info(_log, "Opened {:d} Lightpack device(s) for {:d} LEDs", static_cast<int>(devices), static_cast<int>(_ledCount));

	for (size_t index = 0; index < devices; ++index) {
		Info(_log, "Lightpack {:d}: {:s}", static_cast<int>(index + 1), describeDevice(index));
	}

	return true;
}

std::vector<ProviderHid::HidDeviceInfo> DriverHidLightpack::selectDevices(std::vector<HidDeviceInfo> found)
{
	found.erase(
		std::remove_if(found.begin(), found.end(), [](const HidDeviceInfo& device)
		{
			return !isCurrentLightpack(device) && !isOldLightpack(device);
		}),
		found.end());

	return found;
}

std::vector<ProviderHid::HidDeviceInfo> DriverHidLightpack::devicesToOpen(std::vector<HidDeviceInfo> supported)
{
	// a single device picked by its HID path
	const auto chosen = std::find_if(supported.begin(), supported.end(), [this](const HidDeviceInfo& device) { return device.path == _output; });
	if (chosen != supported.end()) {
		return std::vector<HidDeviceInfo>{ *chosen };
	}

	// The order of the devices is the order of the LEDs of the strip: current Lightpacks before the old ones,
	// then by serial number (devices without a serial number last, by path)
	std::sort(supported.begin(), supported.end(), [](const HidDeviceInfo& left, const HidDeviceInfo& right)
	{
		if (isCurrentLightpack(left) != isCurrentLightpack(right)) {
			return isCurrentLightpack(left);
		}

		if (left.serial.isEmpty() != right.serial.isEmpty()) {
			return !left.serial.isEmpty();
		}

		if (left.serial != right.serial) {
			return left.serial < right.serial;
		}

		return left.path < right.path;
	});

	return supported;
}

std::pair<bool, int> DriverHidLightpack::writeInfiniteColors(SharedOutputColors nonlinearRgbColors)
{
	if (nonlinearRgbColors->empty())
	{
		return { true, 0 };
	}

	std::vector<DeepColor> colors;
	colors.reserve(nonlinearRgbColors->size());

	for (const auto& color : *nonlinearRgbColors)
	{
		// 0.0-1.0 to 12bits
		colors.push_back(static_cast<DeepColor>(linalg::round(linalg::clamp(color, 0.0f, 1.0f) * 4095.0f)));
	}

	return { true, writeColors(colors) };
}

int DriverHidLightpack::writeColors(const std::vector<DeepColor>& ledValues)
{
	const size_t devices = deviceCount();

	if (ledValues.size() > (devices * LEDS_PER_DEVICE))
	{
		setInError(QString("Received %1 LED colors, but the connected Lightpacks support only %2")
				.arg(ledValues.size())
				.arg(devices * LEDS_PER_DEVICE)
		);
		return -1;
	}

	for (size_t deviceIndex = 0; deviceIndex < devices; ++deviceIndex)
	{
		Command cmd{}; // cmd[0] is the report ID (0)
		cmd[1] = CMD_UPDATE_LEDS;

		for (size_t led = 0; led < LEDS_PER_DEVICE; ++led)
		{
			const size_t source = deviceIndex * LEDS_PER_DEVICE + led;
			if (source >= ledValues.size()) {
				break;
			}

			const auto& color = ledValues[source];
			const size_t offset = 2 + LED_REMAP[led] * BYTES_PER_LED;
			cmd[offset+0] = static_cast<uint8_t>(color[0] >> 4);
			cmd[offset+1] = static_cast<uint8_t>(color[1] >> 4);
			cmd[offset+2] = static_cast<uint8_t>(color[2] >> 4);
			cmd[offset+3] = static_cast<uint8_t>(color[0] & 0x0f);
			cmd[offset+4] = static_cast<uint8_t>(color[1] & 0x0f);
			cmd[offset+5] = static_cast<uint8_t>(color[2] & 0x0f);
		}

		if (!writeReport(cmd.data(), cmd.size(), deviceIndex))
		{
			setInError(QString("Could not write to Lightpack %1").arg(describeDevice(deviceIndex)));
			setupRetry(2500);
			return -1;
		}
	}

	return 0;
}

QString DriverHidLightpack::describeDevice(size_t index) const
{
	const HidDeviceInfo& device = deviceInfo(index);

	if (device.serial.isEmpty()) {
		return device.path;
	}

	// interfaces of one composite device share the serial number: the path tells them apart
	for (size_t other = 0; other < deviceCount(); ++other)
	{
		if (other != index && deviceInfo(other).serial == device.serial) {
			return QString("%1 (%2)").arg(device.serial, device.path);
		}
	}

	return device.serial;
}

LedDevice* DriverHidLightpack::construct(const QJsonObject& deviceConfig)
{
	return new DriverHidLightpack(deviceConfig);
}

bool DriverHidLightpack::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("lightpack", "leds_group_6_HID", DriverHidLightpack::construct);
