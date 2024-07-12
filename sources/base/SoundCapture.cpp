/* SoundCapture.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/HyperHDR
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
 */

#ifndef PCH_ENABLED
	#include <cmath>

	#include <utils/settings.h>
#endif

#include <QMutexLocker>

#include <base/SoundCaptureResult.h>
#include <base/SoundCapture.h>
#include <effects/AnimationBaseMusic.h>

namespace
{
	SoundCaptureResult resultFFT;
	int16_t	imgBuffer[SOUNDCAP_N_WAVE * 2] = {};
}

uint32_t	  SoundCapture::_noSoundCounter = 0;
bool		  SoundCapture::_noSoundWarning = false;
bool		  SoundCapture::_soundDetectedInfo = false;

namespace
{
	QMutex	soundBufferGuard;
}

SoundCapture::SoundCapture(const QJsonDocument& effectConfig, QObject* parent) :
	_isActive(false),
	_enable_smoothing(true),
	_selectedDevice(""),
	_isRunning(false),
	_maxInstance(0)
{
	_logger = Logger::getInstance("SOUND_GRABBER");
	settingsChangedHandler(settings::type::SNDEFFECT, effectConfig);
	qRegisterMetaType<uint32_t>("uint32_t");
}

SoundCapture::~SoundCapture()
{
	Info(_logger, "SoundGrabber is released");
}

QJsonObject SoundCapture::getJsonInfo()
{
	QJsonObject sndgrabber;
	QJsonArray availableSoundGrabbers;

	sndgrabber["active"] = getActive();
	for (auto sndGrabber : getDevices())
	{
		availableSoundGrabbers.append(sndGrabber);
	}
	sndgrabber["device"] = getSelectedDevice();
	sndgrabber["sound_available"] = availableSoundGrabbers;

	return sndgrabber;
}

QString SoundCapture::getSelectedDevice() const
{
	return _selectedDevice;
}

QList<QString> SoundCapture::getDevices() const
{
	return _availableDevices;
}

bool SoundCapture::getActive() const
{
	return _isActive;
}


void SoundCapture::settingsChangedHandler(settings::type type, const QJsonDocument& config)
{
	if (type == settings::type::SNDEFFECT)
	{
		const QJsonObject& sndEffectConfig = config.object();

		_isActive = false;
		_selectedDevice = "";

		if (sndEffectConfig["enable"].toBool(true))
		{
			_enable_smoothing = sndEffectConfig["enable_smoothing"].toBool(true);

			const QString  dev = sndEffectConfig["device"].toString("");
			if (dev.trimmed().length() > 0)
			{
				_selectedDevice = dev;
				_isActive = true;
				Info(_logger, "Sound device '%s' is selected for activation", QSTRING_CSTR(_selectedDevice));
			}
		}
		if (!_isActive)
			Info(_logger, "Sound device is disabled");
	}
}

uint32_t SoundCapture::open()
{
	uint32_t instance = 0;

	try
	{
		instance = ++_maxInstance;
		_instances.append(instance);

		if (!_isRunning)
		{
			if (!_isActive)
				Error(_logger, "Sound device is not configured");
			else
			{
				Info(_logger, "Sound device is starting");

				resultFFT.ResetData();
				_noSoundCounter = 0;
				_noSoundWarning = false;
				_soundDetectedInfo = false;

				start();
				Info(_logger, "Sound device has started (handle: %i)", instance);
			}
		}
		else
			Info(_logger, "Sound device is already running");
	}
	catch (...)
	{

	}

	return instance;
}

void SoundCapture::close(uint32_t instance)
{
	try
	{
		Info(_logger, "Releasing sound grabber (handle: %i)", instance);

		if (_instances.contains(instance))
		{
			_instances.removeOne(instance);

			if (_instances.count() == 0)
			{
				Info(_logger, "The sound device stops");
				stop();
				Info(_logger, "The sound device has been stopped");
			}
		}
	}
	catch (...)
	{

	}
}

