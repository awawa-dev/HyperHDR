#pragma once

#include "ProviderSerial.h"

class DriverSerialSkydimo : public ProviderSerial
{
	Q_OBJECT

public:
	explicit DriverSerialSkydimo(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(QJsonObject deviceConfig) override;
	void CreateHeader();
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;

	const short _headerSize;


	static bool isRegistered;
};
