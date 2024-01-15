#pragma once

class LedDevice;
class QJsonObject;

class LedDeviceFactory
{
public:
	static LedDevice* construct(const QJsonObject& deviceConfig);
};
