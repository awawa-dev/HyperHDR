/*
 * Copyright © 2026 Mark Pustjens
 * Written by Mark Pustjens <pustjens@dds.nl>
 */

#pragma once


#include "infinite-color-engine/SharedOutputColors.h"
#ifndef PCH_ENABLED
	#include <cstdint>
	#include <optional>
	#include <QString>
	#include <string>
	#include <vector>
#endif

#include <span>
#include <hidapi.h>
#include <led-drivers/LedDevice.h>


/**
 * Provides shared HIDAPI functionality for HID LED drivers.
 *
 * @see LedDevice
 * @see https://libusb.info/hidapi/
 */
class ProviderHid : public LedDevice
{
	public:

		/**
		 * Constructor.
		 *
		 * @param deviceConfig device configuration.
		 */
		explicit ProviderHid (const QJsonObject& deviceConfig);


		/**
		 * Destructor.
		 *
		 * @details Closes all HID handles owned by the provider.
		 */
		~ProviderHid () override;


		/**
		 * Discover/enumerate supported devices.
		 *
		 * @param params Discovery parameters.
		 * @return JSON description of the discovered devices.
		 */
		QJsonObject discover (const QJsonObject& params) final;


	protected:

		/**
		 * Identifies supported HID devices.
		 */
		struct DeviceId
		{
			/* USB vendor identifier. */
			uint16_t vendorId;

			/* USB product identifier. */
			uint16_t productId;
		};


		/**
		 * Describes a HID device.
		 *
		 * @see `hid_device_info`
		 */
		struct Device
		{
			/* Platform-specific HID path. */
			std::string path;

			/* USB vendor identifier reported by HIDAPI. */
			uint16_t vendorId;

			/* USB product identifier reported by HIDAPI. */
			uint16_t productId;

			/* Device serial number, or an empty string when unavailable. */
			QString serial;

			/* HIDAPI device handle. */
			hid_device* handle;
		};


		/**
		 * Initialize `LedDevice` and HIDAPI.
		 *
		 * @param deviceConfig device configuration.
		 * @return True when initialization succeed, false otherwise.
		 */
		bool init (QJsonObject deviceConfig) override;


		/**
		 * Initalize a single HID device.
		 *
		 * Child classes can overload this if they need to send commands
		 * to the device to initialize it.
		 *
		 * @param device The device.
		 * @return `true`.
		 */
		virtual bool init_device (const Device& /*device*/)
		{
			return true;
		}


		/**
		 * Opens all found devices matching the supported vendor and
		 * product IDs.
		 *
		 * If any matching device cannot be opened, all handles opened
		 * by this call are closed and the provider enters the error state.
		 *
		 * @return `0` on success; otherwise a negative value.
		 */
		int open () final;


		/**
		 * Open a single HID device.
		 *
		 * @param path The device path.
		 * @return the device, or nothing on failure.
		 */
		std::optional<Device> open (const Device& deviceInfo);


		/**
		 * Closes all open HID devices.
		 *
		 * @return `0`.
		 */
		int close () final;


		/**
		 * Powers on every open HID device.
		 *
		 * @return `true` when every device was powered on successfully.
		 */
		bool powerOn () final;


		/**
		 * Powers on one HID device.
		 *
		 * Child classes can override this to perform device-specific
		 * power-on handling. The default implementation does nothing.
		 *
		 * @param device Open HID device.
		 * @return `true` on success.
		 */
		virtual bool powerOn (const Device& /*device*/)
		{
			return true;
		}


		/**
		 * Powers off every open HID device.
		 *
		 * @return `true` when every device was powered off successfully.
		 */
		bool powerOff () final;


		/**
		 * Powers off one HID device.
		 *
		 * Child classes can override this to perform device-specific
		 * power-off handling. The default implementation does nothing.
		 *
		 * @param device Open HID device.
		 * @return `true` on success.
		 */
		virtual bool powerOff (const Device& /*device*/)
		{
			return true;
		}


		/**
		 * Set the LED colors using floating point valued colors.
		 *
		 * Colors are sent to each device, until non colors or devices
		 * are left. Child classes must implement `writeFiniteColors(device, ...)`
		 *
		 * `LedDevice` falls back to `writeFiniteColors()`
		 * when a child reports that infinite colors are not handled.
		 *
		 * @param nonlinearRgbColors Vector of float3 colors.
		 * @return Handled flag and write status.
		 */
		std::pair<bool, int> writeInfiniteColors (SharedOutputColors nonlinearRgbColors) final;