bool SoundCapture::analyzeSpectrum(int16_t soundBuffer[], int sizeP)
{
	if (!_isRunning)
		return false;

	QMutexLocker locker(&soundBufferGuard);

	int16_t resolutionP = 10;
	int16_t sec[SOUNDCAP_RESULT_RES] = {
				2 * 2,   // 0-86
				4 * 2,   // 86-258
				6 * 2,   // 258-516
				12 * 2,  // 516-1032
				24 * 2,  // 1032-2064
				48 * 2,  // 2064-4128
				48 * 2,  // 4128-6192
				512    // 6192-11025
	};

	int noSound = 0;
	for (noSound = 0; noSound < pow(2, resolutionP); noSound++)
		if (soundBuffer[noSound] < -8 || soundBuffer[noSound] > 8)
			break;

	if (noSound >= pow(2, resolutionP))
		noSound = 1;
	else
		noSound = 0;

	if ((1 << sizeP) > SOUNDCAP_N_WAVE)
		return false;

	memset(imgBuffer, 0, sizeof(imgBuffer));
	resultFFT.ClearResult();

	if (noSound == 0)
	{
		if (fix_fft(soundBuffer, imgBuffer, resolutionP, 0) < 0)
			return false;

		for (int i = 0, limit = pow(2, resolutionP - 1), samplerIndex = 0, samplerCount = 0; i < limit; i++)
		{
			uint32_t res = std::sqrt(((int32_t)soundBuffer[i]) * soundBuffer[i] + ((int32_t)imgBuffer[i]) * imgBuffer[i]);

			resultFFT.AddResult(samplerIndex, res);
			samplerCount++;

			if (samplerIndex < SOUNDCAP_RESULT_RES - 1)
			{
				if (samplerCount >= sec[samplerIndex])
				{
					samplerCount = 0;
					samplerIndex++;
				}
			}
		}
	}

	if (noSound == 1 && !_noSoundWarning && _noSoundCounter++ > 20)
	{
		_noSoundWarning = true;
		_soundDetectedInfo = false;
		Warning(_logger, "Audio Stream: audio data captured, but it is silent.");
	}

	if (noSound == 0 && !_soundDetectedInfo)
	{
		_noSoundCounter = 0;
		_noSoundWarning = false;
		_soundDetectedInfo = true;
		Info(_logger, "Audio Stream: audio data successfully captured and sound detected.");
	}

	resultFFT.Smooth();

	return true;
}

SoundCaptureResult* SoundCapture::hasResult(AnimationBaseMusic* effect, uint32_t& lastIndex, bool* newAverage, bool* newSlow, bool* newFast, int* isMulti)
{
	QMutexLocker locker(&soundBufferGuard);

	if (lastIndex != resultFFT.getResultIndex())
	{
		lastIndex = resultFFT.getResultIndex();

		effect->store(&resultFFT.mtInternal);

		*isMulti = 2;

		if (newAverage != nullptr)
			*newAverage = true;
		if (newSlow != nullptr)
			*newSlow = true;
		if (newFast != nullptr)
			*newFast = true;

		return &resultFFT;
	}

	effect->restore(&resultFFT.mtWorking);

	if (*isMulti <= 0 || !_enable_smoothing)
	{
		return nullptr;
	}

	(*isMulti)--;

	if (newAverage != nullptr)
		*newAverage = resultFFT.hasMiddleAverage(*isMulti);

	if (newSlow != nullptr)
		*newSlow = resultFFT.hasMiddleSlow(*isMulti);

	if (newFast != nullptr)
		*newFast = resultFFT.hasMiddleFast(*isMulti);

	effect->store(&resultFFT.mtWorking);

	return &resultFFT;
}

/* Code below based on work:
  Tom Roberts  11/8/89
  Made portable:  Malcolm Slaney 12/15/94 malcolm@interval.com
  Enhanced:  Dimitrios P. Bouras  14 Jun 2006 dbouras@ieee.org

  All data are fixed-point short integers, in which -32768
  to +32768 represent -1.0 to +1.0 respectively. Integer
  arithmetic is used for speed, instead of the more natural
  floating-point.
  For the forward FFT (time -> freq), fixed scaling is
  performed to prevent arithmetic overflow, and to map a 0dB
  sine/cosine wave (i.e. amplitude = 32767) to two -6dB freq
  coefficients. The return value is always 0.
  For the inverse FFT (freq -> time), fixed scaling cannot be
  done, as two 0dB coefficients would sum to a peak amplitude
  of 64K, overflowing the 32k range of the fixed-point integers.
  Thus, the fix_fft() routine performs variable scaling, and
  returns a value which is the number of bits LEFT by which
  the output must be shifted to get the actual amplitude
  (i.e. if fix_fft() returns 3, each value of fr[] and fi[]
  must be multiplied by 8 (2**3) for proper scaling.
  Clearly, this cannot be done within fixed-point short
  integers. In practice, if the result is to be used as a
  filter, the scale_shift can usually be ignored, as the
  result will be approximately correctly normalized as is.
  Written by:  Tom Roberts  11/8/89
  Made portable:  Malcolm Slaney 12/15/94 malcolm@interval.com
  Enhanced:  Dimitrios P. Bouras  14 Jun 2006 dbouras@ieee.org
  Data types ungrade, modify variables to packed into the class: awawa-dev 11/Feb/2021
*/

