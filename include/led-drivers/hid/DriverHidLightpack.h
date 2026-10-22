#pragma once

#ifndef PCH_ENABLED
	#include <QString>
	#include <array>
	#include <cstdint>
	#include <utility>
	#include <vector>
#endif

#include <led-drivers/hid/ProviderHid.h>

/* LED device driver for one or more USB HID Lightpack devices.
 *
 * Enumerating, opening, closing and writing HID reports is done by
 * ProviderHid. Chained Lightpacks form
 * one strip: 10 LEDs per device, devices ordered by serial number.
 */
class DriverHidLightpack : public ProviderHid
{
public:

	/* Constructor
	 *
	 * @param[in] deviceConfig Device configuration.
	 */
	explicit DriverHidLightpack(const QJsonObject& deviceConfig);


	/* Construct a `DriverHidLightpack` instance.
	 *
	 * @param[in] deviceConfig Device configuration.
	 * @returns Constructed instance.
	 */
	static LedDevice* construct(const QJsonObject& deviceConfig);


protected:

	/* Reads the Lightpack configuration and decides how many devices are needed.
	 *
	 * @param[in] deviceConfig Device configuration.
	 * @returns True on success, false otherwise.
	 */
	bool init(QJsonObject deviceConfig) override;


	/* Called by ProviderHid right after the devices were opened.
	 *
	 * Checks that enough Lightpacks were found for the configured LED count
	 * and switches off their built-in smoothing.
	 *
	 * @returns True on success, false otherwise.
	 */
	bool initDevice() override;

	/* Send new colors to the device.
	 *
	 * For the Lightpack device, this converts normalized RGB
	 * values to 12-bit values and writes them to the devices.
	 *
	 * @param[in] nonlinearRgbColors Normalized RGB color for each LED.
	 * @returns A handled flag and the write status.
	 */
	std::pair<bool, int> writeInfiniteColors(SharedOutputColors nonlinearRgbColors) override;


	/* Keeps the HID devices that are Lightpacks
	 *
	 * @param[in] found Every HID device present in the system.
	 * @returns The supported Lightpack devices.
	 */
	std::vector<HidDeviceInfo> selectDevices(std::vector<HidDeviceInfo> found) override;


	/* Chooses the Lightpacks to open and their order, which is the order of the LEDs.
	 *
	 * The device whose HID path is set in "output" wins. Otherwise every Lightpack
	 * ordered by serial number and by path.
	 *
	 * @param[in] supported Result of selectDevices().
	 * @returns The devices to open.
	 */
	std::vector<HidDeviceInfo> devicesToOpen(std::vector<HidDeviceInfo> supported) override;


private:

	/* A 12-bit RGB color stored in 16-bit channels. */
	using DeepColor = linalg::vec<uint16_t, 3>;

	/* A fixed-size Lightpack HID command: report ID, command, 64 data bytes. */
	using Command = std::array<uint8_t, 65>;


	/* Human readable name of an opened device for the logs: its serial number,
	 * with the HID path added when another opened device shares that serial
	 * (interfaces of one composite device), or the path alone without a serial.
	 *
	 * @param[in] index Index of the opened device.
	 * @returns The name.
	 */
	QString describeDevice(size_t index) const;


	/* Writes 12-bit RGB values across the open Lightpack devices.
	 *
	 * @param[in] ledValues RGB color for each LED.
	 * @returns Zero on success, otherwise negative.
	 */
	int writeColors(const std::vector<DeepColor>& ledValues);


	/* Configured HID path of a single Lightpack, or "auto". */
	QString _output;


	/* Use to register this driver with the LED device factory. */
	static bool isRegistered;
};
