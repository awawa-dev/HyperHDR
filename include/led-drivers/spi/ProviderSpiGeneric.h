#pragma once

#include <linux/spi/spidev.h>
#include <led-drivers/LedDevice.h>
#include <led-drivers/spi/ProviderSpiInterface.h>

class ProviderSpiGeneric : public QObject, public ProviderSpiInterface
{
public:
	ProviderSpiGeneric(Logger* _log);
	~ProviderSpiGeneric();

public:
	bool init(const QJsonObject& deviceConfig) override;

	QString open() override;
	int close() override;

	QJsonObject discover(const QJsonObject& params) override;

	int writeBytes(unsigned size, const uint8_t* data) override;
	int getRate() override;
	QString getSpiType() override;
	ProviderSpiInterface::SpiProvider getProviderType() override;
};
