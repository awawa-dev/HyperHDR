#pragma once

#ifndef PCH_ENABLED
	#include <QObject>
	#include <QString>
	#include <list>
#endif

#include <utils/Components.h>

class Logger;
class cecHandler;

class WrapperCEC : public QObject
{
	Q_OBJECT

public:
	WrapperCEC();
	~WrapperCEC() override;

public slots:
	void sourceRequestHandler(hyperhdr::Components component, int hyperHdrInd, bool listen);	

signals:
	void SignalStateChange(bool enabled, QString info);
	void SignalKeyPressed(int keyCode);

private:
	void enable(bool enabled);

	std::list<int>	_cecClients;
	cecHandler*		_cecHandler;
	Logger*			_log;
};
