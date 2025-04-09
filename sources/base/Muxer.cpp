#ifndef PCH_ENABLED
	#include <QTimer>
	#include <QDebug>

	#include <algorithm>
	#include <limits>
#endif

#include <utils/Logger.h>
#include <base/Muxer.h>

#define	TIMEOUT_INACTIVE -100

const int Muxer::LOWEST_PRIORITY = std::numeric_limits<uint8_t>::max();
const int Muxer::HIGHEST_EFFECT_PRIORITY = 0;
const int Muxer::LOWEST_EFFECT_PRIORITY = 254;

Muxer::Muxer(int instanceIndex, int ledCount, QObject* parent)
	: QObject(parent)
	, _log(Logger::getInstance(QString("MUXER%1").arg(instanceIndex)))
	, _currentPriority(Muxer::LOWEST_PRIORITY)
	, _previousPriority(_currentPriority)
	, _manualSelectedPriority(256)
	, _activeInputs()
	, _lowestPriorityInfo()
	, _sourceAutoSelectEnabled(true)
	, _updateTimer(new QTimer(this))
	, _timer(new QTimer(this))
	, _blockTimer(new QTimer(this))
{
	// init lowest priority info
	_lowestPriorityInfo.priority = Muxer::LOWEST_PRIORITY;
	_lowestPriorityInfo.timeout = -1;
	_lowestPriorityInfo.staticColor = ColorRgb::BLACK;
	_lowestPriorityInfo.componentId = hyperhdr::COMP_COLOR;
	_lowestPriorityInfo.origin = "System";
	_lowestPriorityInfo.owner = "";

	_activeInputs[Muxer::LOWEST_PRIORITY] = _lowestPriorityInfo;

	// adapt to 1s interval for COLOR and EFFECT timeouts > -1
	connect(_timer, &QTimer::timeout, this, &Muxer::timeTrigger);
	_timer->setSingleShot(true);
	_blockTimer->setSingleShot(true);
	// forward timeRunner signal to prioritiesChanged signal & threading workaround
	connect(this, &Muxer::SignalTimeRunner_Internal, this, &Muxer::SignalPrioritiesChanged);
	connect(this, &Muxer::SignalTimeTrigger_Internal, this, &Muxer::timeTrigger);

	// start muxer timer
	connect(_updateTimer, &QTimer::timeout, this, &Muxer::setCurrentTime);
	_updateTimer->setInterval(250);
	_updateTimer->start();

	Debug(_log, "Muxer initialized");
}

Muxer::~Muxer()
{
	delete _updateTimer;
	delete _timer;
	delete _blockTimer;
}

QList<int> Muxer::getPriorities() const
{
	return _activeInputs.keys();
}

const Muxer::InputInfo& Muxer::getInputInfo(int priority) const
{
	auto elemIt = _activeInputs.find(priority);
	if (elemIt == _activeInputs.end())
	{
		elemIt = _activeInputs.find(Muxer::LOWEST_PRIORITY);
		if (elemIt == _activeInputs.end())
		{
			// fallback
			return _lowestPriorityInfo;
		}
	}
	return elemIt.value();
}

const QMap<int, Muxer::InputInfo>& Muxer::getInputInfoTable() const
{
	return _activeInputs;
}

hyperhdr::Components Muxer::getComponentOfPriority(int priority) const
{
	return _activeInputs[priority].componentId;
}

void Muxer::setEnable(bool enable)
{
	enable ? _updateTimer->start() : _updateTimer->stop();
}

bool Muxer::setInput(int priority, int64_t timeout_ms)
{
	if (!_activeInputs.contains(priority))
	{
		Error(_log, "setInput() used without registerInput() for priority '%d', probably the priority reached timeout", priority);
		return false;
	}

	// calculate final timeout
	if (timeout_ms > 0)
		timeout_ms = InternalClock::now() + timeout_ms;

	InputInfo& input = _activeInputs[priority];

	// detect active <-> inactive changes
	bool activeChange = false, active = true;
	if (input.timeout == TIMEOUT_INACTIVE && timeout_ms != TIMEOUT_INACTIVE)
	{
		activeChange = true;
	}
	else if (timeout_ms == TIMEOUT_INACTIVE && input.timeout != TIMEOUT_INACTIVE)
	{
		active = false;
		activeChange = true;
	}

	// update input
	input.timeout = timeout_ms;

	// emit active change
	if (activeChange)
	{
		Info(_log, "Priority %d is now %s", priority, active ? "active" : "inactive");
		if (_currentPriority < priority)
			emit SignalPrioritiesChanged();
		setCurrentTime();
	}

	return true;
}

bool Muxer::setSourceAutoSelectEnabled(bool enable, bool update)
{
	if (_sourceAutoSelectEnabled != enable)
	{
		// on disable we need to make sure the last priority call to setPriority is still valid
		if (!enable && !_activeInputs.contains(_manualSelectedPriority))
		{
			Warning(_log, "Can't disable auto selection, as the last manual selected priority (%d) is no longer available", _manualSelectedPriority);
			return false;
		}

		_sourceAutoSelectEnabled = enable;
		Debug(_log, "Source auto select is now %s", enable ? "enabled" : "disabled");

		// update _currentPriority if called from external
		if (update)
			setCurrentTime();

		return true;
	}
	return false;
}

bool Muxer::setPriority(int priority)
{
	if (_activeInputs.contains(priority))
	{
		_manualSelectedPriority = priority;
		// update auto select state -> update _currentPriority
		setSourceAutoSelectEnabled(false);
		return true;
	}
	return false;
}

