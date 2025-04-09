#pragma once

#include <linux/spi/spidev.h>
#include <led-drivers/LedDevice.h>

class ProviderSpi : public LedDevice
{
public:
	ProviderSpi(const QJsonObject& deviceConfig);
	bool init(const QJsonObject& deviceConfig) override;
	~ProviderSpi() override;
	int open() override;

	QJsonObject discover(const QJsonObject& params) override;

public slots:
	int close() override;

protected:
	int writeBytes(unsigned size, const uint8_t* data);
	int writeBytesEsp8266(unsigned size, const uint8_t* data);
	int writeBytesEsp32(unsigned size, const uint8_t* data);
	int writeBytesRp2040(unsigned size, const uint8_t* data);

	QString _deviceName;
	int _baudRate_Hz;
	int _fid;
	int _spiMode;
	bool _spiDataInvert;

	QString _spiType;	
};
