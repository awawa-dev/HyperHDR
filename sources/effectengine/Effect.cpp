/* Effect.cpp
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
	#include <QFile>
	#include <QResource>
#endif

// effect engin eincludes
#include <effectengine/Effect.h>
#include <utils/Logger.h>
#include <base/HyperHdrInstance.h>
#include <effectengine/Animation_RainbowSwirl.h>
#include <effectengine/Animation_RainbowWaves.h>
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

#include <utils/GlobalSignals.h>

Effect::Effect(HyperHdrInstance* hyperhdr, int visiblePriority, int priority, int timeout, const EffectDefinition& effect)
	: QObject()
	, _visiblePriority(visiblePriority)
	, _priority(priority)
	, _timeout(timeout)
	, _instanceIndex(hyperhdr->getInstanceIndex())
	, _name(effect.name)
	, _effect(effect.factory())
	, _endTime(-1)
	, _interrupt(false)
	, _image(hyperhdr->getLedGridSize())	
	, _timer(this)
	, _ledCount(hyperhdr->getLedCount())
{
	_log = Logger::getInstance(QString("EFFECT%1(%2)").arg(_instanceIndex).arg((_name.length() > 9) ? _name.left(6) + "..." : _name));

	_colors.resize(_ledCount);
	_colors.fill(ColorRgb::BLACK);	

	_timer.setTimerType(Qt::PreciseTimer);
	connect(&_timer, &QTimer::timeout, this, &Effect::run);
}

Effect::~Effect()
{	
	delete _effect;

	Info(_log, "Effect named: '%s' is deleted", QSTRING_CSTR(_name));
}

void Effect::start()
{
	_ledBuffer.resize(_ledCount);

	_effect->Init(_image, 10);

	if (_timeout > 0)
		_endTime = InternalClock::now() + _timeout;
	else
		_endTime = -1;	

	_timer.setInterval(_effect->GetSleepTime());

	Info(_log, "Begin playing the %s with priority: %i", (_effect->isSoundEffect()) ? "music effect" : "effect", _priority);

	run();
	_timer.start();	
}

void Effect::run()
{
	int left = (_timeout >= 0) ? std::max(static_cast<int>(_endTime - InternalClock::now()), 0) : -1;

	if (_interrupt || (left == 0 && _timeout > 0) || _effect->isStop())
	{
		stop();
		return;
	}

	if (_visiblePriority < _priority)
		return;

	bool   hasLedData = false;

	if (!_effect->hasOwnImage())
	{
		_effect->Play(_image);

		hasLedData = _effect->hasLedData(_ledBuffer);
	}

	if (_effect->hasOwnImage())
	{
		Image<ColorRgb> image(80, 45);
			
		if (_effect->getImage(image))
			emit SignalSetImage(_priority, image, left, false);
	}
	else if (hasLedData)
	{
		ledShow(left);
	}
	else
	{
		imageShow(left);
	}

	int sleepTime = std::min(_effect->GetSleepTime(), 100);
	if (sleepTime != _timer.interval())
	{
		_timer.setInterval(sleepTime);
	}
}

void Effect::stop()
{
	Info(_log, "The effect quits with priority: %i", _priority);

	_timer.stop();

	emit SignalEffectFinished(_priority, _name, _interrupt);
}

void Effect::ledShow(int left)
{
	if (_ledCount == _ledBuffer.length())
	{
		QVector<ColorRgb> _cQV = _ledBuffer;
		emit SignalSetLeds(_priority, std::vector<ColorRgb>(_cQV.begin(), _cQV.end()), left, false);
	}
	else
	{
		Warning(_log, "Mismatch led number detected for the effect");
		_ledBuffer.resize(_ledCount);
	}
}

void Effect::imageShow(int left)
{
	Image<ColorRgb> image = _image.renderImage();

	emit SignalSetImage(_priority, image, left, false);
}

void Effect::visiblePriorityChanged(quint8 priority)
{
	_visiblePriority = priority;

	if (_timeout <= 0)
	{
		if (_visiblePriority < _priority)
			_timer.stop();
		else if (!_timer.isActive())
			_timer.start();
	}
}

void Effect::setLedCount(int newCount)
{
	_ledCount = newCount;
}

int Effect::getPriority() const {
	return _priority;
}

void Effect::requestInterruption() {
	_interrupt = true;
}

QString Effect::getName()     const {
	return _name;
}

int Effect::getTimeout()      const {
	return _timeout;
}

QString Effect::getDescription() const
{
	return QString("effect%1/%2 => \"%3\"").		
		arg(_instanceIndex).
		arg(_priority).
		arg(_name);
}


std::list<EffectDefinition> Effect::getAvailableEffects()
{
	std::list<EffectDefinition> _availableEffects;

	_availableEffects.push_back(Animation4Music_WavesPulse::getDefinition());

	_availableEffects.push_back(Animation4Music_WavesPulseFast::getDefinition());

	_availableEffects.push_back(Animation4Music_WavesPulseSlow::getDefinition());

	_availableEffects.push_back(Animation4Music_PulseMulti::getDefinition());

	_availableEffects.push_back(Animation4Music_PulseMultiFast::getDefinition());

	_availableEffects.push_back(Animation4Music_PulseMultiSlow::getDefinition());

	_availableEffects.push_back(Animation4Music_PulseYellow::getDefinition());

	_availableEffects.push_back(Animation4Music_PulseWhite::getDefinition());

	_availableEffects.push_back(Animation4Music_PulseRed::getDefinition());

	_availableEffects.push_back(Animation4Music_PulseGreen::getDefinition());

	_availableEffects.push_back(Animation4Music_PulseBlue::getDefinition());


	_availableEffects.push_back(Animation4Music_StereoMulti::getDefinition());

	_availableEffects.push_back(Animation4Music_StereoMultiFast::getDefinition());

	_availableEffects.push_back(Animation4Music_StereoMultiSlow::getDefinition());

	_availableEffects.push_back(Animation4Music_StereoYellow::getDefinition());

	_availableEffects.push_back(Animation4Music_StereoWhite::getDefinition());

	_availableEffects.push_back(Animation4Music_StereoRed::getDefinition());

	_availableEffects.push_back(Animation4Music_StereoGreen::getDefinition());

	_availableEffects.push_back(Animation4Music_StereoBlue::getDefinition());


	_availableEffects.push_back(Animation4Music_QuatroMulti::getDefinition());

	_availableEffects.push_back(Animation4Music_QuatroMultiFast::getDefinition());

	_availableEffects.push_back(Animation4Music_QuatroMultiSlow::getDefinition());

	_availableEffects.push_back(Animation4Music_QuatroYellow::getDefinition());

	_availableEffects.push_back(Animation4Music_QuatroWhite::getDefinition());

	_availableEffects.push_back(Animation4Music_QuatroRed::getDefinition());

	_availableEffects.push_back(Animation4Music_QuatroGreen::getDefinition());

	_availableEffects.push_back(Animation4Music_QuatroBlue::getDefinition());




	_availableEffects.push_back(Animation4Music_TestEq::getDefinition());

	_availableEffects.push_back(Animation_AtomicSwirl::getDefinition());

	_availableEffects.push_back(Animation_BlueMoodBlobs::getDefinition());

	_availableEffects.push_back(Animation_Breath::getDefinition());

	_availableEffects.push_back(Animation_Candle::getDefinition());

	_availableEffects.push_back(Animation_CinemaBrightenLights::getDefinition());

	_availableEffects.push_back(Animation_CinemaDimLights::getDefinition());

	_availableEffects.push_back(Animation_ColdMoodBlobs::getDefinition());

	_availableEffects.push_back(Animation_DoubleSwirl::getDefinition());

	_availableEffects.push_back(Animation_FullColorMoodBlobs::getDefinition());

	_availableEffects.push_back(Animation_GreenMoodBlobs::getDefinition());

	_availableEffects.push_back(Animation_KnightRider::getDefinition());

	_availableEffects.push_back(Animation_NotifyBlue::getDefinition());

	_availableEffects.push_back(Animation_Plasma::getDefinition());

	_availableEffects.push_back(Animation_PoliceLightsSingle::getDefinition());

	_availableEffects.push_back(Animation_PoliceLightsSolid::getDefinition());

	_availableEffects.push_back(Animation_RainbowSwirl::getDefinition());

	_availableEffects.push_back(Animation_SwirlFast::getDefinition());

	_availableEffects.push_back(Animation_RainbowWaves::getDefinition());

	_availableEffects.push_back(Animation_RedMoodBlobs::getDefinition());

	_availableEffects.push_back(Animation_SeaWaves::getDefinition());

	_availableEffects.push_back(Animation_Sparks::getDefinition());

	_availableEffects.push_back(Animation_StrobeRed::getDefinition());

	_availableEffects.push_back(Animation_StrobeWhite::getDefinition());

	_availableEffects.push_back(Animation_SystemShutdown::getDefinition());

	_availableEffects.push_back(Animation_WarmMoodBlobs::getDefinition());

	_availableEffects.push_back(Animation_WavesWithColor::getDefinition());

	return _availableEffects;
}
