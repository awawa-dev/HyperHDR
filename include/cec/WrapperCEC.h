#pragma once

#include <HyperhdrConfig.h>

#ifndef PCH_ENABLED
	#include <QObject>
	#include <QJsonObject>
	#include <QJsonArray>
	#include <QString>
	#include <QStringList>
	#include <QMultiMap>

	#include <utils/ColorRgb.h>
	#include <utils/Image.h>
	#include <utils/Logger.h>
	#include <utils/settings.h>
	#include <utils/Components.h>
#endif

#include <cec/cecHandler.h>

class GlobalSignals;

static QList<int> CEC_CLIENTS;

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

	cecHandler* _cecHandler;
	Logger*		_log;
};
