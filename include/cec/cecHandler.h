#pragma once
#include <QObject>
#include <QString>
#include <iostream>
#include <utils/Logger.h>

class cecHandler : public QObject
{
	Q_OBJECT

public:
	cecHandler();
	~cecHandler() override;

	bool start();
	void stop();
	Logger* _log;

signals:
	void stateChange(bool enabled, QString info);
	void keyPressed(int keyCode);
};


