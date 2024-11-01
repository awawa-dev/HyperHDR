#pragma once

#include "ProviderSerial.h"

class DriverSerialSkydimo : public ProviderSerial
{
	Q_OBJECT

public:
	explicit DriverSerialSkydimo(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(const QJsonObject& deviceConfig) override;
	void CreateHeader();
	int write(const std::vector<ColorRgb>& ledValues) override;

	const short _headerSize;


	static bool isRegistered;
};
