#pragma once

#include <memory>
#include <led-drivers/LedDevice.h>
#include <led-drivers/spi/ProviderSpiInterface.h>

class ProviderSpi : public LedDevice
{
	std::unique_ptr<ProviderSpiInterface> _provider;

public:
	ProviderSpi(const QJsonObject& deviceConfig);
	bool init(QJsonObject deviceConfig) override;
	int open() override;

	QJsonObject discover(const QJsonObject& params) override;

public slots:
	int close() override;

protected:
	int writeBytes(unsigned size, const uint8_t* data);
	int writeBytesEsp8266(unsigned size, const uint8_t* data);
	int writeBytesEsp32(unsigned size, const uint8_t* data);
	int writeBytesRp2040(unsigned size, const uint8_t* data);
	int getRate();
	QString getSpiType();
};
