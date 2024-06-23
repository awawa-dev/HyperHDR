#pragma once

#include "ProviderUdp.h"

class DriverNetTpm2net : public ProviderUdp
{
public:
	explicit DriverNetTpm2net(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;

	int _tpm2Max;
	int _tpm2ByteCount;
	int _tpm2TotalPackets;

	static bool isRegistered;
};
