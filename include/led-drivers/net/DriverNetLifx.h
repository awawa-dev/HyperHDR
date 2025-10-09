#pragma once

#include "ProviderUdp.h"
#include <array>
#include <vector>
#include <utility>
#include <QHostAddress>

class DriverNetLifx : public ProviderUdp
{	
public:
	explicit DriverNetLifx(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

	using MacAddress = std::array<uint8_t, 6>;
	using IpMacAddress = std::pair<QHostAddress, MacAddress>;

protected:
	bool init(QJsonObject deviceConfig) override;
	std::pair<bool, int> writeInfiniteColors(SharedOutputColors nonlinearRgbColors) override;

	std::vector<IpMacAddress> lamps;
	static bool isRegistered;
};
