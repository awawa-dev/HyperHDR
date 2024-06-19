#ifndef PCH_ENABLED
	#include <cmath>
	#include <cstring> 
#endif

#include <base/SoundCaptureResult.h>

#define Limit(x,y) std::min(std::max(x, 0), y)
#define LimitMod(x,y) ((x+y) % y)
#define Drive(x,y) ((x == 0) ? x : ((x>=0 && x < y)? 1 : ( (x<0 && -x < y) ? -1: (x/y) ) ))

int32_t SoundCaptureResult::getValue(int isMulti)
{
	if (isMulti == 2)
	{
		return Limit((_oldScaledAverage + _scaledAverage) / 2, 255);
	}
	else if (isMulti == 1)
	{
		return _scaledAverage;
	}

	return -1;
}

int32_t SoundCaptureResult::getValue3Step(int isMulti)
{
	if (isMulti == 2)
	{
		return Limit((_scaledAverage - _oldScaledAverage) / 3 + _oldScaledAverage, 255);
	}
	else if (isMulti == 1)
	{
		return Limit(((_scaledAverage - _oldScaledAverage) * 2) / 3 + _oldScaledAverage, 255);
	}
	else {
		return _scaledAverage;
	}
}

SoundCaptureResult::SoundCaptureResult()
{
	ResetData();

	color[0] = ColorRgb(255, 0, 0);
	color[1] = ColorRgb(190, 0, 0);
	color[2] = ColorRgb(160, 160, 0);
	color[3] = ColorRgb(50, 190, 20);
	color[4] = ColorRgb(20, 255, 50);
	color[5] = ColorRgb(0, 212, 160);
	color[6] = ColorRgb(0, 120, 190);
	color[7] = ColorRgb(0, 0, 255);

}

ColorRgb SoundCaptureResult::getRangeColor(uint8_t index) const
{
	if (index < SOUNDCAP_RESULT_RES)
		return color[index];
	return color[0];
}

void SoundCaptureResult::ClearResult()
{
	memset(pureResult, 0, sizeof(pureResult));
}

void SoundCaptureResult::AddResult(int samplerIndex, uint32_t val)
{
	pureResult[samplerIndex] += val;
}



