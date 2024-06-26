#pragma once

#ifndef PCH_ENABLED
	#include <QThread>
	#include <QJsonObject>
	#include <QTimer>

	#include <atomic>	
#endif

#include <hyperimage/HyperImage.h>

#include <utils/Components.h>
#include <utils/Image.h>
#include <effectengine/AnimationBase.h>


class HyperHdrInstance;
class Logger;

class Effect : public QObject
{
	Q_OBJECT

public slots:
	void start();
	void run();
	void stop();

	void visiblePriorityChanged(quint8 priority);

public:
	Effect(HyperHdrInstance* hyperhdr,
			int visiblePriority,
			int priority,
			int timeout,
			const EffectDefinition& effect
	);

	~Effect() override;

	QString	getDescription() const;

	int  getPriority() const;
	void requestInterruption();
	void setLedCount(int newCount);

	QString getName()     const;
	int getTimeout()      const;

	static std::list<EffectDefinition> getAvailableEffects();	

signals:
	void SignalSetLeds(int priority, const std::vector<ColorRgb>& ledColors, int timeout_ms, bool clearEffect);
	void SignalSetImage(int priority, const Image<ColorRgb>& image, int timeout_ms, bool clearEffect);
	void SignalEffectFinished(int priority, QString name, bool forced);

private:
	void imageShow(int left);
	void ledShow(int left);

	int					_visiblePriority;
	const int			_priority;
	const int			_timeout;
	const int			_instanceIndex;
	const QString		_name;
	AnimationBase*		_effect;

	int64_t				_endTime;
	QVector<ColorRgb>	_colors;

	Logger*				_log;
	std::atomic<bool>	_interrupt;

	HyperImage			_image;
	
	QTimer				_timer;
	QVector<ColorRgb>	_ledBuffer;	
	std::atomic<int>	_ledCount;
};
