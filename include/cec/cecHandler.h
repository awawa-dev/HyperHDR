#pragma once

#ifndef PCH_ENABLED
	#include <QObject>
	#include <QString>
#endif

#include <utils/Logger.h>
class QSocketNotifier;

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

private slots:
	void handleMessages();

private:
	int _fd;
	QSocketNotifier* _notifier;
	void sendActiveSource();
};
