// Qt includes
#include <QDateTime>
#include <QTimer>

#include <hyperhdrbase/LinearColorSmoothing.h>
#include <hyperhdrbase/HyperHdrInstance.h>

#include <cmath>

using namespace hyperhdr;

const int64_t  DEFAUL_SETTLINGTIME    = 200;	// settlingtime in ms
const double   DEFAUL_UPDATEFREQUENCY = 25;	// updatefrequncy in hz
const int64_t  DEFAUL_UPDATEINTERVALL = static_cast<int64_t>(1000 / DEFAUL_UPDATEFREQUENCY); // updateintervall in ms
const unsigned DEFAUL_OUTPUTDEPLAY    = 0;	// outputdelay in ms

LinearColorSmoothing::LinearColorSmoothing(const QJsonDocument& config, HyperHdrInstance* hyperhdr)
	: QObject(hyperhdr)
	, _log(Logger::getInstance("SMOOTHING"))
	, _semaphore(1)
	, _hyperion(hyperhdr)
	, _updateInterval(DEFAUL_UPDATEINTERVALL)
	, _settlingTime(DEFAUL_SETTLINGTIME)
	, _timer(this)
	, _outputDelay(DEFAUL_OUTPUTDEPLAY)
	, _writeToLedsEnable(false)
	, _continuousOutput(false)
	, _pause(false)
	, _currentConfigId(0)
	, _enabled(false)
	, _directMode(false)
{
	// init cfg 0 (default)
	addConfig(DEFAUL_SETTLINGTIME, DEFAUL_UPDATEFREQUENCY, DEFAUL_OUTPUTDEPLAY);
	handleSettingsUpdate(settings::type::SMOOTHING, config);
	selectConfig(0, true);

	// add pause on cfg 1
	SmoothingCfg cfg (true, 0, 0, 0, false);
	_cfgList.append(cfg);

	// listen for comp changes
	connect(_hyperion, &HyperHdrInstance::compStateChangeRequest, this, &LinearColorSmoothing::componentStateChange);

	// timer
	_timer.setTimerType(Qt::PreciseTimer);
	connect(&_timer, &QTimer::timeout, this, &LinearColorSmoothing::updateLeds);
}

void LinearColorSmoothing::handleSettingsUpdate(settings::type type, const QJsonDocument& config)
{
	if(type == settings::type::SMOOTHING)
	{		
		QJsonObject obj = config.object();

		if (enabled() != obj["enable"].toBool(true))
		{
			setEnable(obj["enable"].toBool(true));
		}

		_continuousOutput = obj["continuousOutput"].toBool(true);

		SmoothingCfg	cfg (false,
							 static_cast<int64_t>(obj["time_ms"].toInt(DEFAUL_SETTLINGTIME)),
							 static_cast<int64_t>(1000.0/obj["updateFrequency"].toDouble(DEFAUL_UPDATEFREQUENCY)),
							 static_cast<unsigned>(obj["updateDelay"].toInt(DEFAUL_OUTPUTDEPLAY)),
							 false);

		Debug( _log, "Creating smoothing config (%d) => direct mode: %s, pause: %s, settlingTime: %i ms, interval: %i ms (%i Hz), updateDelay: %u ms",
					_currentConfigId, (cfg._directMode)?"true":"false", (cfg._pause)?"true":"false", int32_t(cfg._settlingTime), int32_t(cfg._updateInterval), int32_t(1000.0/cfg._updateInterval), cfg._outputDelay );
		_cfgList[0] = cfg;

		// if current id is 0, we need to apply the settings (forced)
		if( _currentConfigId == 0)
		{		
			selectConfig(0, true);
		}
	}
}

int LinearColorSmoothing::write(const std::vector<ColorRgb> &ledValues)
{
	bool fireTimer = false;	

	if (_directMode)
	{
		_semaphore.acquire();

		_timer.stop();
		_targetValues = ledValues;
		_previousValues.clear();
		
		_semaphore.release();

		if (!_writeToLedsEnable || _pause || _targetValues.size() == 0)
			return 0;

		emit _hyperion->ledDeviceData(_targetValues);
			return 0;
	}

	try
	{
		_semaphore.acquire();		

		_targetTime = QDateTime::currentMSecsSinceEpoch() + _settlingTime;
		_targetValues = ledValues;

		if (!_previousValues.empty() && (_previousValues.size() != _targetValues.size()))
		{
			Warning(_log, "Detect size changed. Previuos value: %d, new value: %d", _previousValues.size(), _targetValues.size());
			_previousValues.clear();			
		}

		// received a new target color
		if (_previousValues.empty() ||
			((QDateTime::currentMSecsSinceEpoch() - _previousTime) > qMax(10 * _updateInterval, (int64_t)1000)))
		{
			// not initialized yet
			_previousTime = QDateTime::currentMSecsSinceEpoch();
			_previousValues = ledValues;
			fireTimer = true;
		}

		_semaphore.release();
	}
	catch (...)
	{
		_semaphore.release();
		throw;
	}

	if (fireTimer)
		_timer.start(_updateInterval);

	return 0;
}

int LinearColorSmoothing::updateLedValues(const std::vector<ColorRgb>& ledValues)
{
	int retval = 0;

	if (!_enabled)
	{
		return -1;
	}
	else
	{
		retval = write(ledValues);
	}

	return retval;
}

