#pragma once


#include <utils/Logger.h>
#include <utils/settings.h>
#include <QString>
#include <QSemaphore>
#include <QJsonObject>
#include <QColor>
#include <effectengine/AnimationBaseMusic.h>

#define SOUNDCAP_N_WAVE      1024
#define SOUNDCAP_LOG2_N_WAVE 10

#define SOUNDCAP_RESULT_RES 8

class SoundCaptureResult
{
	friend class  SoundCapture;

	int32_t _maxAverage;	
	bool     _validData;
	int32_t pureResult[SOUNDCAP_RESULT_RES];
	int32_t lastResult[SOUNDCAP_RESULT_RES];

	int32_t maxResult[SOUNDCAP_RESULT_RES];
	uint8_t  pureScaledResult[SOUNDCAP_RESULT_RES];
	uint8_t  deltas[SOUNDCAP_RESULT_RES];
	uint8_t  buffScaledResult[SOUNDCAP_RESULT_RES];
	QColor	 color[SOUNDCAP_RESULT_RES];
	int32_t  averageDelta;

	QColor   _lastPrevColor;
	int32_t  _scaledAverage;
	int32_t  _oldScaledAverage;
	int32_t  _currentMax;

	MovingTarget mtWorking, mtInternal;	


public:
	SoundCaptureResult();

	void ClearResult();

	void AddResult(int samplerIndex, uint32_t val);

	void Smooth();

	bool isDataValid();

	void ResetData();

	void GetBufResult(uint8_t* dest, size_t size);

	QColor getRangeColor(uint8_t index) const;

	void RestoreFullLum(QColor& color, int scale = 32);

	int32_t	getValue(int isMulti);

	int32_t getValue3Step(int isMulti);

	bool GetStats(uint32_t& scaledAverage, uint32_t& currentMax, QColor& averageColor, QColor *fastColor = NULL, QColor *slowColor = NULL);

private:
	bool hasMiddleAverage(int middle);

	bool hasMiddleSlow(int middle);

	bool hasMiddleFast(int middle);

	void CalculateRgbDelta(QColor currentColor, QColor prevColor, QColor selcolor, int& ab_r, int& ab_g, int& ab_b);
};

class  SoundCapture : public QObject
{
	Q_OBJECT
protected:
	static SoundCapture* _soundInstance;

	SoundCapture(const QJsonDocument& effectConfig, QObject* parent = nullptr);		
	~SoundCapture();

	static bool AnaliseSpectrum(int16_t soundBuffer[], int sizeP);

	QList<QString>   _availableDevices;
	bool	         _isActive;
	QString          _selectedDevice;
	static bool		 _isRunning;
	QList<uint32_t>  _instances;
	static uint32_t  _resultIndex;
	static SoundCaptureResult   _resultFFT;

public:	
	static SoundCapture* getInstance();

	QList<QString>		getDevices() const;
	bool				getActive() const;
	QString				getSelectedDevice() const;	
	SoundCaptureResult* hasResult(uint32_t& lastIndex);
	SoundCaptureResult* hasResult(AnimationBaseMusic *effect, uint32_t& lastIndex, bool* newAverage, bool* newSlow, bool* newFast, int *isMulti);
	void				ForcedClose();

public slots:
	uint32_t	getCaptureInstance();
	void		releaseCaptureInstance(uint32_t instance);
	void		handleSettingsUpdate(settings::type type, const QJsonDocument& config);

private:
	virtual void    Start() = 0;
	virtual void    Stop() = 0;

	uint32_t            _maxInstance;

	static uint32_t	    _noSoundCounter;
	static bool			_noSoundWarning;
	static bool			_soundDetectedInfo;

	static QSemaphore   _semaphore;


	// FFT
	static int16_t  _imgBuffer[SOUNDCAP_N_WAVE * 2];	
	static int16_t  _lutSin[SOUNDCAP_N_WAVE - SOUNDCAP_N_WAVE / 4];

	static inline int16_t  FIX_MPY(int16_t  a, int16_t  b);	        
	static int32_t fix_fft(int16_t fr[], int16_t fi[], int16_t m, bool inverse);
};
