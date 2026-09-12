/*
 * Copyright © 2026 Mark Pustjens
 * Written by Mark Pustjens <pustjens@dds.nl>
 */

#include <led-drivers/hid/DriverHidLightpack.h>
#include <linalg.h>

#ifndef PCH_ENABLED
	#include <QJsonArray>
	#include <QJsonDocument>
	#include <algorithm>
	#include <cmath>
	#include <set>
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

	struct LightpackDescriptor
	{
		std::string path;
		QString serial;
		uint16_t vendorId;
		uint16_t productId;
	};

	inline bool ensureHidInitialized()
	{
		return hid_init() == 0;
	}

	QString fromWide(const wchar_t* value)
	{
		if (value == nullptr) {
			return {};
		}
		return QString::fromWCharArray(value);
	}

	QString hidError(hid_device* handle)
	{
		const wchar_t* error = hid_error(handle);

		if (error == nullptr) {
			return QStringLiteral("unknown HID error");
		}

		return QString::fromWCharArray(error);
	}

	void appendDescriptors(
			std::vector<LightpackDescriptor>& result,
			std::set<std::string>& paths,
			uint16_t vendorId,
			uint16_t productId)
	{
		std::vector<LightpackDescriptor> found;
		hid_device_info* devices = hid_enumerate(vendorId, productId);

		for (hid_device_info* current = devices; current != nullptr; current = current->next)
		{
			if (current->path == nullptr || current->interface_number > 0 || !paths.insert(current->path).second) {
				continue;
			}

			found.push_back({ current->path, fromWide(current->serial_number), vendorId, productId });
		}
		hid_free_enumeration(devices);

		std::sort(found.begin(), found.end(), [](const LightpackDescriptor& left, const LightpackDescriptor& right)
		{
			if (left.serial.isEmpty() != right.serial.isEmpty()) {
				return !left.serial.isEmpty();
			}

			if (left.serial.isEmpty()) {
				return left.path < right.path;
			}

			return left.serial < right.serial;
		});

		result.insert(result.end(), found.begin(), found.end());
	}

	std::vector<LightpackDescriptor> enumerateLightpacks()
	{
		std::vector<LightpackDescriptor> result;
		if (!ensureHidInitialized()) {
			return result;
		}

		std::set<std::string> paths;
		appendDescriptors(result, paths, LIGHTPACK_VENDOR_ID, LIGHTPACK_PRODUCT_ID);
		appendDescriptors(result, paths, LIGHTPACK_OLD_VENDOR_ID, LIGHTPACK_OLD_PRODUCT_ID);
		return result;
	}

	std::array<uint16_t, 3> reorderColor(std::array<uint16_t, 3> color, LedString::ColorOrder order)
	{
		switch (order)
		{
			case LedString::ColorOrder::ORDER_RBG:
				return { color[0], color[2], color[1] };
			case LedString::ColorOrder::ORDER_GRB:
				return { color[1], color[0], color[2] };
			case LedString::ColorOrder::ORDER_BRG:
				return { color[2], color[0], color[1] };
			case LedString::ColorOrder::ORDER_GBR:
				return { color[1], color[2], color[0] };
			case LedString::ColorOrder::ORDER_BGR:
				return { color[2], color[1], color[0] };
			case LedString::ColorOrder::ORDER_RGB:
				return color;
		}
		return color;
	}
}

DriverHidLightpack::DriverHidLightpack(const QJsonObject& deviceConfig)
	: LedDevice(deviceConfig)
	, serial(QStringLiteral("all"))
{
}

DriverHidLightpack::~DriverHidLightpack()
{
	close();
}

bool DriverHidLightpack::init(QJsonObject deviceConfig)
{
	if (!LedDevice::init(deviceConfig)) {
		return false;
	}

	serial = deviceConfig["serial"].toString(QStringLiteral("all")).trimmed();

	if (serial.isEmpty()) {
		serial = QStringLiteral("all");
	}

	if (!ensureHidInitialized()) {
		Error(_log, "Could not initialize hidapi");
		return false;
	}

	return true;
}

int DriverHidLightpack::open()
{
	close();
	const auto descriptors = enumerateLightpacks();

	for (const auto& descriptor : descriptors)
	{
		if (serial != QStringLiteral("all") && descriptor.serial != serial) {
			continue;
		}

		hid_device* handle = hid_open_path(descriptor.path.c_str());
		if (handle == nullptr) {
			setInError(QString("Could not open Lightpack %1: %2").arg(descriptor.serial, hidError(nullptr)));
			setupRetry(2500);
			return -1;
		}

		devices.push_back({
				handle,
				descriptor.serial,
				descriptor.path
		});
	}

	if (devices.empty())
	{
		if (serial == QStringLiteral("all")) {
			setInError(QStringLiteral("No Lightpack devices were found"));
		} else {
			setInError(QString("Lightpack with serial %1 was not found").arg(serial));
		}

		setupRetry(2500);
		return -1;
	}

	if (_ledCount > devices.size() * LEDS_PER_DEVICE)
	{
		setInError(QString("Configured for %1 LEDs, but %2 Lightpack device(s) provide only %3")
				.arg(_ledCount)
				.arg(devices.size())
				.arg(devices.size() * LEDS_PER_DEVICE)
		);

		if (serial == QStringLiteral("all")) {
			setupRetry(2500);
		}

		return -1;
	}

	Command cmd{};
	cmd[1] = CMD_SET_SMOOTH_SLOWDOWN;
	QString error;
	for (const auto& device : devices)
	{
		if (!sendCommand(device, cmd, error))
		{
			setInError(QString("Could not configure Lightpack %1: %2").arg(device.serial, error));
			setupRetry(2500);
			return -1;
		}
	}

	_isDeviceReady = true;

	QString pluralSuffix;
	if (devices.size() != 1) {
		pluralSuffix = QStringLiteral("s");
	}

	_customInfo = QString(" (%1 device%2)").arg(devices.size()).arg(pluralSuffix);
	Info(_log, "Opened {:d} Lightpack device(s) for {:d} LEDs", devices.size(), _ledCount);

	for (size_t index = 0; index < devices.size(); ++index) {
		Info(_log, "Lightpack {:d}: serial {:s}", index + 1, devices[index].serial);
	}

	return 0;
}