/*
  Henceforth "short" implies 16-bit word. If this is not
  the case in your architecture, please replace "short"
  with a type definition which *is* a 16-bit word.
*/

/*
  Since we only use 3/4 of N_WAVE, we define only
  this many samples, in order to conserve data space.
*/


namespace {
	int16_t lutSin[SOUNDCAP_N_WAVE - SOUNDCAP_N_WAVE / 4] = {
		0,    201,    402,    603,    804,   1005,   1206,   1406,
	 1607,   1808,   2009,   2209,   2410,   2610,   2811,   3011,
	 3211,   3411,   3611,   3811,   4011,   4210,   4409,   4608,
	 4807,   5006,   5205,   5403,   5601,   5799,   5997,   6195,
	 6392,   6589,   6786,   6982,   7179,   7375,   7571,   7766,
	 7961,   8156,   8351,   8545,   8739,   8932,   9126,   9319,
	 9511,   9703,   9895,  10087,  10278,  10469,  10659,  10849,
	11038,  11227,  11416,  11604,  11792,  11980,  12166,  12353,
	12539,  12724,  12909,  13094,  13278,  13462,  13645,  13827,
	14009,  14191,  14372,  14552,  14732,  14911,  15090,  15268,
	15446,  15623,  15799,  15975,  16150,  16325,  16499,  16672,
	16845,  17017,  17189,  17360,  17530,  17699,  17868,  18036,
	18204,  18371,  18537,  18702,  18867,  19031,  19194,  19357,
	19519,  19680,  19840,  20000,  20159,  20317,  20474,  20631,
	20787,  20942,  21096,  21249,  21402,  21554,  21705,  21855,
	22004,  22153,  22301,  22448,  22594,  22739,  22883,  23027,
	23169,  23311,  23452,  23592,  23731,  23869,  24006,  24143,
	24278,  24413,  24546,  24679,  24811,  24942,  25072,  25201,
	25329,  25456,  25582,  25707,  25831,  25954,  26077,  26198,
	26318,  26437,  26556,  26673,  26789,  26905,  27019,  27132,
	27244,  27355,  27466,  27575,  27683,  27790,  27896,  28001,
	28105,  28208,  28309,  28410,  28510,  28608,  28706,  28802,
	28897,  28992,  29085,  29177,  29268,  29358,  29446,  29534,
	29621,  29706,  29790,  29873,  29955,  30036,  30116,  30195,
	30272,  30349,  30424,  30498,  30571,  30643,  30713,  30783,
	30851,  30918,  30984,  31049,  31113,  31175,  31236,  31297,
	31356,  31413,  31470,  31525,  31580,  31633,  31684,  31735,
	31785,  31833,  31880,  31926,  31970,  32014,  32056,  32097,
	32137,  32176,  32213,  32249,  32284,  32318,  32350,  32382,
	32412,  32441,  32468,  32495,  32520,  32544,  32567,  32588,
	32609,  32628,  32646,  32662,  32678,  32692,  32705,  32717,
	32727,  32736,  32744,  32751,  32757,  32761,  32764,  32766,
	32767,  32766,  32764,  32761,  32757,  32751,  32744,  32736,
	32727,  32717,  32705,  32692,  32678,  32662,  32646,  32628,
	32609,  32588,  32567,  32544,  32520,  32495,  32468,  32441,
	32412,  32382,  32350,  32318,  32284,  32249,  32213,  32176,
	32137,  32097,  32056,  32014,  31970,  31926,  31880,  31833,
	31785,  31735,  31684,  31633,  31580,  31525,  31470,  31413,
	31356,  31297,  31236,  31175,  31113,  31049,  30984,  30918,
	30851,  30783,  30713,  30643,  30571,  30498,  30424,  30349,
	30272,  30195,  30116,  30036,  29955,  29873,  29790,  29706,
	29621,  29534,  29446,  29358,  29268,  29177,  29085,  28992,
	28897,  28802,  28706,  28608,  28510,  28410,  28309,  28208,
	28105,  28001,  27896,  27790,  27683,  27575,  27466,  27355,
	27244,  27132,  27019,  26905,  26789,  26673,  26556,  26437,
	26318,  26198,  26077,  25954,  25831,  25707,  25582,  25456,
	25329,  25201,  25072,  24942,  24811,  24679,  24546,  24413,
	24278,  24143,  24006,  23869,  23731,  23592,  23452,  23311,
	23169,  23027,  22883,  22739,  22594,  22448,  22301,  22153,
	22004,  21855,  21705,  21554,  21402,  21249,  21096,  20942,
	20787,  20631,  20474,  20317,  20159,  20000,  19840,  19680,
	19519,  19357,  19194,  19031,  18867,  18702,  18537,  18371,
	18204,  18036,  17868,  17699,  17530,  17360,  17189,  17017,
	16845,  16672,  16499,  16325,  16150,  15975,  15799,  15623,
	15446,  15268,  15090,  14911,  14732,  14552,  14372,  14191,
	14009,  13827,  13645,  13462,  13278,  13094,  12909,  12724,
	12539,  12353,  12166,  11980,  11792,  11604,  11416,  11227,
	11038,  10849,  10659,  10469,  10278,  10087,   9895,   9703,
	 9511,   9319,   9126,   8932,   8739,   8545,   8351,   8156,
	 7961,   7766,   7571,   7375,   7179,   6982,   6786,   6589,
	 6392,   6195,   5997,   5799,   5601,   5403,   5205,   5006,
	 4807,   4608,   4409,   4210,   4011,   3811,   3611,   3411,
	 3211,   3011,   2811,   2610,   2410,   2209,   2009,   1808,
	 1607,   1406,   1206,   1005,    804,    603,    402,    201,
		0,   -201,   -402,   -603,   -804,  -1005,  -1206,  -1406,
	-1607,  -1808,  -2009,  -2209,  -2410,  -2610,  -2811,  -3011,
	-3211,  -3411,  -3611,  -3811,  -4011,  -4210,  -4409,  -4608,
	-4807,  -5006,  -5205,  -5403,  -5601,  -5799,  -5997,  -6195,
	-6392,  -6589,  -6786,  -6982,  -7179,  -7375,  -7571,  -7766,
	-7961,  -8156,  -8351,  -8545,  -8739,  -8932,  -9126,  -9319,
	-9511,  -9703,  -9895, -10087, -10278, -10469, -10659, -10849,
   -11038, -11227, -11416, -11604, -11792, -11980, -12166, -12353,
   -12539, -12724, -12909, -13094, -13278, -13462, -13645, -13827,
   -14009, -14191, -14372, -14552, -14732, -14911, -15090, -15268,
   -15446, -15623, -15799, -15975, -16150, -16325, -16499, -16672,
   -16845, -17017, -17189, -17360, -17530, -17699, -17868, -18036,
   -18204, -18371, -18537, -18702, -18867, -19031, -19194, -19357,
   -19519, -19680, -19840, -20000, -20159, -20317, -20474, -20631,
   -20787, -20942, -21096, -21249, -21402, -21554, -21705, -21855,
   -22004, -22153, -22301, -22448, -22594, -22739, -22883, -23027,
   -23169, -23311, -23452, -23592, -23731, -23869, -24006, -24143,
   -24278, -24413, -24546, -24679, -24811, -24942, -25072, -25201,
   -25329, -25456, -25582, -25707, -25831, -25954, -26077, -26198,
   -26318, -26437, -26556, -26673, -26789, -26905, -27019, -27132,
   -27244, -27355, -27466, -27575, -27683, -27790, -27896, -28001,
   -28105, -28208, -28309, -28410, -28510, -28608, -28706, -28802,
   -28897, -28992, -29085, -29177, -29268, -29358, -29446, -29534,
   -29621, -29706, -29790, -29873, -29955, -30036, -30116, -30195,
   -30272, -30349, -30424, -30498, -30571, -30643, -30713, -30783,
   -30851, -30918, -30984, -31049, -31113, -31175, -31236, -31297,
   -31356, -31413, -31470, -31525, -31580, -31633, -31684, -31735,
   -31785, -31833, -31880, -31926, -31970, -32014, -32056, -32097,
   -32137, -32176, -32213, -32249, -32284, -32318, -32350, -32382,
   -32412, -32441, -32468, -32495, -32520, -32544, -32567, -32588,
   -32609, -32628, -32646, -32662, -32678, -32692, -32705, -32717,
   -32727, -32736, -32744, -32751, -32757, -32761, -32764, -32766,
	};
};

