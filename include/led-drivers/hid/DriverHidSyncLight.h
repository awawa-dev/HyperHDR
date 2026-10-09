#pragma once

#include <led-drivers/hid/ProviderHid.h>

#include <initializer_list>

class DriverHidSyncLight : public ProviderHid
{
public:
	explicit DriverHidSyncLight(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	static constexpr size_t MAX_RANGES = 34;

	enum class OutputMode
	{
		Global,
		PerLed
	};

	bool init(QJsonObject deviceConfig) override;
	bool initDevice() override;
	bool powerOn() override;
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;
	std::vector<HidDeviceInfo> selectDevices(std::vector<HidDeviceInfo> found) override;
	std::vector<HidDeviceInfo> devicesToOpen(std::vector<HidDeviceInfo> supported) override;

	bool sendRb(uint8_t action, std::initializer_list<uint8_t> payload = {});
	bool sendFrame(const std::vector<uint8_t>& frame);
	bool sendBrightness();
	bool sendAveragedColor(const std::vector<ColorRgb>& leds);
	bool sendScColors(const std::vector<ColorRgb>& leds);
	uint8_t nextId();
	void logDeviceInfo();

	uint8_t _idCounter = 0;
	uint8_t _brightness = 0xff;
	QString _output;
	int _controllerLedCount = 0;
	int _controllerSegmentLimit = MAX_RANGES;
	OutputMode _outputMode = OutputMode::Global;

	static bool isRegistered;
};
