#pragma once

#ifndef PCH_ENABLED
	#include <QDateTime>
	#include <chrono>
#endif

#include <led-drivers/LedDevice.h>

class QFile;

class DriverOtherFile : public LedDevice
{
public:
	explicit DriverOtherFile(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

protected:
	bool init(QJsonObject deviceConfig) override;
	int open() override;
	int close() override;
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;

private:
	std::chrono::time_point<std::chrono::high_resolution_clock> _lastWriteTimeNano;
	
	QFile* _file;

	QString _fileName;
	bool _printTimeStamp;

	static bool isRegistered;
};