void LinearColorSmoothing::updateLeds()
{
	int64_t now = QDateTime::currentMSecsSinceEpoch();
	int64_t deltaTime = _targetTime - now;

	

	try
	{
		_semaphore.acquire();

		if (deltaTime <= 0)
		{
			_previousValues = _targetValues;
			_previousTime = now;

			queueColors(_previousValues);
			_writeToLedsEnable = _continuousOutput;
		}
		else
		{
			_writeToLedsEnable = true;

			float k = 1.0f - 1.0f * deltaTime / (_targetTime - _previousTime);

			int reddif = 0, greendif = 0, bluedif = 0;

			if (_previousValues.size() != _targetValues.size())
			{
				Error(_log, "Detect abnormal state. Previuos value: %d, new value: %d", _previousValues.size(), _targetValues.size());
			}
			else
			{
				for (size_t i = 0; i < _previousValues.size(); ++i)
				{
					ColorRgb& prev = _previousValues[i];
					ColorRgb& target = _targetValues[i];

					reddif = target.red - prev.red;
					greendif = target.green - prev.green;
					bluedif = target.blue - prev.blue;

					prev.red += (reddif < 0 ? -1 : 1) * std::ceil(k * std::abs(reddif));
					prev.green += (greendif < 0 ? -1 : 1) * std::ceil(k * std::abs(greendif));
					prev.blue += (bluedif < 0 ? -1 : 1) * std::ceil(k * std::abs(bluedif));
				}
			}
			_previousTime = now;

			queueColors(_previousValues);
		}

		_semaphore.release();
	}
	catch (...)
	{
		_semaphore.release();
		throw;
	}
}

void LinearColorSmoothing::queueColors(const std::vector<ColorRgb> & ledColors)
{
	if (_outputDelay == 0)
	{
		// No output delay => immediate write
		if ( _writeToLedsEnable && !_pause)
		{
			emit _hyperion->ledDeviceData(ledColors);
		}
	}
	else
	{
		// Push new colors in the delay-buffer
		if ( _writeToLedsEnable )
			_outputQueue.push_back(ledColors);

		// If the delay-buffer is filled pop the front and write to device
		if (_outputQueue.size() > 0 )
		{
			if ( _outputQueue.size() > _outputDelay || !_writeToLedsEnable )
			{
				if (!_pause)
				{
					emit _hyperion->ledDeviceData(_outputQueue.front());
				}
				_outputQueue.pop_front();
			}
		}
	}
}

void LinearColorSmoothing::clearQueuedColors()
{
	try
	{
		_semaphore.acquire();

		_timer.stop();
		_previousValues.clear();
		_targetValues.clear();

		_semaphore.release();
	}
	catch (...)
	{
		_semaphore.release();
		throw;
	}
}

void LinearColorSmoothing::componentStateChange(hyperhdr::Components component, bool state)
{
	_writeToLedsEnable = state;
	if(component == hyperhdr::COMP_LEDDEVICE)
	{
		clearQueuedColors();
	}

	if(component == hyperhdr::COMP_SMOOTHING)
	{
		setEnable(state);
	}
}

void LinearColorSmoothing::setEnable(bool enable)
{
	_enabled = enable;

	if (!_enabled)
	{
		clearQueuedColors();
	}

	// update comp register
	_hyperion->setNewComponentState(hyperhdr::COMP_SMOOTHING, enable);
}

void LinearColorSmoothing::setPause(bool pause)
{
	_pause = pause;
}

unsigned LinearColorSmoothing::addConfig(int settlingTime_ms, double ledUpdateFrequency_hz, unsigned updateDelay, bool directMode)
{
	SmoothingCfg cfg (false, settlingTime_ms, int64_t(1000.0/ledUpdateFrequency_hz), updateDelay, directMode);
	_cfgList.append(cfg);

	return _cfgList.count() - 1;
}

unsigned LinearColorSmoothing::updateConfig(unsigned cfgID, int settlingTime_ms, double ledUpdateFrequency_hz, unsigned updateDelay, bool directMode)
{
	unsigned updatedCfgID = cfgID;
	if ( cfgID < static_cast<unsigned>(_cfgList.count()) )
	{
		SmoothingCfg cfg (false, settlingTime_ms, int64_t(1000.0/ledUpdateFrequency_hz), updateDelay, directMode);
		_cfgList[updatedCfgID] = cfg;
	}
	else
	{
		updatedCfgID = addConfig ( settlingTime_ms, ledUpdateFrequency_hz, updateDelay, directMode);
	}

	return updatedCfgID;
}

bool LinearColorSmoothing::selectConfig(unsigned cfg, bool force)
{
	if (_currentConfigId == cfg && !force)
	{
		return true;
	}

	if ( cfg < (unsigned)_cfgList.count())
	{
		_settlingTime     = _cfgList[cfg]._settlingTime;
		_outputDelay      = _cfgList[cfg]._outputDelay;
		_pause            = _cfgList[cfg]._pause;
		_directMode       = _cfgList[cfg]._directMode;		


		if (_cfgList[cfg]._updateInterval != _updateInterval)
		{
			clearQueuedColors();

			_updateInterval = _cfgList[cfg]._updateInterval;
			
			if ( this->enabled() && this->_writeToLedsEnable )
			{				
				_timer.start(_updateInterval);
			}
		}
		_currentConfigId = cfg;


		Info( _log, "Selecting smoothing config (%d) => direct mode: %s, pause: %s, settlingTime: %i ms, interval: %i ms (%i Hz), updateDelay: %u ms",
					_currentConfigId, (_cfgList[cfg]._directMode)?"true":"false" , (_cfgList[cfg]._pause)?"true":"false", int32_t(_cfgList[cfg]._settlingTime), int32_t(_cfgList[cfg]._updateInterval), int32_t(1000.0/_cfgList[cfg]._updateInterval), _cfgList[cfg]._outputDelay );

		return true;
	}

	// reset to default
	_currentConfigId = 0;
	return false;
}
