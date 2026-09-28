/*
 * Copyright © 2026 Mark Pustjens
 * Written by Mark Pustjens <pustjens@dds.nl>
 */

#include <led-drivers/hid/DriverHidLightpack.h>
#include <linalg.h>

#ifndef PCH_ENABLED
	#include <algorithm>
	#include <cmath>
#endif

bool DriverHidLightpack::init_device (const Device& device)
{
	Command cmd{};
	cmd[1] = set_smoothing_command;
	QString error;
	if (!sendOutputReport(device, cmd, error)) {
		setInError(QString("Could not configure Lightpack %1: %2")
				.arg(device.serial, error));
		return false;
	}

	return true;
}


bool DriverHidLightpack::powerOff (const Device& device)
{
	Command cmd{};
	QString error;

	cmd[1] = update_led_command;

	if (!sendOutputReport(device, cmd, error))
	{
		Error (_log, "Could not power off Lightpack {:s}: {:s}",
				device.serial, error);
		return false;
	}

	return true;
}


int DriverHidLightpack::writeFiniteColors (
		const Device& device,
		std::span<const ColorRgb> ledValues)
{
	std::vector<DeepColor> colors;

	colors.reserve (ledValues.size ());

	for (const auto& color : ledValues)
	{
		/* We could also do ((col * 4095) / 255), but that is not as fast
		 * and we don't need the accuracy. */
		colors.push_back ( reorderColor ({
			static_cast<uint16_t>((color.red << 4) | (color.red >> 4)),
			static_cast<uint16_t>((color.green << 4) | (color.green >> 4)),
			static_cast<uint16_t>((color.blue << 4) | (color.blue >> 4))
		}, _colorOrder));
	}

	return writeColors (device, colors);
}


std::pair<bool, int> DriverHidLightpack::writeInfiniteColors (
		const Device& device,
		std::span<const linalg::aliases::float3> nonlinearRgbColors)
{
	const auto normalizeTo12Bit = [](float value) -> uint16_t {
		if (!std::isfinite(value)) {
			return 0;
		}
		/* 4095 is the maximum unsigned 12-bit value (2^12 - 1). */
		return static_cast<uint16_t> (std::lround(std::clamp(value, 0.0f, 1.0f) * 4095.0f));
	};

	std::vector<DeepColor> colors;

	colors.reserve(nonlinearRgbColors.size());

	for (const auto& color : nonlinearRgbColors)
	{
		colors.push_back(reorderColor({
			normalizeTo12Bit(color.x),
			normalizeTo12Bit(color.y),
			normalizeTo12Bit(color.z)
		}, _colorOrder));
	}
	return { true, writeColors (device, colors) };
}


int DriverHidLightpack::writeColors (
		const Device& device,
		std::span<const DeepColor> ledValues)
{
	if (ledValues.size() > lightpack_led_count)
	{
		setInError (QString ("Received %1 LED colors, but one Lightpack supports only %2")
				.arg (ledValues.size())
				.arg (lightpack_led_count)
		);
		return -1;
	}

	Command cmd{};
	cmd[1] = update_led_command;

	for (size_t led = 0; led < ledValues.size(); ++led)
	{
		const auto& color = ledValues[led];
		const size_t offset = 2 + led_remap[led] * bytes_per_led;

		cmd[offset+0] = static_cast<uint8_t>(color[0] >> 4);
		cmd[offset+1] = static_cast<uint8_t>(color[1] >> 4);
		cmd[offset+2] = static_cast<uint8_t>(color[2] >> 4);
		cmd[offset+3] = static_cast<uint8_t>(color[0] & 0x0f);
		cmd[offset+4] = static_cast<uint8_t>(color[1] & 0x0f);
		cmd[offset+5] = static_cast<uint8_t>(color[2] & 0x0f);
	}

	QString error;

	if (!sendOutputReport(device, cmd, error))
	{
		setInError (QString ("Could not write to Lightpack %1: %2")
				.arg (device.serial, error));
		return -1;
	}

	return 0;
}


LedDevice* DriverHidLightpack::construct (const QJsonObject& deviceConfig)
{
	return new DriverHidLightpack(deviceConfig);
}


inline
std::array<uint16_t, 3> DriverHidLightpack::reorderColor (std::array<uint16_t, 3> color, LedString::ColorOrder order)
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


bool DriverHidLightpack::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE(
		"lightpack",
		"leds_group_3_serial",
		DriverHidLightpack::construct);