/*
  FIX_MPY() - fixed-point multiplication & scaling.
  Substitute inline assembly for hardware-specific
  optimization suited to a particluar DSP processor.
  Scaling ensures that result remains 16-bit.
*/
inline int16_t   SoundCapture::FIX_MPY(int16_t  a, int16_t  b)
{
	/* shift right one less bit (i.e. 15-1) */
	int32_t c = ((int32_t)a * (int32_t)b) >> 14;
	/* last bit shifted out = rounding-bit */
	b = c & 0x01;
	/* last shift + rounding bit */
	a = (c >> 1) + b;
	return a;
}

/*
  fix_fft() - perform forward/inverse fast Fourier transform.
  fr[n],fi[n] are real and imaginary arrays, both INPUT AND
  RESULT (in-place FFT), with 0 <= n < 2**m; set inverse to
  0 for forward transform (FFT), or 1 for iFFT.
*/
int32_t  SoundCapture::fix_fft(int16_t fr[], int16_t fi[], int16_t exp, bool inverse)
{
	int m, mr, nn, i, j, l, k, istep, n, scale, shift;
	short qr, qi, tr, ti, wr, wi;

	n = 1 << exp;

	/* max FFT size = N_WAVE */
	if (n > SOUNDCAP_N_WAVE)
		return -1;

	mr = 0;
	nn = n - 1;
	scale = 0;

	/* decimation in time - re-order data */
	for (m = 1; m <= nn; ++m)
	{
		l = n;
		do {
			l >>= 1;
		} while (mr + l > nn);
		mr = (mr & (l - 1)) + l;

		if (mr <= m)
			continue;

		tr = fr[m];
		fr[m] = fr[mr];
		fr[mr] = tr;
		ti = fi[m];
		fi[m] = fi[mr];
		fi[mr] = ti;
	}

	l = 1;
	k = SOUNDCAP_LOG2_N_WAVE - 1;
	while (l < n) {
		if (inverse)
		{
			/* variable scaling, depending upon data */
			shift = 0;
			for (i = 0; i < n; ++i)
			{
				j = fr[i];
				if (j < 0)
					j = -j;
				m = fi[i];
				if (m < 0)
					m = -m;
				if (j > 16383 || m > 16383) {
					shift = 1;
					break;
				}
			}

			if (shift)
				++scale;
		}
		else
		{
			/*
			  fixed scaling, for proper normalization --
			  there will be log2(n) passes, so this results
			  in an overall factor of 1/n, distributed to
			  maximize arithmetic accuracy.
			*/
			shift = 1;
		}
		/*
		  it may not be obvious, but the shift will be
		  performed on each data point exactly once,
		  during this pass.
		*/
		istep = l << 1;
		for (m = 0; m < l; ++m)
		{
			j = m << k;
			/* 0 <= j < N_WAVE/2 */
			wr = lutSin[j + SOUNDCAP_N_WAVE / 4];
			wi = -lutSin[j];

			if (inverse)
				wi = -wi;

			if (shift)
			{
				wr >>= 1;
				wi >>= 1;
			}

			for (i = m; i < n; i += istep)
			{
				j = i + l;
				tr = FIX_MPY(wr, fr[j]) - FIX_MPY(wi, fi[j]);
				ti = FIX_MPY(wr, fi[j]) + FIX_MPY(wi, fr[j]);
				qr = fr[i];
				qi = fi[i];
				if (shift)
				{
					qr >>= 1;
					qi >>= 1;
				}
				fr[j] = qr - tr;
				fi[j] = qi - ti;
				fr[i] = qr + tr;
				fi[i] = qi + ti;
			}
		}
		--k;
		l = istep;
	}
	return scale;
}