		/**
		 * Sets floating-point colors on one HID device.
		 *
		 * @param device Open HID device.
		 * @param nonlinearRgbColors Colors assigned to this device.
		 * @return Handled flag and write status.
		 */
		virtual std::pair<bool, int> writeInfiniteColors (
				const Device& /*device*/,
				std::span<const linalg::aliases::float3> /*nonlinearRgbColors*/)
		{
			return {false, 0};
		}


		/**
		 * Set the LED colors using uint8_t colors.
		 *
		 * Colors are partitioned by device capacity and passed to the
		 * per-device overload.
		 *
		 * @param ledValues RGB color for each LED.
		 * @return Zero on success, otherwise a negative value.
		 */
		int writeFiniteColors (const std::vector<ColorRgb>& ledValues) final;


		/**
		 * Sets 8-bit colors on one HID device.
		 *
		 * @param device Open HID device.
		 * @param ledValues Colors assigned to this device.
		 * @return Zero on success, otherwise a negative value.
		 */
		virtual int writeFiniteColors (
				const Device& device,
				std::span<const ColorRgb> ledValues) = 0;


		/**
		 * Returns the HID device models supported by the driver.
		 *
		 * Used for enumerating devices.
		 *
		 * @return Supported vendor and product id pairs.
		 */
		[[nodiscard]]
		virtual std::vector<DeviceId> getSupportedDeviceIds () const = 0;


		/**
		 * Returns the LED capacity of an open HID device.
		 *
		 * @param device Open HID device and its identifying metadata.
		 * @return Number of LEDs supported by the device.
		 */
		[[nodiscard]]
		virtual size_t getLedCount (const Device& device) const = 0;


		/**
		 * Returns the device label used for discovery and logging.
		 *
		 * @return Human-readable device identification.
		 */
		[[nodiscard]]
		virtual std::string getDeviceName () const = 0;


		/**
		 * Closes all HID devices and enters the error state.
		 *
		 * @param errorMsg Error description.
		 */
		void setInError (const QString& errorMsg) final;


		/**
		 * Enumerate supported devices.
		 *
		 * Results are sorted by serialnumber, then device path.
		 *
		 * @return A list of found devices, or `std::nullopt` on failure.
		 */
		[[nodiscard]]
		std::optional<std::vector<Device>> enumerate () const;


		/**
		 * Enumerate supported devices matching a vendor and product id.
		 *
		 * @param info Enumerated devices are added to this.
		 * @param DeviceId The structure holding the vid and pid.
		 * @return `true` on success, `false` otherwise.
		 */
		[[nodiscard]]
		bool enumerateForDeviceId (std::vector<Device>& info, const DeviceId& id) const;


		/**
		 * Writes a report to the device.
		 *
		 * @see `hid_write()` for details.
		 *
		 * @param device Open HID device.
		 * @param report Report data to write.
		 * @param error Error description when complete write fails.
		 * @return `true` when every byte was written, false otherwise.
		 */
		static bool write (
				const Device& device,
				std::span<const uint8_t> report,
				QString& error)
		{
			return writeReport (device, report, error, false);
		}


		/**
		 * Write an output report through the control endpoint.
		 *
		 * @see `hid_send_output_report()`
		 *
		 * @param device Open HID device.
		 * @param report Report data to write.
		 * @param error Error description when complete write fails.
		 * @return `true` when every byte was written, false otherwise.
		 */
		static bool sendOutputReport (
				const Device& device,
				std::span<const uint8_t> report,
				QString& error)
		{
			return writeReport (device, report, error, true);
		}


		/**
		 * Returns the last HIDAPI error.
		 *
		 * @return HIDAPI error text, or a fallback when none is available.
		 */
		[[nodiscard]]
		static QString getHidError ();


		/**
		 * Returns the last HIDAPI error for a device.
		 *
		 * @param device Open HID device.
		 * @return HIDAPI error text, or a fallback when none is available.
		 */
		[[nodiscard]]
		static QString getHidError (const Device& device);


	private:


		/* 'serialnumber' to use all devices, instead of a single one. */
		static const QString all_devices_serial;


		/* Retry interval to use when initialization fails. */
		static constexpr int retry_interval = 2500; /* ms */


		/* Configured serialnumber, or `all_devices` to use every
		 * discovered device. */
		QString serial = all_devices_serial;


		/* Open HID handles and their metadata. */
		std::vector<Device> open_devices;


		/**
		 * Write or send a report to the device.
		 *
		 * @see `hid_write()` and `hid_send_output_report()`.
		 *
		 * @param device Open HID device.
		 * @param report Report data to write.
		 * @param error Error description when complete write fails.
		 * @param send false will call `hid_write()`, and true will
		 *   call `hid_send_output_report()`.
		 * @return `true` when every byte was written, false otherwise.
		 */
		static bool writeReport (
				const Device& device,
				std::span<const uint8_t> report,
				QString& error,
				bool send);

};