void SoundCaptureResult::Smooth()
{
	int32_t currentAverage = 0, distance = 0, red = 0, green = 0, blue = 0;

	MovingTarget res = mtInternal;

	_validData = true;

	for (int i = 0; i < SOUNDCAP_RESULT_RES; i++)
	{
		int32_t buffResult = (pureResult[i] + lastResult[i]) / 2;

		if (maxResult[i] <= buffResult && buffResult > 400)
		{
			if (maxResult[i] == 0)
				_validData = false;

			maxResult[i] = buffResult;

			for (int j = 0; j < SOUNDCAP_RESULT_RES; j++)
				if (i != j && maxResult[j] < buffResult / 4 && maxResult[j] < 400)
					maxResult[j] = buffResult / 4;
		}


		if (_validData && maxResult[i] > 0)
		{
			int32_t sr = (pureResult[i] * 255) / maxResult[i];

			if (sr > 255)
				sr = 255;

			pureScaledResult[i] = (uint8_t)sr;

			if (pureScaledResult[i] >= buffScaledResult[i] || buffScaledResult[i] - deltas[i] < pureScaledResult[i])
			{
				buffScaledResult[i] = pureScaledResult[i];
				deltas[i] = std::max(pureScaledResult[i] / 10, 3);
			}
			else
			{
				buffScaledResult[i] -= deltas[i];
				deltas[i] = std::min((deltas[i] * 3) / 2, 255);
			}
		}

		lastResult[i] = pureResult[i];
	}

	for (int j = 0; j < SOUNDCAP_RESULT_RES; j++)
	{
		int32_t max = maxResult[j];
		if (max > 2000)
			maxResult[j] = max - 2;

		currentAverage += buffScaledResult[j];
	}

	if (_validData)
	{
		if (currentAverage > _maxAverage)
			_maxAverage = currentAverage;

		if (_maxAverage > 0)
		{
			_currentMax = 0;
			for (int i = 0; i < SOUNDCAP_RESULT_RES; i++)
				if (buffScaledResult[i] > _currentMax)
					_currentMax = buffScaledResult[i];

			currentAverage = std::min((currentAverage * 255) / _maxAverage, 255);
			currentAverage = std::min((currentAverage + _currentMax) / 2, 255);

			_oldScaledAverage = _scaledAverage;

			if (currentAverage >= _scaledAverage || _scaledAverage - averageDelta < currentAverage)
			{
				_scaledAverage = currentAverage;
				averageDelta = std::max(currentAverage / 14, 3);
			}
			else
			{
				_scaledAverage -= averageDelta;
				averageDelta = std::min(averageDelta * 2, 255);
			}



			int32_t  r = 0, g = 0, b = 0;
			for (int i = 0; i < SOUNDCAP_RESULT_RES && _currentMax > 0; i++)
			{
				int rr = 0, gg = 0, bb = 0;

				color[i].getRGB(rr, gg, bb);


				r += std::max((rr * (_currentMax - 5 * (_currentMax - buffScaledResult[i]))) / _currentMax, 0);
				g += std::max((gg * (_currentMax - 5 * (_currentMax - buffScaledResult[i]))) / _currentMax, 0);
				b += std::max((bb * (_currentMax - 5 * (_currentMax - buffScaledResult[i]))) / _currentMax, 0);
			}

			int32_t ccScale = std::max(std::max(r, g), b);
			if (ccScale != 0)
			{
				r = (Limit((r * 255) / ccScale + 32, 255) * _scaledAverage) / 255;
				g = (Limit((g * 255) / ccScale + 32, 255) * _scaledAverage) / 255;
				b = (Limit((b * 255) / ccScale + 32, 255) * _scaledAverage) / 255;
			}
			ColorRgb currentColor(r, g, b);

			int av_r, av_g, av_b;
			CalculateRgbDelta(currentColor, _lastPrevColor, res._averageColor, av_r, av_g, av_b);

			int sv_r, sv_g, sv_b;
			CalculateRgbDelta(currentColor, _lastPrevColor, res._slowColor, sv_r, sv_g, sv_b);

			int fv_r, fv_g, fv_b;
			CalculateRgbDelta(currentColor, _lastPrevColor, res._fastColor, fv_r, fv_g, fv_b);


			_lastPrevColor = currentColor;

			if (res._targetFastCounter-- <= 0)
			{
				distance = Limit((int32_t)std::sqrt(fv_r * fv_r + fv_g * fv_g + fv_b * fv_b), 255);
				distance = std::max(distance, 30) / 15;

				res._targetFastR = Drive(fv_r, distance);
				res._targetFastG = Drive(fv_g, distance);
				res._targetFastB = Drive(fv_b, distance);

				if (std::abs(res._targetFastR) >= 2 || std::abs(res._targetFastG) >= 2 || std::abs(res._targetFastB) >= 2)
					res._targetFastCounter = 1;
				else
					res._targetFastCounter = 0;
			}


			red = Limit(res._targetFastR + res._fastColor.Red(), 255);
			green = Limit(res._targetFastG + res._fastColor.Green(), 255);
			blue = Limit(res._targetFastB + res._fastColor.Blue(), 255);
			res._fastColor = ColorRgb(red, green, blue);

			if (res._targetSlowCounter-- <= 0)
			{
				distance = Limit((int32_t)std::sqrt(sv_r * sv_r + sv_g * sv_g + sv_b * sv_b), 255);
				distance = std::max(distance, 16) / 8;

				res._targetSlowR = Drive(sv_r, distance);
				res._targetSlowG = Drive(sv_g, distance);
				res._targetSlowB = Drive(sv_b, distance);

				if (std::abs(res._targetSlowR) >= 3 || std::abs(res._targetSlowG) >= 3 || std::abs(res._targetSlowB) >= 3)
					res._targetSlowCounter = 3;
				else if (std::abs(res._targetSlowR) >= 2 || std::abs(res._targetSlowG) >= 2 || std::abs(res._targetSlowB) >= 2)
					res._targetSlowCounter = 1;
				else
					res._targetSlowCounter = 0;
			}


			red = Limit(res._targetSlowR + res._slowColor.Red(), 255);
			green = Limit(res._targetSlowG + res._slowColor.Green(), 255);
			blue = Limit(res._targetSlowB + res._slowColor.Blue(), 255);
			res._slowColor = ColorRgb(red, green, blue);


			if (res._targetAverageCounter-- <= 0)
			{
				distance = Limit((int32_t)std::sqrt(av_r * av_r + av_g * av_g + av_b * av_b), 255);
				distance = std::max(distance, 22) / 11;

				res._targetAverageR = Drive(av_r, distance);
				res._targetAverageG = Drive(av_g, distance);
				res._targetAverageB = Drive(av_b, distance);

				if (std::abs(res._targetAverageR) >= 3 || std::abs(res._targetAverageG) >= 3 || std::abs(res._targetAverageB) >= 3)
					res._targetAverageCounter = 3;
				else if (std::abs(res._targetAverageR) >= 2 || std::abs(res._targetAverageG) >= 2 || std::abs(res._targetAverageB) >= 2)
					res._targetAverageCounter = 1;
				else
					res._targetAverageCounter = 0;
			}


			red = Limit(res._targetAverageR + res._averageColor.Red(), 255);
			green = Limit(res._targetAverageG + res._averageColor.Green(), 255);
			blue = Limit(res._targetAverageB + res._averageColor.Blue(), 255);
			res._averageColor = ColorRgb(red, green, blue);

		}

		if (_maxAverage > 1024)
			_maxAverage--;
	}

	mtInternal = res;
	mtWorking = res;

	if (_validData)
		_resultIndex++;
}

