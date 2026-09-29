/*
 * Copyright © 2026 Mark Pustjens
 * Written by Mark Pustjens <pustjens@dds.nl>
 */

#pragma once

#ifndef PCH_ENABLED
	#include <QString>
	#include <array>
	#include <cstdint>
	#include <vector>
#endif

#include <led-drivers/hid/ProviderHid.h>


/* LED device driver for one or more USB HID Lightpack devices.
 *
 * Based on the original driver that was part of the now-defunct
 * Prismatik software.
 *
 * See https://github.com/psieg/Lightpack
 */
class DriverHidLightpack : public ProviderHid
{
	public:

		/* Constructor
		 *
		 * @param deviceConfig Device configuration.
		 */
		explicit DriverHidLightpack (const QJsonObject& deviceConfig)
		: ProviderHid (deviceConfig)
		{}


		/* Destructor. */
		~DriverHidLightpack() override = default;


		/* Construct a `DriverHidLightpack` instance.
		 *
		 * @param[in] deviceConfig Device configuration.
		 * @returns Constructed instance.
		 */
		static LedDevice* construct (const QJsonObject& deviceConfig);


	protected:

		/* Configures one open Lightpack device.
		 *
		 * This driver disables hardware smoothing, so that
		 * `LedDevice` can take care of that.
		 *
		 * @param device Open Lightpack device.
		 * @returns True on success, false otherwise.
		 */
		[[nodiscard]]
		bool init_device (const Device& device) override;


		/* Returns the LED capacity of one Lightpack device.
		 *
		 * @param device Open Lightpack device.
		 * @returns Number of LEDs supported by the device.
		 */
		[[nodiscard]]
		size_t getLedCount (const Device& device) const final
		{
			return lightpack_led_count;
		}


		/* Returns the Lightpack label used for discovery and logging. */
		[[nodiscard]]
		std::string getDeviceName () const final
		{
			return "Lightpack";
		}


		/* Turns off every LED off.
		 *
		 * @param device Open Lightpack device.
		 * @returns True on success, otherwise false.
		 */
		bool powerOff (const Device& device) override;


		/* Send new colors to the device.
		 *
		 * For the Lightpack device, this converts normalized RGB
		 * values to 12-bit values and writes them to the device.
		 *
		 * @param device Lightpack device.
		 * @param nonlinearRgbColors Normalized RGB colors assigned to the device.
		 * @returns A handled flag and the write status.
		 */
		std::pair<bool, int> writeInfiniteColors (
				const Device& device,
				std::span<const linalg::aliases::float3> nonlinearRgbColors) final;


		/**
		 * Set the LED colors using `uint8_t` colors.
		 *
		 * For the Lightpack device, this converts 24-bit RGB values
		 * to 36-bit RGB values and writes them to the devices.
		 *
		 * @param device Lightpack device.
		 * @param ledValues 24-bit RGB colors assigned to the device.
		 * @returns 0 on success, -1 otherwise.
		 */
		int writeFiniteColors (
				const Device& device,
				std::span<const ColorRgb> ledValues) final;


		/**
		 * Returns the device ids supported by the Lightpack driver.
		 */
		[[nodiscard]]
		std::vector<DeviceId> getSupportedDeviceIds () const final
		{
			return {
				{ .vendorId=0x1d50, .productId=0x6022 },
				{ .vendorId=0x03eb, .productId=0x204f },
			};
		}


	private:

		/* A 12-bit RGB color stored in 16-bit channels. */
		using DeepColor = std::array<uint16_t, 3>;


		/* A fixed-size Lightpack HID command. */
		using Command = std::array<uint8_t, 65>;


		/* Number of leds on a Lightpack device. */
		static constexpr auto lightpack_led_count = 10;


		/* Lightpack use 16 bits to store 12 bits colors per channel. */
		static constexpr size_t bytes_per_led = 6;


		/* The numbering the Lightpack firmware does not match the ordering
		 * on the hardware. We need to remap leds. */
		static constexpr std::array<size_t, lightpack_led_count> led_remap = { 4, 3, 0, 1, 2, 5, 6, 7, 8, 9 };


		/* Command to update LED colors. */
		static constexpr uint8_t update_led_command = 0x01;


		/* Command to configure hardware smoothing. */
		static constexpr uint8_t set_smoothing_command = 0x05;


		/* Use to register this driver with the LED device factory. */
		static bool isRegistered;


		/* Writes 12-bit RGB values to one open Lightpack device.
		 *
		 * @param device Open Lightpack device.
		 * @param ledValues RGB color for each LED.
		 * @returns Zero on success, otherwise negative.
		 */
		int writeColors (const Device& device, std::span<const DeepColor> ledValues);

};