int DriverHidLightpack::close()
{
	_isDeviceReady = false;

	for (const auto& device : devices)
	{
		if (device.handle != nullptr) {
			hid_close(device.handle);
		}
	}

	devices.clear();

	return 0;
}

bool DriverHidLightpack::powerOff()
{
	if (devices.empty()) {
		return true;
	}

	Command cmd{};
	cmd[1] = CMD_UPDATE_LEDS;
	QString error;
	bool success = true;

	for (const auto& device : devices) {
		if (!sendCommand(device, cmd, error)) {
			Error(_log, "Could not power off Lightpack {:s}: {:s}", device.serial, error);
			success = false;
		}
	}
	return success;
}

int DriverHidLightpack::writeFiniteColors(const std::vector<ColorRgb>& ledValues)
{
	std::vector<DeepColor> colors;
	colors.reserve(ledValues.size());

	for (const auto& color : ledValues)
	{
		colors.push_back(reorderColor({
			static_cast<uint16_t>((color.red << 4) | (color.red >> 4)),
			static_cast<uint16_t>((color.green << 4) | (color.green >> 4)),
			static_cast<uint16_t>((color.blue << 4) | (color.blue >> 4))
		}, _colorOrder));
	}

	return writeColors(colors);
}

std::pair<bool, int> DriverHidLightpack::writeInfiniteColors(SharedOutputColors nonlinearRgbColors)
{
	const auto normalizeTo12Bit = [](float value) -> uint16_t {
		if (!std::isfinite(value)) {
			return 0;
		}
		// 4095 is the maximum unsigned 12-bit value (2^12 - 1).
		return static_cast<uint16_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 4095.0f));
	};

	std::vector<DeepColor> colors;
	colors.reserve(nonlinearRgbColors->size());
	for (const auto& color : *nonlinearRgbColors)
	{
		colors.push_back(reorderColor({
			normalizeTo12Bit(color.x),
			normalizeTo12Bit(color.y),
			normalizeTo12Bit(color.z)
		}, _colorOrder));
	}
	return { true, writeColors(colors) };
}

int DriverHidLightpack::writeColors(const std::vector<DeepColor>& ledValues)
{
	if (ledValues.size() > (devices.size() * LEDS_PER_DEVICE))
	{
		setInError(QString("Received %1 LED colors, but the connected Lightpacks support only %2")
				.arg(ledValues.size())
				.arg(devices.size() * LEDS_PER_DEVICE)
		);
		return -1;
	}

	QString error;
	for (size_t deviceIndex = 0; deviceIndex < devices.size(); ++deviceIndex)
	{
		Command cmd{};
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

		if (!sendCommand(devices[deviceIndex], cmd, error))
		{
			const QString deviceSerial = devices[deviceIndex].serial;
			setInError(QString("Could not write to Lightpack %1: %2").arg(deviceSerial, error));
			setupRetry(2500);
			return -1;
		}
	}

	return 0;
}

bool DriverHidLightpack::sendCommand(const Device& device, const Command& cmd, QString& error) const
{
	const int written = hid_send_output_report(device.handle, cmd.data(), cmd.size());

	if (written != static_cast<int>(cmd.size())) {
		error = hidError(device.handle);
		return false;
	}
	return true;
}

void DriverHidLightpack::setInError(const QString& errorMsg)
{
	close();
	LedDevice::setInError(errorMsg);
}

QJsonObject DriverHidLightpack::discover(const QJsonObject& /*params*/)
{
	QJsonArray deviceList;
	const auto descriptors = enumerateLightpacks();

	if (!descriptors.empty())
	{
		deviceList.push_back(QJsonObject{
			{
				"value",
				"all"
			},
			{
				"name",
				QString("All detected Lightpacks (%1 devices, %2 LEDs)")
					.arg(descriptors.size())
					.arg(descriptors.size() * LEDS_PER_DEVICE)
			}
		});
	}

	for (const auto& descriptor : descriptors)
	{
		QString value = descriptor.serial;
		if (value.isEmpty()) {
			value = QString::fromStdString(descriptor.path);
		}

		deviceList.push_back(QJsonObject{
			{
				"value",
				value
			},
			{
				"name",
				QString("Lightpack %1 (%2:%3)")
					.arg(value)
					.arg(descriptor.vendorId, 4, 16, QLatin1Char('0'))
					.arg(descriptor.productId, 4, 16, QLatin1Char('0')) },
			{
				"serialNumber",
				descriptor.serial
			},
			{
				"path",
				QString::fromStdString(descriptor.path)
			}
		});
	}

	QJsonObject result{
		{
			"ledDeviceType",
			_activeDeviceType
		},
		{
			"devices",
			deviceList
		}
	};

	Debug(_log, "Lightpack devices discovered: [{:s}]", QString(QJsonDocument(result).toJson(QJsonDocument::Compact)));

	return result;
}

LedDevice* DriverHidLightpack::construct(const QJsonObject& deviceConfig)
{
	return new DriverHidLightpack(deviceConfig);
}

bool DriverHidLightpack::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE(
		"lightpack",
		"leds_group_3_serial",
		DriverHidLightpack::construct
	);
