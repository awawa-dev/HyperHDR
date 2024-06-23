#pragma once

#include "ProviderUdp.h"

class DriverNetUdpH801 : public ProviderUdp
{
public:
	explicit DriverNetUdpH801(const QJsonObject& deviceConfig);

	static LedDevice* construct(const QJsonObject& deviceConfig);

private:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;

	QList<int> _ids;
	QByteArray _message;
	const int _prefix_size = 2;
	const int _colors = 5;
	const int _id_size = 3;
	const int _suffix_size = 1;

	static bool isRegistered;
};
