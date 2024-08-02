#pragma once

#include <led-drivers/LedDevice.h>
#include "ProviderUdp.h"

class DriverNetCololight : public ProviderUdp
{
	enum appID {
		TL1_CMD = 0x00,
		DIRECT_CONTROL = 0x01,
		TRANSMIT_FILE = 0x02,
		CLEAR_FILES = 0x03,
		WRITE_FILE = 0x04,
		READ_FILE = 0x05,
		MODIFY_SECU = 0x06
	};

	enum effect : uint32_t {
		SAVANNA = 0x04970400,
		SUNRISE = 0x01c10a00,
		UNICORNS = 0x049a0e00,
		PENSIEVE = 0x04c40600,
		THE_CIRCUS = 0x04810130,
		INSTASHARE = 0x03bc0190,
		EIGTHIES = 0x049a0000,
		CHERRY_BLOS = 0x04940800,
		RAINBOW = 0x05bd0690,
		TEST = 0x03af0af0,
		CHRISTMAS = 0x068b0900
	};

	enum verbs {
		GET = 0x03,
		SET = 0x04,
		SETEEPROM = 0x07,
		SETVAR = 0x0b
	};

	enum commandTypes {
		STATE_OFF = 0x80,
		STATE_ON = 0x81,
		BRIGTHNESS = 0xCF,
		SETCOLOR = 0xFF
	};

	enum idxTypes {
		BRIGTHNESS_CONTROL = 0x01,
		COLOR_CONTROL = 0x02,
		COLOR_DIRECT_CONTROL = 0x81,
		READ_INFO_FROM_STORAGE = 0x86
	};

	enum bufferMode {
		MONOCROME = 0x01,
		LIGHTBEAD = 0x02,
	};

	enum ledLayout {
		STRIP_LAYOUT,
		MODLUE_LAYOUT
	};

	enum modelType {
		STRIP,
		PLUS
	};

public:
	explicit DriverNetCololight(const QJsonObject& deviceConfig);
	static LedDevice* construct(const QJsonObject& deviceConfig);
	QJsonObject discover(const QJsonObject& params) override;
	QJsonObject getProperties(const QJsonObject& params) override;
	void identify(const QJsonObject& params) override;

protected:
	bool init(const QJsonObject& deviceConfig) override;
	int write(const std::vector<ColorRgb>& ledValues) override;
	bool powerOn() override;
	bool powerOff() override;

private:
	bool initLedsConfiguration();
	void initDirectColorCmdTemplate();
	bool getInfo();
	bool setEffect(const effect effect);
	bool setColor(const ColorRgb colorRgb);
	bool setColor(const uint32_t color);
	bool setColor(const std::vector<ColorRgb>& ledValues);
	bool setTL1CommandMode(bool isOn);
	bool setState(bool isOn);
	bool setStateDirect(bool isOn);
	bool sendRequest(const appID appID, const QByteArray& command);
	bool readResponse();
	bool readResponse(QByteArray& response);

	int _modelType;
	int _ledLayoutType;
	int _ledBeadCount;
	int _distance;

	QByteArray _packetFixPart;
	QByteArray _DataPart;
	QByteArray _directColorCommandTemplate;
	quint32 _sequenceNumber;

	QMultiMap<QString, QMap <QString, QString>> _services;
	static bool isRegistered;
};
