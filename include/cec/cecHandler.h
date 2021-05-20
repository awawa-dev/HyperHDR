#pragma once
#include <QObject>
#include <QString>
#include <iostream>
#include <utils/Logger.h>
#include <cec.h>

class cecHandler : public QObject
{
	Q_OBJECT

public:
	cecHandler();
	~cecHandler() override;

	bool start();
	void stop();

signals:
	void stateChange(bool enabled, QString info);
	void keyPressed(int keyCode);

private:
	static void handleCecLogMessage(void* context, const CEC::cec_log_message* message);
	static void handleCecCommandMessage(void* context, const CEC::cec_command* command);
	static void handleCecKeyPress(void* context, const CEC::cec_keypress* key);

	CEC::ICECCallbacks			_cecCallbacks;
	CEC::libcec_configuration	_cecConfig;
	CEC::ICECAdapter*			_cecAdapter;

	Logger*						_log;
};


