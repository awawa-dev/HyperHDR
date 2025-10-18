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
	int writeColors(const std::vector<ColorRgb>* ledValues, const SharedOutputColors infiniteColors);
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;
	std::pair<bool, int> writeInfiniteColors(SharedOutputColors nonlinearRgbColors) override;

private:
	std::chrono::time_point<std::chrono::high_resolution_clock> _lastWriteTimeNano;
	
	QFile* _file;

	QString _fileName;
	bool _printTimeStamp;
	bool _infiniteColorEngine;

	static bool isRegistered;
};