void SoundCaptureResult::CalculateRgbDelta(ColorRgb currentColor, ColorRgb prevColor, ColorRgb selcolor, int& ab_r, int& ab_g, int& ab_b)
{
	ab_r = ((currentColor.Red() * 2 + prevColor.Red()) / 3) - selcolor.Red();
	ab_g = ((currentColor.Green() * 2 + prevColor.Green()) / 3) - selcolor.Green();
	ab_b = ((currentColor.Blue() * 2 + prevColor.Blue()) / 3) - selcolor.Blue();
}

bool SoundCaptureResult::hasMiddleAverage(int middle)
{
	if (mtWorking._targetAverageCounter <= 0)
	{
		return false;
	}

	int red = Limit(mtWorking._targetAverageR / 2 + mtWorking._averageColor.Red(), 255);
	int green = Limit(mtWorking._targetAverageG / 2 + mtWorking._averageColor.Green(), 255);
	int blue = Limit(mtWorking._targetAverageB / 2 + mtWorking._averageColor.Blue(), 255);

	mtWorking._averageColor = ColorRgb(red, green, blue);

	if (middle == 0)
		mtWorking._targetAverageCounter--;

	return true;
}

bool SoundCaptureResult::hasMiddleSlow(int middle)
{
	if (mtWorking._targetSlowCounter <= 0)
	{
		return false;
	}

	int red = Limit(mtWorking._targetSlowR / 2 + mtWorking._slowColor.Red(), 255);
	int green = Limit(mtWorking._targetSlowG / 2 + mtWorking._slowColor.Green(), 255);
	int blue = Limit(mtWorking._targetSlowB / 2 + mtWorking._slowColor.Blue(), 255);

	mtWorking._slowColor = ColorRgb(red, green, blue);

	if (middle == 0)
		mtWorking._targetSlowCounter--;

	return true;
}

bool SoundCaptureResult::hasMiddleFast(int middle)
{
	if (mtWorking._targetFastCounter <= 0)
	{
		return false;
	}

	int red = Limit(mtWorking._targetFastR / 2 + mtWorking._fastColor.Red(), 255);
	int green = Limit(mtWorking._targetFastG / 2 + mtWorking._fastColor.Green(), 255);
	int blue = Limit(mtWorking._targetFastB / 2 + mtWorking._fastColor.Blue(), 255);

	mtWorking._fastColor = ColorRgb(red, green, blue);

	if (middle == 0)
		mtWorking._targetFastCounter--;

	return true;
}


void SoundCaptureResult::RestoreFullLum(ColorRgb& color, int scale)
{
	int a, b, v;
	color.getHsv(a, b, v);

	if (v < scale)
		color.fromHsv(a, b, scale);
}

bool SoundCaptureResult::GetStats(uint32_t& scaledAverage, uint32_t& currentMax, ColorRgb& averageColor, ColorRgb* fastColor, ColorRgb* slowColor)
{
	scaledAverage = _scaledAverage;
	averageColor = mtWorking._averageColor;
	currentMax = _currentMax;
	if (fastColor != NULL)
		*fastColor = mtWorking._fastColor;
	if (slowColor != NULL)
		*slowColor = mtWorking._slowColor;
	return _validData;
}

uint32_t SoundCaptureResult::getResultIndex()
{
	return _resultIndex;
}

void SoundCaptureResult::ResetData()
{
	_validData = false;
	_maxAverage = 0;
	_scaledAverage = 0;
	_oldScaledAverage = 0;
	_resultIndex = 0;

	memset(pureResult, 0, sizeof(pureResult));
	memset(lastResult, 0, sizeof(lastResult));

	memset(pureScaledResult, 0, sizeof(pureScaledResult));
	memset(buffScaledResult, 0, sizeof(buffScaledResult));
	memset(maxResult, 0, sizeof(maxResult));

	for (int i = 0; i < SOUNDCAP_RESULT_RES; i++)
		deltas[i] = 10;

	averageDelta = 10;


	_lastPrevColor = ColorRgb(0, 0, 0);
	_currentMax = 0;


	mtWorking.Clear();
	mtInternal.Clear();
}

void SoundCaptureResult::GetBufResult(uint8_t* dest, size_t size) {
	size_t c = std::min(size, sizeof(buffScaledResult));
	memcpy(dest, buffScaledResult, c);
}
