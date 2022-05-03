#ifndef LEDEVICEAWA_SPI_H
#define LEDEVICEAWA_SPI_H

// HyperHDR includes
#include "ProviderSpi.h"

///
/// Implementation of the LedDevice interface for writing to AWA SPI led device.
///
class LedDeviceAWA_spi : public ProviderSpi
{
public:

	///
	/// @brief Constructs an AWA SPI LED-device
	///
	/// @param deviceConfig Device's configuration as JSON-Object
	///
	explicit LedDeviceAWA_spi(const QJsonObject& deviceConfig);

	///
	/// @brief Constructs the LED-device
	///
	/// @param[in] deviceConfig Device's configuration as JSON-Object
	/// @return LedDevice constructed
	///
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	void CreateHeader();

	///
	/// @brief Initialise the device's configuration
	///
	/// @param[in] deviceConfig the JSON device configuration
	/// @return True, if success
	///
	bool init(const QJsonObject& deviceConfig) override;

	///
	/// @brief Writes the RGB-Color values to the LEDs.
	///
	/// @param[in] ledValues The RGB-color per LED
	/// @return Zero on success, else negative
	///
	int write(const std::vector<ColorRgb>& ledValues) override;

	void whiteChannelExtension(uint8_t*& writer);

	int _headerSize;

	bool _white_channel_calibration;
	uint8_t _white_channel_limit;
	uint8_t _white_channel_red;
	uint8_t _white_channel_green;
	uint8_t _white_channel_blue;
};

#endif // LEDEVICEAPA102_H
