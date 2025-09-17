#pragma once

#ifndef PCH_ENABLED
	#include <QObject>
	#include <QString>
	#include <iostream>
#endif

#include <utils/Logger.h>

class cecHandler : public QObject
{
	Q_OBJECT

public:
	cecHandler();
	~cecHandler() override;

	bool start();
	void stop();
	LoggerName _log;

signals:
	void stateChange(bool enabled, QString info);
	void keyPressed(int keyCode);
};


