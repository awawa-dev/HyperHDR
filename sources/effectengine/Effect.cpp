/* Effect.cpp
*
*  MIT License
*
*  Copyright (c) 2021 awawa-dev
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

// Qt includes
#include <QDateTime>
#include <QFile>
#include <QResource>
#include <QCoreApplication>

// effect engin eincludes
#include <effectengine/Effect.h>
#include <utils/Logger.h>
#include <hyperhdrbase/HyperHdrInstance.h>
#include <effectengine/Animation_RainbowSwirl.h>
#include <effectengine/Animation_SwirlFast.h>
#include <effectengine/Animation_AtomicSwirl.h>
#include <effectengine/Animation_Candle.h>
#include <effectengine/Animation_DoubleSwirl.h>
#include <effectengine/Animation_KnightRider.h>
#include <effectengine/Animation_Plasma.h>
#include <effectengine/Animation_PoliceLightsSolid.h>
#include <effectengine/Animation_PoliceLightsSingle.h>
#include <effectengine/Animation_SeaWaves.h>
#include <effectengine/Animation_WavesWithColor.h>
#include <effectengine/Animation_RedMoodBlobs.h>
#include <effectengine/Animation_ColdMoodBlobs.h>
#include <effectengine/Animation_BlueMoodBlobs.h>
#include <effectengine/Animation_FullColorMoodBlobs.h>
#include <effectengine/Animation_GreenMoodBlobs.h>
#include <effectengine/Animation_WarmMoodBlobs.h>
#include <effectengine/Animation_Breath.h>
#include <effectengine/Animation_CinemaBrightenLights.h>
#include <effectengine/Animation_CinemaDimLights.h>
#include <effectengine/Animation_NotifyBlue.h>
#include <effectengine/Animation_Sparks.h>
#include <effectengine/Animation_StrobeRed.h>
#include <effectengine/Animation_StrobeWhite.h>
#include <effectengine/Animation_SystemShutdown.h>
#include <effectengine/Animation4Music_TestEq.h>
#include <effectengine/Animation4Music_PulseWhite.h>
#include <effectengine/Animation4Music_PulseBlue.h>
#include <effectengine/Animation4Music_PulseGreen.h>
#include <effectengine/Animation4Music_PulseRed.h>
#include <effectengine/Animation4Music_PulseYellow.h>
#include <effectengine/Animation4Music_PulseMulti.h>
#include <effectengine/Animation4Music_PulseMultiFast.h>
#include <effectengine/Animation4Music_PulseMultiSlow.h>

#include <effectengine/Animation4Music_StereoWhite.h>
#include <effectengine/Animation4Music_StereoBlue.h>
#include <effectengine/Animation4Music_StereoGreen.h>
#include <effectengine/Animation4Music_StereoRed.h>
#include <effectengine/Animation4Music_StereoYellow.h>
#include <effectengine/Animation4Music_StereoMulti.h>
#include <effectengine/Animation4Music_StereoMultiFast.h>
#include <effectengine/Animation4Music_StereoMultiSlow.h>

#include <effectengine/Animation4Music_QuatroWhite.h>
#include <effectengine/Animation4Music_QuatroBlue.h>
#include <effectengine/Animation4Music_QuatroGreen.h>
#include <effectengine/Animation4Music_QuatroRed.h>
#include <effectengine/Animation4Music_QuatroYellow.h>
#include <effectengine/Animation4Music_QuatroMulti.h>
#include <effectengine/Animation4Music_QuatroMultiFast.h>
#include <effectengine/Animation4Music_QuatroMultiSlow.h>

#include <effectengine/Animation4Music_WavesPulse.h>
#include <effectengine/Animation4Music_WavesPulseFast.h>
#include <effectengine/Animation4Music_WavesPulseSlow.h>

#include <hyperhdrbase/SoundCapture.h>

Effect::Effect(HyperHdrInstance* hyperhdr, int priority, int timeout, const QString& name, const QJsonObject& args, const QString& imageData)
	: QThread()
	, _hyperhdr(hyperhdr)
	, _priority(priority)
	, _timeout(timeout)
	, _name(name)
	, _args(args)
	, _imageData(imageData)
	, _endTime(-1)
	, _colors()
	, _imageSize(hyperhdr->getLedGridSize())
	, _image(_imageSize, QImage::Format_ARGB32_Premultiplied)
	, _painter(NULL)
	, _effect(NULL)
	, _soundHandle(0)
{
	_colors.resize(_hyperhdr->getLedCount());
	_colors.fill(ColorRgb::BLACK);

	_log = Logger::getInstance(QString("EFFECT%1(%2)").arg(hyperhdr->getInstanceIndex()).arg((name.length() > 9) ? name.left(6) + "..." : name));

	// init effect image for image based effects, size is based on led layout
	_image.fill(Qt::black);


	if (name == ANIM_RAINBOW_SWIRL)
	{
		_effect = new Animation_RainbowSwirl();
	}
	else if (name == ANIM_SWIRL_FAST)
	{
		_effect = new Animation_SwirlFast();
	}
	else if (name == ANIM_ATOMIC_SWIRL)
	{
		_effect = new Animation_AtomicSwirl();
	}
	else if (name == ANIM_DOUBLE_SWIRL)
	{
		_effect = new Animation_DoubleSwirl();
	}
	else if (name == ANIM_KNIGHT_RIDER)
	{
		_effect = new Animation_KnightRider();
	}
	else if (name == ANIM_PLASMA)
	{
		_effect = new Animation_Plasma();
	}
	else if (name == ANIM_POLICELIGHTSSINGLE)
	{
		_effect = new Animation_PoliceLightsSingle();
	}
	else if (name == ANIM_POLICELIGHTSSOLID)
	{
		_effect = new Animation_PoliceLightsSolid();
	}
	else if (name == ANIM_WAVESWITHCOLOR)
	{
		_effect = new Animation_WavesWithColor();
	}
	else if (name == ANIM_SEAWAVES)
	{
		_effect = new Animation_SeaWaves();
	}
	else if (name == ANIM_RED_MOOD_BLOBS)
	{
		_effect = new Animation_RedMoodBlobs();
	}
	else if (name == ANIM_COLD_MOOD_BLOBS)
	{
		_effect = new Animation_ColdMoodBlobs();
	}
	else if (name == ANIM_BLUE_MOOD_BLOBS)
	{
		_effect = new Animation_BlueMoodBlobs();
	}
	else if (name == ANIM_FULLCOLOR_MOOD_BLOBS)
	{
		_effect = new Animation_FullColorMoodBlobs();
	}
	else if (name == ANIM_GREEN_MOOD_BLOBS)
	{
		_effect = new Animation_GreenMoodBlobs();
	}
	else if (name == ANIM_WARM_MOOD_BLOBS)
	{
		_effect = new Animation_WarmMoodBlobs();
	}
	else if (name == ANIM_BREATH)
	{
		_effect = new Animation_Breath();
	}
	else if (name == ANIM_CINEMA_BRIGHTEN_LIGHTS)
	{
		_effect = new Animation_CinemaBrightenLights();
	}
	else if (name == ANIM_CINEMA_DIM_LIGHTS)
	{
		_effect = new Animation_CinemaDimLights();
	}
	else if (name == ANIM_NOTIFY_BLUE)
	{
		_effect = new Animation_NotifyBlue();
	}
	else if (name == ANIM_STROBE_RED)
	{
		_effect = new Animation_StrobeRed();
	}
	else if (name == ANIM_SPARKS)
	{
		_effect = new Animation_Sparks();
	}
	else if (name == ANIM_STROBE_WHITE)
	{
		_effect = new Animation_StrobeWhite();
	}
	else if (name == ANIM_SYSTEM_SHUTDOWN)
	{
		_effect = new Animation_SystemShutdown();
	}
	else if (name == ANIM_CANDLE)
	{
		_effect = new Animation_Candle();
	}
	else if (name == AMUSIC_TESTEQ)
	{
		_effect = new Animation4Music_TestEq();
	}
	else if (name == AMUSIC_PULSEWHITE)
	{
		_effect = new Animation4Music_PulseWhite();
	}
	else if (name == AMUSIC_PULSEYELLOW)
	{
		_effect = new Animation4Music_PulseYellow();
	}
	else if (name == AMUSIC_PULSERED)
	{
		_effect = new Animation4Music_PulseRed();
	}
	else if (name == AMUSIC_PULSEGREEN)
	{
		_effect = new Animation4Music_PulseGreen();
	}
	else if (name == AMUSIC_PULSEBLUE)
	{
		_effect = new Animation4Music_PulseBlue();
	}
	else if (name == AMUSIC_PULSEMULTI)
	{
		_effect = new Animation4Music_PulseMulti();
	}
	else if (name == AMUSIC_PULSEMULTIFAST)
	{
		_effect = new Animation4Music_PulseMultiFast();
	}
	else if (name == AMUSIC_PULSEMULTISLOW)
	{
		_effect = new Animation4Music_PulseMultiSlow();
	}


	else if (name == AMUSIC_STEREOWHITE)
	{
		_effect = new Animation4Music_StereoWhite();
	}
	else if (name == AMUSIC_STEREOYELLOW)
	{
		_effect = new Animation4Music_StereoYellow();
	}
	else if (name == AMUSIC_STEREORED)
	{
		_effect = new Animation4Music_StereoRed();
	}
	else if (name == AMUSIC_STEREOGREEN)
	{
		_effect = new Animation4Music_StereoGreen();
	}
	else if (name == AMUSIC_STEREOBLUE)
	{
		_effect = new Animation4Music_StereoBlue();
	}
	else if (name == AMUSIC_STEREOMULTI)
	{
		_effect = new Animation4Music_StereoMulti();
	}
	else if (name == AMUSIC_STEREOMULTIFAST)
	{
		_effect = new Animation4Music_StereoMultiFast();
	}
	else if (name == AMUSIC_STEREOMULTISLOW)
	{
		_effect = new Animation4Music_StereoMultiSlow();
	}


	else if (name == AMUSIC_QUATROWHITE)
	{
		_effect = new Animation4Music_QuatroWhite();
	}
	else if (name == AMUSIC_QUATROYELLOW)
	{
		_effect = new Animation4Music_QuatroYellow();
	}
	else if (name == AMUSIC_QUATRORED)
	{
		_effect = new Animation4Music_QuatroRed();
	}
	else if (name == AMUSIC_QUATROGREEN)
	{
		_effect = new Animation4Music_QuatroGreen();
	}
	else if (name == AMUSIC_QUATROBLUE)
	{
		_effect = new Animation4Music_QuatroBlue();
	}
	else if (name == AMUSIC_QUATROMULTI)
	{
		_effect = new Animation4Music_QuatroMulti();
	}
	else if (name == AMUSIC_QUATROMULTIFAST)
	{
		_effect = new Animation4Music_QuatroMultiFast();
	}
	else if (name == AMUSIC_QUATROMULTISLOW)
	{
		_effect = new Animation4Music_QuatroMultiSlow();
	}

	else if (name == AMUSIC_WAVESPULSE)
	{
		_effect = new Animation4Music_WavesPulse();
	}
	else if (name == AMUSIC_WAVESPULSEFAST)
	{
		_effect = new Animation4Music_WavesPulseFast();
	}
	else if (name == AMUSIC_WAVESPULSESLOW)
	{
		_effect = new Animation4Music_WavesPulseSlow();
	}
}

Effect::~Effect()
{
	Info(_log, "Deleting effect named: '%s'", QSTRING_CSTR(_name));

	requestInterruption();

	while (!isFinished())
	{
		QCoreApplication::processEvents(QEventLoop::AllEvents, 15);
	}

	if (_effect != nullptr)
	{
		delete _effect;
		_effect = nullptr;
	}

	if (_painter != nullptr)
	{
		delete _painter;
		_painter = nullptr;
	}

	Info(_log, "Effect named: '%s' is deleted", QSTRING_CSTR(_name));
}

void Effect::run()
{
	if (_effect == NULL)
	{
		Error(_log, "Unable to find effect by this name. Please review configuration. Effect name: '%s'", QSTRING_CSTR(_name));
		return;
	}

	int latchTime = 10;
	int ledCount = 0;
	QMetaObject::invokeMethod(_hyperhdr, "getLedCount", Qt::BlockingQueuedConnection, Q_RETURN_ARG(int, ledCount));

	_ledBuffer.resize(ledCount);

	_effect->Init(_image, latchTime);

	_painter = new QPainter(&_image);

	if (_timeout > 0)
	{
		_endTime = QDateTime::currentMSecsSinceEpoch() + _timeout;
	}

	if (_effect->isSoundEffect())
	{
		QMetaObject::invokeMethod(SoundCapture::getInstance(), "getCaptureInstance", Qt::BlockingQueuedConnection, Q_RETURN_ARG(uint32_t, _soundHandle));
	}

	Info(_log, "Begin playing the effect with priority: %i", _priority);
	while (!_interupt && (_timeout <= 0 || QDateTime::currentMSecsSinceEpoch() < _endTime))
	{
		if (_priority > 0)
		{
			int currentPriority = 0;
			QMetaObject::invokeMethod(_hyperhdr, "getCurrentPriority", Qt::BlockingQueuedConnection, Q_RETURN_ARG(int, currentPriority));

			if (currentPriority < _priority)
			{
				QCoreApplication::processEvents(QEventLoop::AllEvents, 15);
				if (!_interupt && (_timeout <= 0 || QDateTime::currentMSecsSinceEpoch() < _endTime))
					QThread::msleep(500);
				continue;
			}
		}

		bool   hasLedData = false;

		if (!_effect->hasOwnImage())
		{
			_effect->Play(_painter);

			hasLedData = _effect->hasLedData(_ledBuffer);
		}

		int    micro = _effect->GetSleepTime();
		qint64 dieTime = QDateTime::currentMSecsSinceEpoch() + micro;

		if (_effect->hasOwnImage())
		{
			Image<ColorRgb> image(80, 45);
			int timeout = _timeout;
			if (timeout > 0)
			{
				timeout = _endTime - QDateTime::currentMSecsSinceEpoch();
				if (timeout <= 0)
					break;
			}

			if (_effect->getImage(image))
				emit setInputImage(_priority, image, timeout, false);
		}
		else if (hasLedData)
		{
			if (!LedShow())
				break;
		}
		else
		{
			ImageShow();
		}

		if (_effect->isStop())
			break;

		while (!_interupt && QDateTime::currentMSecsSinceEpoch() < dieTime &&
			(_timeout <= 0 || QDateTime::currentMSecsSinceEpoch() < _endTime))
		{
			micro = dieTime - QDateTime::currentMSecsSinceEpoch();
			while (micro > 200)
				micro /= 2;

			if (micro > 0)
				QThread::msleep(micro);
		}
	}

	Info(_log, "The effect quits with priority: %i", _priority);

	if (_soundHandle != 0)
	{
		Info(_log, "Releasing sound handle %i for effect named: '%s'", _soundHandle, QSTRING_CSTR(_name));
		QMetaObject::invokeMethod(SoundCapture::getInstance(), "releaseCaptureInstance", Qt::QueuedConnection, Q_ARG(uint32_t, _soundHandle));
		_soundHandle = 0;
	}
}

bool Effect::LedShow()
{
	if (_interupt)
		return false;

	int timeout = _timeout;
	if (timeout > 0)
	{
		timeout = _endTime - QDateTime::currentMSecsSinceEpoch();
		if (timeout <= 0)
			return false;
	}

	int ledCount = 0;
	QMetaObject::invokeMethod(_hyperhdr, "getLedCount", Qt::BlockingQueuedConnection, Q_RETURN_ARG(int, ledCount));


	if (ledCount == _ledBuffer.length())
	{
		QVector<ColorRgb> _cQV = _ledBuffer;
		emit setInput(_priority, std::vector<ColorRgb>(_cQV.begin(), _cQV.end()), timeout, false);
	}
	else
	{
		Warning(_log, "Mismatch led number detected for the effect");
		return false;
	}

	return true;
}

bool Effect::ImageShow()
{
	if (_interupt)
		return false;

	int timeout = _timeout;
	if (timeout > 0)
	{
		timeout = _endTime - QDateTime::currentMSecsSinceEpoch();
		if (timeout <= 0)
			return false;
	}

	int width = _image.width();
	int height = _image.height();

	Image<ColorRgb> image(width, height);
	QByteArray binaryImage;

	for (int i = 0; i < height; ++i)
	{
		const QRgb* scanline = reinterpret_cast<const QRgb*>(_image.scanLine(i));
		for (int j = 0; j < width; ++j)
		{
			binaryImage.append((char)qRed(scanline[j]));
			binaryImage.append((char)qGreen(scanline[j]));
			binaryImage.append((char)qBlue(scanline[j]));
		}
	}

	memcpy(image.memptr(), binaryImage.data(), binaryImage.size());
	emit setInputImage(_priority, image, timeout, false);

	return true;
}


int  Effect::getPriority() const {
	return _priority;
}

void Effect::requestInterruption() {
	_interupt = true;
}

bool Effect::isInterruptionRequested() {
	return _interupt;
}

QString Effect::getName()     const {
	return _name;
}

int Effect::getTimeout()      const {
	return _timeout;
}

QJsonObject Effect::getArgs() const {
	return _args;
}
