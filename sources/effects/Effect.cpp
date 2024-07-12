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
#include <effects/Effect.h>
#include <effects/EffectManufactory.h>
#include <utils/Logger.h>
#include <base/HyperHdrInstance.h>
#include <base/SoundCapture.h>
#include <utils/GlobalSignals.h>
#include <effects/AnimationBaseMusic.h>

Effect::Effect(HyperHdrInstance* hyperhdr, int visiblePriority, int priority, int timeout, const EffectDefinition& effect)
	: QObject()
	, _visiblePriority(visiblePriority)
	, _priority(priority)
	, _timeout(timeout)
	, _instanceIndex(hyperhdr->getInstanceIndex())
	, _name(QString::fromStdString(effect.name))
	, _effect(effect.factory())
	, _soundHandle(0)
	, _soundCapture(nullptr)
	, _endTime(-1)
	, _interrupt(false)
	, _image(hyperhdr->getLedGridSize().width(), hyperhdr->getLedGridSize().height())
	, _timer(this)
	, _ledCount(hyperhdr->getLedCount())
{
	_log = Logger::getInstance(QString("EFFECT%1(%2)").arg(_instanceIndex).arg((_name.length() > 9) ? _name.left(6) + "..." : _name));

	_colors.resize(_ledCount);
	_colors.fill(ColorRgb::BLACK);	

	_timer.setTimerType(Qt::PreciseTimer);
	connect(&_timer, &QTimer::timeout, this, &Effect::run);

	_isSoundEffect = _effect->isSoundEffect();

	if (_isSoundEffect)
	{
		emit GlobalSignals::getInstance()->SignalGetSoundCapture(_soundCapture);
		if (_soundCapture == nullptr)
		{
			Error(_log, "Could not access the sound grabber");
			requestInterruption();
		}
		else
		{
			auto musicEffect = dynamic_cast<AnimationBaseMusic*>(_effect);
			if (musicEffect != nullptr)
			{
				musicEffect->hasResult = [&](AnimationBaseMusic* effect, uint32_t& lastIndex, bool* newAverage, bool* newSlow, bool* newFast, int* isMulti){
					return _soundCapture->hasResult(effect, lastIndex, newAverage, newSlow, newFast, isMulti);
				};
			}
			else
			{
				Error(_log, "Could not cast the object as music effect. The effect definition is incorrect");
				requestInterruption();
			}
			SAFE_CALL_0_RET(_soundCapture.get(), open, uint32_t, _soundHandle);
		}
	}
}

Effect::~Effect()
{	
	delete _effect;

	if (_isSoundEffect && _soundCapture != nullptr)
	{
		QUEUE_CALL_1(_soundCapture.get(), close, uint32_t, _soundHandle);
	}

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

	Info(_log, "Begin playing the %s with priority: %i", (_isSoundEffect) ? "music effect" : "effect", _priority);

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
	if (_ledCount == static_cast<int>(_ledBuffer.size()))
	{
		emit SignalSetLeds(_priority, _ledBuffer, left, false);
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
	return hyperhdr::effects::GET_ALL_EFFECTS();
}
