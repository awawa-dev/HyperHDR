#pragma once

#ifndef PCH_ENABLED
	#include <QByteArray>
	#include <QElapsedTimer>
	#include <QList>
	#include <QMutex>
	#include <QString>
	#include <cstdint>
	#include <vector>
#endif

#include <led-drivers/LedDevice.h>

#if defined(_WIN32)
	#include <windows.h>
#endif

class DriverOtherSyncLight : public LedDevice
{
public:
	explicit DriverOtherSyncLight(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);

	struct SupportedDevice
	{
		quint16 vendorId;
		quint16 productId;
	};

	enum class OutputMode
	{
		Global,
		Segments
	};

protected:
	bool init(QJsonObject deviceConfig) override;
	int open() override;
	int close() override;
	int writeFiniteColors(const std::vector<ColorRgb>& ledValues) override;
	bool powerOn() override;
	bool powerOff() override;

private:
	static constexpr int REPORT_SIZE = 64;
	static constexpr int RB_OVERHEAD = 6;
	static constexpr int SC_HEADER_SIZE = 5;
	static constexpr int SC_RECORD_SIZE = 5;
	static constexpr int SC_FOOTER_SIZE = 1;
	static constexpr int SC_CHECKSUM_SIZE = 1;
	static constexpr int DEFAULT_CONTROLLER_LED_COUNT = 65;
	static constexpr int KEEPALIVE_INTERVAL_MS = 3000;
	static constexpr quint8 ACTION_COLOR = 0x86;
	static constexpr quint8 ACTION_BRIGHTNESS = 0x87;
	static constexpr quint8 ACTION_KEEPALIVE = 0x97;
	static constexpr quint8 SECTION_GLOBAL = 1;

	static quint8 checksum(const QByteArray& frame);
	static bool parseDeviceId(const QString& text, quint16& value);
	static QByteArray buildRbFrame(quint8 action, const QByteArray& payload, quint8 id);
	static QByteArray buildScFrame(const std::vector<ColorRgb>& ledValues, int totalLedCount, int controllerLedCount, quint8 id);
	static QByteArray buildReport(const QByteArray& frame);
	static QByteArray buildSectionPayload(quint8 section, quint8 red, quint8 green, quint8 blue);
	static ColorRgb averageColor(const std::vector<ColorRgb>& ledValues, int ledCount);
	static ColorRgb averageColorRange(const std::vector<ColorRgb>& ledValues, int offset, int count);

	QList<SupportedDevice> configuredDevices() const;
	QString openDeviceHandle();
	void closeDeviceHandle();
	bool writeReport(const QByteArray& report);
	bool sendRb(quint8 action, const QByteArray& payload);
	bool sendAveragedSectionColor(const std::vector<ColorRgb>& ledValues);
	bool sendScColors(const std::vector<ColorRgb>& ledValues);
	bool sendBrightness(quint8 value);
	bool sendKeepaliveIfNeeded();

	quint8 nextId();

	QMutex _transaction;
	QList<SupportedDevice> _devices;
	quint8 _idCounter;
	quint8 _brightness;
	int _totalLedCount;
	int _controllerLedCount;
	OutputMode _outputMode;
	QElapsedTimer _lastKeepalive;

#if defined(_WIN32)
	HANDLE _deviceHandle;
#endif

	static bool isRegistered;
};
