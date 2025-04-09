#pragma once

#ifndef PCH_ENABLED
	#include <QString>
	#include <QMutex>
	#include <QJsonObject>
	#include <QJsonArray>
#endif

#include <utils/Logger.h>
#include <utils/settings.h>
#include <base/SoundCaptureResult.h>

class AnimationBaseMusic;

class SoundCapture : public QObject
{
	Q_OBJECT

protected:
	Logger*			_logger;

	SoundCapture(const QJsonDocument& effectConfig, QObject* parent = nullptr);
	virtual ~SoundCapture();
	
	QList<QString>	_availableDevices;
	bool			_isActive;
	bool			_enable_smoothing;
	QString			_selectedDevice;
	QString			_normalizedName;
	bool			_isRunning;
	QList<uint32_t>	_instances;

public:
	bool analyzeSpectrum(int16_t soundBuffer[], int sizeP);

	SoundCaptureResult* hasResult(AnimationBaseMusic* effect, uint32_t& lastIndex, bool* newAverage, bool* newSlow, bool* newFast, int* isMulti);
	virtual void		start() = 0;
	virtual void		stop() = 0;

public slots:
	QJsonObject getJsonInfo();
	uint32_t	open();
	void		close(uint32_t instance);
	void		settingsChangedHandler(settings::type type, const QJsonDocument& config);

private:
	QList<QString>		getDevices() const;
	bool				getActive() const;
	QString				getSelectedDevice() const;
	uint32_t            _maxInstance;

	static uint32_t	    _noSoundCounter;
	static bool			_noSoundWarning;
	static bool			_soundDetectedInfo;

	static inline int16_t  FIX_MPY(int16_t  a, int16_t  b);
	static int32_t fix_fft(int16_t fr[], int16_t fi[], int16_t exp, bool inverse);
};
