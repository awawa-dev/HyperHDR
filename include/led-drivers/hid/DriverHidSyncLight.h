#pragma once

#ifndef PCH_ENABLED
	#include <array>
	#include <QString>
	#include <cstdint>
	#include <string>
	#include <vector>
#endif

#include <led-drivers/hid/ProviderHid.h>


class DriverHidSyncLight : public ProviderHid
{
	public:

		explicit DriverHidSyncLight (const QJsonObject& deviceConfig)
		: ProviderHid (deviceConfig)
		{}

		~DriverHidSyncLight () override;

		static LedDevice* construct (const QJsonObject& deviceConfig);


	protected:

		bool init (QJsonObject deviceConfig) override;
		bool init_device (const Device& device) override;

		bool powerOn (const Device& device) override;
		bool powerOff (const Device& device) override;

		int writeFiniteColors (
				const Device& device,
				std::span<const ColorRgb> ledValues) final;

		[[nodiscard]]
		std::vector<DeviceId> getSupportedDeviceIds () const final
		{
			return {
				{ .vendorId=0x1a86, .productId=0xfe07 },
				{ .vendorId=0x1a86, .productId=0xfe0c },
			};
		}

		[[nodiscard]]
		size_t getLedCount (const Device& /*device*/) const final
		{
			return static_cast<size_t> (_controllerLedCount);
		}

		[[nodiscard]]
		std::string getDeviceName () const final
		{
			return "SyncLight";
		}


	private:

		enum class OutputMode
		{
			Global,
			PerLed
		};

		static constexpr size_t report_size = 64;
		static constexpr size_t rb_overhead = 6;
		static constexpr int sc_header_size = 5;
		static constexpr int sc_record_size = 5;
		static constexpr int sc_footer_size = 1;
		static constexpr int sc_checksum_size = 1;
		static constexpr int default_controller_led_count = 65;
		static constexpr size_t maximum_led_count = 254;
		static constexpr uint8_t action_color = 0x86;
		static constexpr uint8_t action_brightness = 0x87;
		static constexpr uint8_t action_keepalive = 0x97;
		static constexpr uint8_t section_global = 1;

		static uint8_t checksum (std::span<const uint8_t> data);
		static bool buildRbFrame (
				HidReport& report,
				uint8_t action,
				std::span<const uint8_t> payload,
				uint8_t id);
		static std::vector<uint8_t> buildScFrame (
				std::span<const ColorRgb> ledValues,
				int totalLedCount,
				int controllerLedCount,
				uint8_t id);
		static std::array<uint8_t, 10> buildSectionPayload (
				uint8_t section,
				uint8_t red,
				uint8_t green,
				uint8_t blue);
		static ColorRgb averageColor (std::span<const ColorRgb> ledValues, int ledCount);
		static ColorRgb averageColorRange (
				std::span<const ColorRgb> ledValues,
				int offset,
				int count);

		bool writeReport (const Device& device, const HidReport& report);
		bool sendRb (const Device& device, const HidReport& report);
		bool sendKeepalive (const Device& device);
		bool sendAveragedSectionColor (
				const Device& device,
				std::span<const ColorRgb> ledValues,
				int totalLedCount);
		bool sendScColors (
				const Device& device,
				std::span<const ColorRgb> ledValues,
				int totalLedCount);
		bool sendBlackFrame (const Device& device);
		bool sendBrightness (const Device& device, uint8_t value);

		uint8_t nextId ();

		uint8_t _idCounter = 0;
		uint8_t _brightness = 0xff;
		int _totalLedCount = 0;
		int _controllerLedCount = default_controller_led_count;
		OutputMode _outputMode = OutputMode::Global;

		static bool isRegistered;
};
