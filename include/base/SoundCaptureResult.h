#pragma once

#ifndef PCH_ENABLED
	#include <QColor>
#endif

#define SOUNDCAP_N_WAVE      1024
#define SOUNDCAP_LOG2_N_WAVE 10
#define SOUNDCAP_RESULT_RES  8

struct MovingTarget
{
	QColor	_averageColor;
	QColor	_fastColor;
	QColor	_slowColor;
	int32_t	_targetAverageR;
	int32_t _targetAverageG;
	int32_t _targetAverageB;
	int32_t _targetAverageCounter;
	int32_t _targetSlowR;
	int32_t _targetSlowG;
	int32_t _targetSlowB;
	int32_t _targetSlowCounter;
	int32_t _targetFastR;
	int32_t _targetFastG;
	int32_t _targetFastB;
	int32_t _targetFastCounter;

	void Clear();
	void CopyFrom(MovingTarget* source);
};

class SoundCaptureResult
{
	friend class  SoundCapture;

	int32_t  _maxAverage;
	bool     _validData;
	uint32_t _resultIndex;
	int32_t  pureResult[SOUNDCAP_RESULT_RES];
	int32_t  lastResult[SOUNDCAP_RESULT_RES];

	int32_t  maxResult[SOUNDCAP_RESULT_RES];
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
	uint32_t getResultIndex();
	void ResetData();
	void GetBufResult(uint8_t* dest, size_t size);
	QColor getRangeColor(uint8_t index) const;
	void RestoreFullLum(QColor& color, int scale = 32);
	int32_t	getValue(int isMulti);
	int32_t getValue3Step(int isMulti);
	bool GetStats(uint32_t& scaledAverage, uint32_t& currentMax, QColor& averageColor, QColor* fastColor = NULL, QColor* slowColor = NULL);

private:
	bool hasMiddleAverage(int middle);
	bool hasMiddleSlow(int middle);
	bool hasMiddleFast(int middle);
	void CalculateRgbDelta(QColor currentColor, QColor prevColor, QColor selcolor, int& ab_r, int& ab_g, int& ab_b);
};
