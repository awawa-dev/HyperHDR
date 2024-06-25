#pragma once

#ifndef PCH_ENABLED
	#include <QMap>
	#include <QObject>
	#include <QMap>
	#include <QVector>
	#include <QTime>

	#include <vector>
	#include <cstdint>
#endif

#include <image/ColorRgb.h>
#include <image/Image.h>
#include <utils/Components.h>

#define SMOOTHING_DEFAULT_CONFIG 0

class QTimer;
class Logger;

class Muxer : public QObject
{
	Q_OBJECT
public:
	struct InputInfo
	{
		int priority;
		int64_t timeout;
		ColorRgb staticColor;
		hyperhdr::Components componentId;
		QString origin;
		unsigned smooth_cfg;
		QString owner;
	};

	const static int LOWEST_PRIORITY;
	const static int HIGHEST_EFFECT_PRIORITY;
	const static int LOWEST_EFFECT_PRIORITY;

	Muxer(int instanceIndex, int ledCount, QObject* parent);
	~Muxer() override;

	void setEnable(bool enable);
	bool setSourceAutoSelectEnabled(bool enabel, bool update = true);
	bool isSourceAutoSelectEnabled() const { return _sourceAutoSelectEnabled; }
	bool setPriority(int priority);
	int getCurrentPriority() const { return _currentPriority; }
	int getPreviousPriority() const { return _previousPriority; }
	bool hasPriority(int priority) const;
	QList<int> getPriorities() const;
	const InputInfo& getInputInfo(int priority) const;
	const QMap<int, InputInfo>& getInputInfoTable() const;
	void registerInput(int priority, hyperhdr::Components component, const QString& origin = "System", const ColorRgb& staticColor = ColorRgb::BLACK, unsigned smooth_cfg = SMOOTHING_DEFAULT_CONFIG, const QString& owner = "");
	bool setInput(int priority, int64_t timeout_ms);
	bool setInputInactive(int priority);
	bool clearInput(int priority);
	void clearAll(bool forceClearAll = false);

signals:
	void SignalTimeRunner_Internal();
	void SignalVisiblePriorityChanged(quint8 priority);
	void SignalVisibleComponentChanged(hyperhdr::Components comp);
	void SignalPrioritiesChanged();
	void SignalTimeTrigger_Internal();

private slots:
	void timeTrigger();
	void setCurrentTime();

private:
	hyperhdr::Components getComponentOfPriority(int priority) const;

	Logger* _log;
	int _currentPriority;
	int _previousPriority;
	int _manualSelectedPriority;
	hyperhdr::Components _prevVisComp = hyperhdr::COMP_INVALID;
	QMap<int, InputInfo> _activeInputs;
	InputInfo _lowestPriorityInfo;
	bool _sourceAutoSelectEnabled;

	QTimer* _updateTimer;
	QTimer* _timer;
	QTimer* _blockTimer;
};
