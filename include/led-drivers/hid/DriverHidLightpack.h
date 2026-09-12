/*
 * Copyright © 2026 Mark Pustjens
 * Written by Mark Pustjens <pustjens@dds.nl>
 */

#pragma once

#ifndef PCH_ENABLED
	#include <QString>
	#include <array>
	#include <cstdint>
	#include <string>
	#include <vector>
#endif

#include <hidapi.h>
#include <led-drivers/LedDevice.h>

/* LED device driver for one or more USB HID Lightpack devices.
 *
 * Based on the original driver that was part of the now-defunct
 * Prismatik software.
 *
 * See https://github.com/psieg/Lightpack
 */
class DriverHidLightpack : public LedDevice
{
public:

	/* Constructor
	 *
	 * @param[in] deviceConfig Device configuration.
	 */
	explicit DriverHidLightpack(const QJsonObject& deviceConfig);


	/* Destructor. */
	~DriverHidLightpack() override;


	/* Construct a `DriverHidLightpack` instance.
	 *
	 * @param[in] deviceConfig Device configuration.
	 * @returns Constructed instance.
	 */
	static LedDevice* construct(const QJsonObject& deviceConfig);


	/* Discovers connected Lightpack devices.
	 *
	 * @param[in] params Discovery parameters.
	 * @returns JSON description of the discovered devices.
	 */
	QJsonObject discover(const QJsonObject& params) override;


protected:

	/* Initializes the Lightpack configuration and HID library.
	 *
	 * @param[in] deviceConfig Device configuration.
	 * @returns True on success, false otherwise.
	 */
	bool init(QJsonObject deviceConfig) override;


	/* Opens the configured Lightpack devices.
	 *
	 * @returns 0 on success, negative otherwise.
	 */
	int open() override;


	/* Closes all open Lightpack devices.
	 *
	 * @returns Zero on success.
	 */
	int close() override;


	/* Turns off every LED on each open Lightpack device.
	 *
	 * @returns True on success, otherwise false.
	 */
	bool powerOff() override;


	/* Send new colors to the device.
	 *
	 * For the Lightpack device, this converts 8-bit RGB values
	 * to 12-bit values and writes them to the devices.
	 *
	 * @param[in] ledValues RGB color for each LED.
	 * @returns Zero on success, otherwise negative.
	 */
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;


	/* Send new colors to the device.
	 *
	 * For the Lightpack device, this converts normalized RGB
	 * values to 12-bit values and writes them to the devices.
	 *
	 * @param[in] nonlinearRgbColors Normalized RGB color for each LED.
	 * @returns A handled flag and the write status.
	 */
	std::pair<bool, int> writeInfiniteColors(SharedOutputColors nonlinearRgbColors) override;


	/* Close all devices, then set the driver error state.
	 *
	 * @param[in] errorMsg Error description.
	 */
	void setInError(const QString& errorMsg) override;


private:

	/* Type to hold identifying information of a Lightpack device. */
	struct Device
	{
		/* Native HID device handle. */
		hid_device* handle;

		/* Device serial number. */
		QString serial;

		/* Platform-specific HID path. */
		std::string path;
	};

	/* A 12-bit RGB color stored in 16-bit channels. */
	using DeepColor = std::array<uint16_t, 3>;

	/* A fixed-size Lightpack HID command. */
	using Command = std::array<uint8_t, 65>;


	/* Writes 12-bit RGB values across the open Lightpack devices.
	 *
	 * @param[in] ledValues RGB color for each LED.
	 * @returns Zero on success, otherwise negative.
	 */
	int writeColors(const std::vector<DeepColor>& ledValues);


	/* Sends a command to a Lightpack device.
	 *
	 * @param[in] device Lightpack device.
	 * @param[in] command Command to send.
	 * @param[out] error Error description when sending fails.
	 * @returns True when the complete command was written, otherwise false.
	 */
	bool sendCommand(const Device& device, const Command& command, QString& error) const;


	/* Configured serial number, or "all" to use every discovered device. */
	QString serial;


	/* List of open Lightpack devices. */
	std::vector<Device> devices;


	/* Use to register this driver with the LED device factory. */
	static bool isRegistered;
};