bool Muxer::hasPriority(int priority) const
{
	return (priority == Muxer::LOWEST_PRIORITY) ? true : _activeInputs.contains(priority);
}

void Muxer::registerInput(int priority, hyperhdr::Components component, const QString& origin, const ColorRgb& staticColor, unsigned smooth_cfg, const QString& owner)
{
	// detect new registers
	bool newInput = false;
	bool reusedInput = false;
	if (!_activeInputs.contains(priority))
		newInput = true;
	else if (_prevVisComp == component || _activeInputs[priority].componentId == component)
		reusedInput = true;

	InputInfo& input = _activeInputs[priority];
	input.priority = priority;
	input.timeout = newInput ? TIMEOUT_INACTIVE : input.timeout;
	input.componentId = component;
	input.origin = origin;
	input.smooth_cfg = smooth_cfg;
	input.owner = owner;
	input.staticColor = staticColor;

	if (newInput)
	{
		Info(_log, "Register new input '%s/%s' with priority %d as inactive", QSTRING_CSTR(origin), hyperhdr::componentToIdString(component), priority);
		// emit 'prioritiesChanged' only if _sourceAutoSelectEnabled is false
		if (!_sourceAutoSelectEnabled)
			emit SignalPrioritiesChanged();
		return;
	}

	if (reusedInput && component != hyperhdr::Components::COMP_FLATBUFSERVER)
	{
		emit SignalTimeRunner_Internal();
	}
}

bool Muxer::setInputInactive(int priority)
{
	return setInput(priority, TIMEOUT_INACTIVE);
}

bool Muxer::clearInput(int priority)
{
	if (priority < Muxer::LOWEST_PRIORITY)
	{
		if (_activeInputs.remove(priority))
		{
			Info(_log, "Removed source priority %d", priority);
			// on clear success update _currentPriority
			setCurrentTime();
		}
		if (!_sourceAutoSelectEnabled || _currentPriority < priority)
			emit SignalPrioritiesChanged();
		return true;
	}
	return false;
}

void Muxer::clearAll(bool forceClearAll)
{
	if (forceClearAll)
	{
		_previousPriority = _currentPriority;
		_activeInputs.clear();
		_currentPriority = Muxer::LOWEST_PRIORITY;
		_activeInputs[_currentPriority] = _lowestPriorityInfo;
	}
	else
	{
		for (auto key : _activeInputs.keys())
		{
			const InputInfo& info = getInputInfo(key);
			if ((info.componentId == hyperhdr::COMP_COLOR || info.componentId == hyperhdr::COMP_EFFECT || info.componentId == hyperhdr::COMP_IMAGE) && key < Muxer::LOWEST_PRIORITY - 1)
			{
				clearInput(key);
			}
		}
	}
}

void Muxer::setCurrentTime()
{
	const int64_t now = InternalClock::now();
	int newPriority = Muxer::LOWEST_PRIORITY;

	for (auto infoIt = _activeInputs.begin(); infoIt != _activeInputs.end();)
	{
		if (infoIt->timeout > 0 && infoIt->timeout <= now)
		{
			int tPrio = infoIt->priority;
			infoIt = _activeInputs.erase(infoIt);
			Info(_log, "Timeout clear for priority %d", tPrio);
			emit SignalPrioritiesChanged();
		}
		else
		{
			// timeoutTime of -100 is awaiting data (inactive); skip
			if (infoIt->timeout > TIMEOUT_INACTIVE)
				newPriority = qMin(newPriority, infoIt->priority);

			// call timeTrigger when effect or color is running with timeout > 0, blacklist prio 255
			if (infoIt->priority < Muxer::LOWEST_EFFECT_PRIORITY &&
				infoIt->timeout > 0 &&
				(infoIt->componentId == hyperhdr::COMP_EFFECT || infoIt->componentId == hyperhdr::COMP_COLOR || infoIt->componentId == hyperhdr::COMP_IMAGE))
				emit SignalTimeTrigger_Internal(); // as signal to prevent Threading issues

			++infoIt;
		}
	}
	// evaluate, if manual selected priority is still available
	if (!_sourceAutoSelectEnabled)
	{
		if (_activeInputs.contains(_manualSelectedPriority))
		{
			newPriority = _manualSelectedPriority;
		}
		else
		{
			Debug(_log, "The manual selected priority '%d' is no longer available, switching to auto selection", _manualSelectedPriority);
			// update state, but no _currentPriority re-eval
			setSourceAutoSelectEnabled(true, false);
		}
	}
	// apply & emit on change (after apply!)
	hyperhdr::Components comp = getComponentOfPriority(newPriority);
	if (_currentPriority != newPriority || comp != _prevVisComp)
	{
		_previousPriority = _currentPriority;
		_currentPriority = newPriority;
		Info(_log, "Set visible priority to %d", newPriority);
		emit SignalVisiblePriorityChanged(newPriority);
		// check for visible comp change
		if (comp != _prevVisComp)
		{
			_prevVisComp = comp;
			emit SignalVisibleComponentChanged(comp);
		}
		emit SignalPrioritiesChanged();
	}
}

void Muxer::timeTrigger()
{
	if (_blockTimer->isActive())
	{
		_timer->start(500);
	}
	else
	{
		emit SignalTimeRunner_Internal();
		_blockTimer->start(1000);
	}
}
