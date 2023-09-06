#pragma once

// Qt includes
#include <QThread>
#include <QJsonObject>
#include <QSize>
#include <QImage>
#include <QPainter>

#include <utils/Components.h>
#include <utils/Image.h>

#include <atomic>
#include <effectengine/AnimationBase.h>

class HyperHdrInstance;
class Logger;

class Effect : public QThread
{
	Q_OBJECT

public:
	friend class EffectModule;

	Effect(HyperHdrInstance* hyperhdr,
			int visiblePriority,
			int priority,
			int timeout,
			const QString& name,
			const QJsonObject& args = QJsonObject(),
			const QString& imageData = ""
	);

	~Effect() override;

	void run() override;

	int  getPriority() const;
	void requestInterruption();
	bool isInterruptionRequested();
	void visiblePriorityChanged(quint8 priority);
	void setLedCount(int newCount);

	QString getName()     const;
	int getTimeout()      const;
	QJsonObject getArgs() const;

	

signals:
	void setInput(int priority, const std::vector<ColorRgb>& ledColors, int timeout_ms, bool clearEffect);
	void setInputImage(int priority, const Image<ColorRgb>& image, int timeout_ms, bool clearEffect);

private:
	bool ImageShow();
	bool LedShow();

	HyperHdrInstance*	_hyperhdr;
	std::atomic<int>	_visiblePriority;
	const int			_priority;
	const int			_timeout;

	const QString		_name;
	const QJsonObject	_args;
	const QString		_imageData;
	int64_t				_endTime;
	QVector<ColorRgb>	_colors;

	Logger*				_log;
	std::atomic<bool>	_interupt{};

	QSize				_imageSize;
	QImage				_image;
	QPainter*			_painter;
	AnimationBase*		_effect;
	QVector<ColorRgb>	_ledBuffer;
	uint32_t			_soundHandle;
	std::atomic<int>	_ledCount;
};
