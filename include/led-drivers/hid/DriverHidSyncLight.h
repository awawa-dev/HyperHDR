#pragma once

#ifndef PCH_ENABLED
	#include <QByteArray>
	#include <QString>
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
			return maximum_led_count;
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

		//TODO: since report_size is fixed, we can introduce a type for it, instead of using qbytearray. also, the real report size is 64+1
		static constexpr int report_size = 64;
		static constexpr int rb_overhead = 6;
		static constexpr int sc_header_size = 5;
		static constexpr int sc_record_size = 5;
		static constexpr int sc_footer_size = 1;
		static constexpr int sc_checksum_size = 1;
		static constexpr int default_controller_led_count = 65;
		static constexpr size_t maximum_led_count = 254;
		static constexpr quint8 action_color = 0x86;
		static constexpr quint8 action_brightness = 0x87;
		static constexpr quint8 action_keepalive = 0x97;
		static constexpr quint8 section_global = 1;

		static quint8 checksum (const QByteArray& frame);
		static QByteArray buildRbFrame (quint8 action, const QByteArray& payload, quint8 id);
		static QByteArray buildScFrame (
				std::span<const ColorRgb> ledValues,
				int totalLedCount,
				int controllerLedCount,
				quint8 id);
		static QByteArray buildReport (const QByteArray& frame);
		static QByteArray buildSectionPayload (
				quint8 section,
				quint8 red,
				quint8 green,
				quint8 blue);
		static ColorRgb averageColor (std::span<const ColorRgb> ledValues, int ledCount);
		static ColorRgb averageColorRange (
				std::span<const ColorRgb> ledValues,
				int offset,
				int count);

		bool writeReport (const Device& device, const QByteArray& report);
		bool sendRb (const Device& device, quint8 action, const QByteArray& payload);
		bool sendAveragedSectionColor (
				const Device& device,
				std::span<const ColorRgb> ledValues,
				int totalLedCount);
		bool sendScColors (
				const Device& device,
				std::span<const ColorRgb> ledValues,
				int totalLedCount);
		bool sendBlackFrame (const Device& device);
		bool sendBrightness (const Device& device, quint8 value);

		quint8 nextId ();

		quint8 _idCounter = 0;
		quint8 _brightness = 0xff;
		int _totalLedCount = 0;
		int _controllerLedCount = default_controller_led_count;
		OutputMode _outputMode = OutputMode::Global;

		static bool isRegistered;
};
