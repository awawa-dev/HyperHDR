#include <led-drivers/net/DriverNetYeelight.h>
#include <ssdp/SSDPDiscover.h>

// Qt includes
#include <QEventLoop>

#include <QtNetwork>
#include <QTcpServer>
#include <QTcpSocket>

#include <chrono>
#include <thread>
#include <utility>

// Constants
namespace {

	const bool verbose = false;

	constexpr std::chrono::milliseconds WRITE_TIMEOUT{ 1000 };		 // device write timeout in ms
	constexpr std::chrono::milliseconds READ_TIMEOUT{ 1000 };			 // device read timeout in ms
	constexpr std::chrono::milliseconds CONNECT_TIMEOUT{ 1000 };		 // device connect timeout in ms
	constexpr std::chrono::milliseconds CONNECT_STREAM_TIMEOUT{ 1000 }; // device streaming connect timeout in ms

	const bool TEST_CORRELATION_IDS = false; //Ignore, if yeelight sends responses in different order as request commands

	// Configuration settings
	const char CONFIG_LIGHTS[] = "lights";

	const char CONFIG_COLOR_MODEL[] = "colorModel";
	const char CONFIG_TRANS_EFFECT[] = "transEffect";
	const char CONFIG_TRANS_TIME[] = "transTime";
	const char CONFIG_EXTRA_TIME_DARKNESS[] = "extraTimeDarkness";
	const char CONFIG_DEBUGLEVEL[] = "debugLevel";

	const char CONFIG_BRIGHTNESS_MIN[] = "brightnessMin";
	const char CONFIG_BRIGHTNESS_SWITCHOFF[] = "brightnessSwitchOffOnMinimum";
	const char CONFIG_BRIGHTNESS_MAX[] = "brightnessMax";
	const char CONFIG_BRIGHTNESSFACTOR[] = "brightnessFactor";

	const char CONFIG_RESTORE_STATE[] = "restoreOriginalState";

	const char CONFIG_QUOTA_WAIT_TIME[] = "quotaWait";

	// Yeelights API
	const int API_DEFAULT_PORT = 55443;
	const quint16 API_DEFAULT_QUOTA_WAIT_TIME = 1000;

	// Yeelight API Command
	const char API_COMMAND_ID[] = "id";
	const char API_COMMAND_METHOD[] = "method";
	const char API_COMMAND_PARAMS[] = "params";

	const char API_PARAM_CLASS_COLOR[] = "color";
	const char API_PARAM_CLASS_HSV[] = "hsv";

	const char API_PROP_NAME[] = "name";
	const char API_PROP_MODEL[] = "model";
	const char API_PROP_FWVER[] = "fw_ver";

	const char API_PROP_POWER[] = "power";
	const char API_PROP_MUSIC[] = "music_on";
	const char API_PROP_RGB[] = "rgb";
	const char API_PROP_CT[] = "ct";
	const char API_PROP_COLORFLOW[] = "cf";
	const char API_PROP_BRIGHT[] = "bright";

	// List of Result Information
	const char API_RESULT_ID[] = "id";
	const char API_RESULT[] = "result";
	//const char API_RESULT_OK[] = "OK";

	// List of Error Information
	const char API_ERROR[] = "error";
	const char API_ERROR_CODE[] = "code";
	const char API_ERROR_MESSAGE[] = "message";

	// Yeelight ssdp services
	const char SSDP_ID[] = "wifi_bulb";
	const char SSDP_FILTER[] = "yeelight(.*)";
	const char SSDP_FILTER_HEADER[] = "Location";
	const quint16 SSDP_PORT = 1982;

} //End of constants

YeelightLight::YeelightLight(const LoggerName& log, const QString& hostname, quint16 port = API_DEFAULT_PORT)
	:_log(log)
	, _debugLevel(0)
	, _isInError(false)
	, _host(hostname)
	, _port(port)
	, _tcpSocket(nullptr)
	, _tcpStreamSocket(nullptr)
	, _correlationID(0)
	, _lastWriteTime(InternalClock::now())
	, _lastColorRgbValue(0)
	, _transitionEffect(YeelightLight::API_EFFECT_SMOOTH)
	, _transitionDuration(API_PARAM_DURATION.count())
	, _extraTimeDarkness(API_PARAM_EXTRA_TIME_DARKNESS.count())
	, _brightnessMin(0)
	, _isBrightnessSwitchOffMinimum(false)
	, _brightnessMax(100)
	, _brightnessFactor(1.0)
	, _transitionEffectParam(API_PARAM_EFFECT_SMOOTH)
	, _waitTimeQuota(API_DEFAULT_QUOTA_WAIT_TIME)
	, _colorRgbValue(0)
	, _bright(0)
	, _ct(0)
	, _isOn(false)
	, _isInMusicMode(false)
{
	_name = hostname;

}

YeelightLight::~YeelightLight()
{
	delete _tcpSocket;
}

void YeelightLight::setHostname(const QString& hostname, quint16 port = API_DEFAULT_PORT)
{
	_host = hostname;
	_port = port;
}

void YeelightLight::setStreamSocket(QTcpSocket* socket)
{
	_tcpStreamSocket = socket;
}

bool YeelightLight::open()
{
	_isInError = false;
	bool rc = false;

	if (_tcpSocket == nullptr)
	{
		_tcpSocket = new QTcpSocket();
	}

	if (_tcpSocket->state() == QAbstractSocket::ConnectedState)
	{
		rc = true;
	}
	else
	{
		_tcpSocket->connectToHost(_host, _port);

		if (_tcpSocket->waitForConnected(CONNECT_TIMEOUT.count()))
		{
			if (_tcpSocket->state() != QAbstractSocket::ConnectedState)
			{
				this->setInError(_tcpSocket->errorString());
				rc = false;
			}
			else
			{
				rc = true;
			}
		}
		else
		{
			this->setInError(_tcpSocket->errorString());
			rc = false;
		}
	}
	return rc;
}

bool YeelightLight::close()
{
	bool rc = true;

	if (_tcpSocket != nullptr)
	{
		// Test, if device requires closing
		if (_tcpSocket->isOpen())
		{
			_tcpSocket->close();
			// Everything is OK -> device is closed
		}
	}

	if (_tcpStreamSocket != nullptr)
	{
		// Test, if stream socket requires closing
		if (_tcpStreamSocket->isOpen())
		{
			_tcpStreamSocket->close();
		}
	}
	return rc;
}

int YeelightLight::writeCommand(const QJsonDocument& command)
{
	QJsonArray result;
	return writeCommand(command, result);
}

int YeelightLight::writeCommand(const QJsonDocument& command, QJsonArray& result)
{
	int rc = -1;

	if (!_isInError && _tcpSocket->isOpen())
	{
		qint64 bytesWritten = _tcpSocket->write(command.toJson(QJsonDocument::Compact) + "\r\n");
		if (bytesWritten == -1)
		{
			this->setInError(QString("Write Error: %1").arg(_tcpSocket->errorString()));
		}
		else
		{
			if (!_tcpSocket->waitForBytesWritten(WRITE_TIMEOUT.count()))
			{
				QString errorReason = QString("(%1) %2").arg(_tcpSocket->error()).arg(_tcpSocket->errorString());
				this->setInError(errorReason);
			}
			else
			{
				// Avoid to overrun the Yeelight Command Quota
				qint64 elapsedTime = InternalClock::now() - _lastWriteTime;

				if (elapsedTime < _waitTimeQuota)
				{
					// Wait time (in ms) before doing next write to not overrun Yeelight command quota
					std::this_thread::sleep_for(std::chrono::milliseconds(_waitTimeQuota));
				}
			}

			if (_tcpSocket->waitForReadyRead(READ_TIMEOUT.count()))
			{
				do
				{
					while (_tcpSocket->canReadLine())
					{
						QByteArray response = _tcpSocket->readLine();

						YeelightResponse yeeResponse = handleResponse(_correlationID, response);
						switch (yeeResponse.error()) {

						case YeelightResponse::API_NOTIFICATION:
							rc = 0;
							break;
						case YeelightResponse::API_OK:
							result = yeeResponse.getResult();
							rc = 0;
							break;
						case YeelightResponse::API_ERROR:
							result = yeeResponse.getResult();
							QString errorReason = QString("(%1) %2").arg(yeeResponse.getErrorCode()).arg(yeeResponse.getErrorReason());
							if (yeeResponse.getErrorCode() != -1)
							{
								this->setInError(errorReason);
								rc = -1;
							}
							else
							{
								//(-1) client quota exceeded
								rc = -2;
							}
							break;
						}
					}
				} while (_tcpSocket->waitForReadyRead(500));
			}
		}

		//In case of no error or quota exceeded, update late write time avoiding immediate next write
		if (rc == 0 || rc == -2)
		{
			_lastWriteTime = InternalClock::now();
		}
	}

	return rc;
}

bool YeelightLight::streamCommand(const QJsonDocument& command)
{
	bool rc = false;

	if (!_isInError && _tcpStreamSocket->isOpen())
	{
		qint64 bytesWritten = _tcpStreamSocket->write(command.toJson(QJsonDocument::Compact) + "\r\n");
		if (bytesWritten == -1)
		{
			this->setInError(QString("Streaming Error %1").arg(_tcpStreamSocket->errorString()));
		}
		else
		{
			if (!_tcpStreamSocket->waitForBytesWritten(WRITE_TIMEOUT.count()))
			{
				int error = _tcpStreamSocket->error();
				QString errorReason = QString("(%1) %2").arg(error).arg(_tcpStreamSocket->errorString());

				if (error == QAbstractSocket::RemoteHostClosedError)
				{
					_isInMusicMode = false;
					rc = true;
				}
				else
				{
					this->setInError(errorReason);
				}
			}
			else
			{
				rc = true;
			}
		}
	}

	//log (2,"streamCommand() rc","%d, isON[%d], isInMusicMode[%d]", rc, _isOn, _isInMusicMode );
	return rc;
}

YeelightResponse YeelightLight::handleResponse(int correlationID, QByteArray const& response)
{
	//std::cout << _name.toStdString() <<"| Response: [" << response.toStdString() << "]" << std::endl << std::flush;

	YeelightResponse yeeResponse;
	QString errorReason;

	QJsonParseError error;
	QJsonDocument jsonDoc = QJsonDocument::fromJson(response, &error);

	if (error.error != QJsonParseError::NoError)
	{
		yeeResponse.setErrorCode(-10000);
		yeeResponse.setErrorReason("Got invalid response");
	}
	else
	{
		QJsonObject jsonObj = jsonDoc.object();

		if (!jsonObj[API_COMMAND_METHOD].isNull())
		{
			yeeResponse.setError(YeelightResponse::API_NOTIFICATION);
			yeeResponse.setResult(QJsonArray());
		}
		else
		{
			int id = jsonObj[API_RESULT_ID].toInt();
			//log ( 3, "Correlation ID:", "%d", id );

			if (id != correlationID && TEST_CORRELATION_IDS)
			{
				errorReason = QString("%1| API is out of sync, received ID [%2], expected [%3]").
					arg(_name).arg(id).arg(correlationID);

				yeeResponse.setErrorCode(-11000);
				yeeResponse.setErrorReason(errorReason);

				this->setInError(errorReason);
			}
			else
			{

				if (jsonObj.contains(API_RESULT) && jsonObj[API_RESULT].isArray())
				{
					// API call returned an result
					yeeResponse.setResult(jsonObj[API_RESULT].toArray());
				}
				else
				{
					yeeResponse.setError(YeelightResponse::API_ERROR);
					if (jsonObj.contains(API_ERROR) && jsonObj[API_ERROR].isObject())
					{
						QVariantMap errorMap = jsonObj[API_ERROR].toVariant().toMap();

						yeeResponse.setErrorCode(errorMap.value(API_ERROR_CODE).toInt());
						yeeResponse.setErrorReason(errorMap.value(API_ERROR_MESSAGE).toString());
					}
					else
					{
						yeeResponse.setErrorCode(-10010);
						yeeResponse.setErrorReason("No valid result message");
					}
				}
			}
		}
	}
	return yeeResponse;
}

void YeelightLight::setInError(const QString& errorMsg)
{
	_isInError = true;
	Error(_log, "Yeelight device '{:s}' signals error: '{:s}'", (_name), (errorMsg));
}

QJsonDocument YeelightLight::getCommand(const QString& method, const QJsonArray& params)
{
	//Increment Correlation-ID
	++_correlationID;

	QJsonObject obj;
	obj.insert(API_COMMAND_ID, _correlationID);
	obj.insert(API_COMMAND_METHOD, method);
	obj.insert(API_COMMAND_PARAMS, params);

	return QJsonDocument(obj);
}

QJsonObject YeelightLight::getProperties()
{
	QJsonObject properties;

	//Selected properties
	//QJsonArray propertyList = { API_PROP_NAME, API_PROP_MODEL, API_PROP_POWER, API_PROP_RGB, API_PROP_BRIGHT, API_PROP_CT, API_PROP_FWVER };

	//All properties
	QJsonArray propertyList = { "power","bright","ct","rgb","hue","sat","color_mode","flowing","delayoff","music_on","name","bg_power","bg_flowing","bg_ct","bg_bright","bg_hue","bg_sat","bg_rgb","nl_br","active_mode" };

	QJsonDocument command = getCommand(API_METHOD_GETPROP, propertyList);

	QJsonArray result;

	if (writeCommand(command, result) > -1)
	{

		// Debug output
		if (!result.empty())
		{
			int i = 0;
			for (const auto& item : std::as_const(result))
			{
				properties.insert(propertyList.at(i).toString(), item);
				++i;
			}
		}
	}

	return properties;
}

bool YeelightLight::identify()
{
	bool rc = true;

	/*
	count		6, total number of visible state changing before color flow is stopped
	action		0, 0 means smart LED recover to the state before the color flow started

	Duration:	500, Gradual change timer sleep-time, in milliseconds
	Mode:		1, color
	Value:		100, RGB value when mode is 1 (blue)
	Brightness:	100, Brightness value

	Duration:	500
	Mode:		1
	Value:		16711696 (red)
	Brightness:	10
	*/
	QJsonArray colorflowParams = { API_PROP_COLORFLOW, 6, 0, "500,1,100,100,500,1,16711696,10" };

	//Blink White
	//QJsonArray colorflowParams = { API_PROP_COLORFLOW, 6, 0, "500,2,4000,1,500,2,4000,50"};

	QJsonDocument command = getCommand(API_METHOD_SETSCENE, colorflowParams);

	if (writeCommand(command) < 0)
	{
		rc = false;
	}

	return rc;
}

bool YeelightLight::isInMusicMode(bool deviceCheck)
{
	bool inMusicMode = false;

	if (deviceCheck)
	{
		// Get status from device directly
		QJsonArray propertylist = { API_PROP_MUSIC };

		QJsonDocument command = getCommand(API_METHOD_GETPROP, propertylist);

		QJsonArray result;

		if (writeCommand(command, result) >= 0)
		{
			if (!result.empty())
			{
				inMusicMode = result.at(0).toString() == "1";
			}
		}
	}
	else
	{
		// Test indirectly avoiding command quota
		if (_tcpStreamSocket != nullptr)
		{
			if (_tcpStreamSocket->state() == QAbstractSocket::ConnectedState)
			{
				inMusicMode = true;
			}
		}
	}
	_isInMusicMode = inMusicMode;

	return _isInMusicMode;
}

void YeelightLight::mapProperties(const QJsonObject& properties)
{
	if (_name.isEmpty())
	{
		_name = properties.value(API_PROP_NAME).toString();
		if (_name.isEmpty())
		{
			_name = _host;
		}
	}
	_model = properties.value(API_PROP_MODEL).toString();
	_fw_ver = properties.value(API_PROP_FWVER).toString();

	_power = properties.value(API_PROP_POWER).toString();
	_colorRgbValue = properties.value(API_PROP_RGB).toString().toInt();
	_bright = properties.value(API_PROP_BRIGHT).toString().toInt();
	_ct = properties.value(API_PROP_CT).toString().toInt();
}

void YeelightLight::storeState()
{
	_originalStateProperties = this->getProperties();
	mapProperties(_originalStateProperties);
}

bool YeelightLight::restoreState()
{
	bool rc = false;

	QJsonArray paramlist = { API_PARAM_CLASS_COLOR, _colorRgbValue, _bright };

	if (_isInMusicMode)
	{
		rc = streamCommand(getCommand(API_METHOD_SETSCENE, paramlist));
	}
	else
	{
		if (writeCommand(getCommand(API_METHOD_SETSCENE, paramlist)) >= 0)
		{
			rc = true;
		}
	}

	return rc;
}

bool YeelightLight::setPower(bool on)
{
	return setPower(on, _transitionEffect, _transitionDuration);
}

bool YeelightLight::setPower(bool on, YeelightLight::API_EFFECT effect, int duration, API_MODE mode)
{
	bool rc = false;

	// Disable music mode to get power-off command executed
	setMusicMode(false);
	if (!on && _isInMusicMode)
	{
		if (_tcpStreamSocket != nullptr)
		{
			_tcpStreamSocket->close();
		}
	}

	QString powerParam = on ? API_METHOD_POWER_ON : API_METHOD_POWER_OFF;
	QString effectParam = effect == YeelightLight::API_EFFECT_SMOOTH ? API_PARAM_EFFECT_SMOOTH : API_PARAM_EFFECT_SUDDEN;

	QJsonArray paramlist = { powerParam, effectParam, duration, mode };

	// If power off was successful, automatically music-mode is off too
	if (writeCommand(getCommand(API_METHOD_POWER, paramlist)) > -1)
	{
		_isOn = on;
		if (!on)
		{
			_isInMusicMode = false;
		}
		rc = true;
	}

	return rc;
}

bool YeelightLight::setColorRGB(const ColorRgb& color)
{
	bool rc = true;

	int colorParam = (color.red * 65536) + (color.green * 256) + color.blue;

	if (colorParam == 0)
	{
		colorParam = 1;
	}

	if (colorParam != _lastColorRgbValue)
	{
		int bri = std::max({ color.red, color.green, color.blue }) * 100 / 255;
		int duration = _transitionDuration;

		if (bri < _brightnessMin)
		{
			if (_isBrightnessSwitchOffMinimum)
			{
				// Set brightness to 0
				bri = 0;
				duration = _transitionDuration + _extraTimeDarkness;
			}
			else
			{
				//If not switchOff on MinimumBrightness, avoid switch-off
				bri = _brightnessMin;
			}
		}
		else
		{
			bri = (qMin(_brightnessMax, static_cast<int> (_brightnessFactor * qMax(_brightnessMin, bri))));
		}

		QJsonArray paramlist = { API_PARAM_CLASS_COLOR, colorParam, bri };

		// Only add transition effect and duration, if device smoothing is configured (older FW do not support this parameters in set_scene
		if (_transitionEffect == YeelightLight::API_EFFECT_SMOOTH)
		{
			paramlist << _transitionEffectParam << duration;
		}

		bool writeOK = false;
		if (_isInMusicMode)
		{
			writeOK = streamCommand(getCommand(API_METHOD_SETSCENE, paramlist));
		}
		else
		{
			if (writeCommand(getCommand(API_METHOD_SETSCENE, paramlist)) >= 0)
			{
				writeOK = true;
			}
		}
		if (writeOK)
		{
			_lastColorRgbValue = colorParam;
		}
		else
		{
			rc = false;
		}
	}
	//log (2,"setColorRGB() rc","%d, isON[%d], isInMusicMode[%d]", rc, _isOn, _isInMusicMode );
	return rc;
}

bool YeelightLight::setColorHSV(const ColorRgb& colorRGB)
{
	bool rc = true;

	ColorRgb color(colorRGB.red, colorRGB.green, colorRGB.blue);

	if (color != _color)
	{
		int hue;
		int sat;
		int bri;
		int duration = _transitionDuration;

		color.getHsv(hue, sat, bri);

		//Align to Yeelight number ranges (hue: 0-359, sat: 0-100, bri: 0-100)
		if (hue == -1)
		{
			hue = 0;
		}
		sat = sat * 100 / 255;
		bri = bri * 100 / 255;

		if (bri < _brightnessMin)
		{
			if (_isBrightnessSwitchOffMinimum)
			{
				// Set brightness to 0
				bri = 0;
				duration = _transitionDuration + _extraTimeDarkness;
			}
			else
			{
				//If not switchOff on MinimumBrightness, avoid switch-off
				bri = _brightnessMin;
			}
		}
		else
		{
			bri = (qMin(_brightnessMax, static_cast<int> (_brightnessFactor * qMax(_brightnessMin, bri))));
		}
		QJsonArray paramlist = { API_PARAM_CLASS_HSV, hue, sat, bri };

		// Only add transition effect and duration, if device smoothing is configured (older FW do not support this parameters in set_scene
		if (_transitionEffect == YeelightLight::API_EFFECT_SMOOTH)
		{
			paramlist << _transitionEffectParam << duration;
		}

		bool writeOK = false;
		if (_isInMusicMode)
		{
			writeOK = streamCommand(getCommand(API_METHOD_SETSCENE, paramlist));
		}
		else
		{
			if (writeCommand(getCommand(API_METHOD_SETSCENE, paramlist)) >= 0)
			{
				writeOK = true;
			}
		}

		if (writeOK)
		{
			_isOn = true;
			if (bri == 0)
			{
				_isOn = false;
				_isInMusicMode = false;
			}
			_color = color;
		}
		else
		{
			rc = false;
		}
	}
	else
	{
		//log ( 3, "setColorHSV", "Skip update. Same Color as before");
	}
	return rc;
}


void YeelightLight::setTransitionEffect(YeelightLight::API_EFFECT effect, int duration)
{
	if (effect != _transitionEffect)
	{
		_transitionEffect = effect;
		_transitionEffectParam = effect == YeelightLight::API_EFFECT_SMOOTH ? API_PARAM_EFFECT_SMOOTH : API_PARAM_EFFECT_SUDDEN;
	}

	if (duration != _transitionDuration)
	{
		_transitionDuration = duration;
	}

}

void YeelightLight::setBrightnessConfig(int min, int max, bool switchoff, int extraTime, double factor)
{
	_brightnessMin = min;
	_isBrightnessSwitchOffMinimum = switchoff;
	_brightnessMax = max;
	_brightnessFactor = factor;
	_extraTimeDarkness = extraTime;
}

bool YeelightLight::setMusicMode(bool on, const QHostAddress& hostAddress, int port)
{
	bool rc = false;
	int musicModeParam = on ? API_METHOD_MUSIC_MODE_ON : API_METHOD_MUSIC_MODE_OFF;

	QJsonArray paramlist = { musicModeParam };

	if (on)
	{
		paramlist << hostAddress.toString() << port;

		// Music Mode is only on, if write did not fail nor quota was exceeded
		if ( writeCommand( getCommand( API_METHOD_MUSIC_MODE, paramlist ) ) > -1 )
		{
			_isInMusicMode = on;
			rc = true;
		}
	}
	else
	{
		auto wasInError = _isInError;
		writeCommand( getCommand( API_METHOD_MUSIC_MODE, paramlist ));
		if (_isInError && !wasInError)
		{
			Warning(_log, "Ignoring Yeelight error when turning off music mode");
			_isInError = false;
		}
		_isInMusicMode = false;
		rc = true;
	}

	return rc;
}


//---------------------------------------------------------------------------------

DriverNetYeelight::DriverNetYeelight(const QJsonObject& deviceConfig)
	: LedDevice(deviceConfig)
	, _lightsCount(0)
	, _outputColorModel(0)
	, _transitionEffect(YeelightLight::API_EFFECT_SMOOTH)
	, _transitionDuration(API_PARAM_DURATION.count())
	, _extraTimeDarkness(0)
	, _brightnessMin(0)
	, _isBrightnessSwitchOffMinimum(false)
	, _brightnessMax(100)
	, _brightnessFactor(1.0)
	, _waitTimeQuota(API_DEFAULT_QUOTA_WAIT_TIME)
	, _debuglevel(0)
	, _musicModeServerPort(-1)
{
}

DriverNetYeelight::~DriverNetYeelight()
{
	delete _tcpMusicModeServer;
}

bool DriverNetYeelight::init(QJsonObject deviceConfig)
{
	// Overwrite non supported/required features
	setRefreshTime(0);

	if (deviceConfig["refreshTime"].toInt(0) > 0)
	{
		Info(_log, "Yeelights do not require setting refresh time. Refresh time is ignored.");
	}

	DebugIf(verbose, _log, "deviceConfig: [{:s}]", QString(QJsonDocument(_devConfig).toJson(QJsonDocument::Compact)).toUtf8().constData());

	bool isInitOK = false;

	if (LedDevice::init(deviceConfig))
	{
		Debug(_log, "DeviceType        : {:s}", (this->getActiveDeviceType()));
		Debug(_log, "LedCount          : {:d}", this->getLedCount());
		Debug(_log, "RefreshTime       : {:d}", this->getRefreshTime());

		//Get device specific configuration

		bool ok;
		if (deviceConfig[CONFIG_COLOR_MODEL].isString())
		{
			_outputColorModel = deviceConfig[CONFIG_COLOR_MODEL].toString().toInt(&ok, MODEL_RGB);
		}
		else
		{
			_outputColorModel = deviceConfig[CONFIG_COLOR_MODEL].toInt(MODEL_RGB);
		}

		if (deviceConfig[CONFIG_TRANS_EFFECT].isString())
		{
			_transitionEffect = static_cast<YeelightLight::API_EFFECT>(deviceConfig[CONFIG_TRANS_EFFECT].toString().toInt(&ok, YeelightLight::API_EFFECT_SMOOTH));
		}
		else
		{
			_transitionEffect = static_cast<YeelightLight::API_EFFECT>(deviceConfig[CONFIG_TRANS_EFFECT].toInt(YeelightLight::API_EFFECT_SMOOTH));
		}

		_transitionDuration = deviceConfig[CONFIG_TRANS_TIME].toInt(API_PARAM_DURATION.count());
		_extraTimeDarkness = _devConfig[CONFIG_EXTRA_TIME_DARKNESS].toInt(0);

		_brightnessMin = _devConfig[CONFIG_BRIGHTNESS_MIN].toInt(0);
		_isBrightnessSwitchOffMinimum = _devConfig[CONFIG_BRIGHTNESS_SWITCHOFF].toBool(true);
		_brightnessMax = _devConfig[CONFIG_BRIGHTNESS_MAX].toInt(100);
		_brightnessFactor = _devConfig[CONFIG_BRIGHTNESSFACTOR].toDouble(1.0);

		if (deviceConfig[CONFIG_DEBUGLEVEL].isString())
		{
			_debuglevel = deviceConfig[CONFIG_DEBUGLEVEL].toString().toInt();
		}
		else
		{
			_debuglevel = deviceConfig[CONFIG_DEBUGLEVEL].toInt(0);
		}

		QString outputColorModel = _outputColorModel == MODEL_RGB ? "RGB" : "HSV";
		QString transitionEffect = _transitionEffect == YeelightLight::API_EFFECT_SMOOTH ? API_PARAM_EFFECT_SMOOTH : API_PARAM_EFFECT_SUDDEN;

		Debug(_log, "colorModel        : {:s}", (outputColorModel));
		Debug(_log, "Transitioneffect  : {:s}", (transitionEffect));
		Debug(_log, "Transitionduration: {:d}", _transitionDuration);
		Debug(_log, "Extra time darkn. : {:d}", _extraTimeDarkness);

		Debug(_log, "Brightn. Min      : {:d}", _brightnessMin);
		Debug(_log, "Brightn. Min Off  : {:d}", _isBrightnessSwitchOffMinimum);
		Debug(_log, "Brightn. Max      : {:d}", _brightnessMax);
		Debug(_log, "Brightn. Factor   : {:.2f}", _brightnessFactor);

		_isRestoreOrigState = _devConfig[CONFIG_RESTORE_STATE].toBool(false);
		Debug(_log, "RestoreOrigState  : {:d}", _isRestoreOrigState);

		_maxRetry = deviceConfig["maxRetry"].toInt(60);
		Debug(_log, "Max retry         : {:d}", _maxRetry);

		_waitTimeQuota = _devConfig[CONFIG_QUOTA_WAIT_TIME].toInt(0);
		Debug(_log, "Wait time (quota) : {:d}", _waitTimeQuota);

		Debug(_log, "Debuglevel        : {:d}", _debuglevel);

		QJsonArray configuredYeelightLights = _devConfig[CONFIG_LIGHTS].toArray();
		int configuredYeelightsCount = 0;
		for (const QJsonValueRef light : configuredYeelightLights)
		{
			QString host = light.toObject().value("host").toString();
			int port = light.toObject().value("port").toInt(API_DEFAULT_PORT);
			if (!host.isEmpty())
			{
				QString name = light.toObject().value("name").toString();
				Debug(_log, "Light [{:d}] - {:s} ({:s}:{:d})", configuredYeelightsCount, (name), (host), port);
				++configuredYeelightsCount;
			}
		}
		Debug(_log, "Light configured  : {:d}", configuredYeelightsCount);

		int configuredLedCount = this->getLedCount();
		if (configuredYeelightsCount < configuredLedCount)
		{
			QString errorReason = QString("Not enough Yeelights [%1] for configured LEDs [%2] found!")
				.arg(configuredYeelightsCount)
				.arg(configuredLedCount);
			this->setInError(errorReason);
			isInitOK = false;
		}
		else
		{

			if (configuredYeelightsCount > configuredLedCount)
			{
				Warning(_log, "More Yeelights defined [{:d}] than configured LEDs [{:d}].", configuredYeelightsCount, configuredLedCount);
			}

			_lightsAddressList.clear();
			for (int j = 0; j < configuredLedCount; ++j)
			{
				QString address = configuredYeelightLights[j].toObject().value("host").toString();
				int port = configuredYeelightLights[j].toObject().value("port").toInt(API_DEFAULT_PORT);

				QStringList addressparts = address.split(':', Qt::SkipEmptyParts);
				QString apiHost = addressparts[0];
				int apiPort = port;

				_lightsAddressList.append({ apiHost, apiPort });
			}

			if (updateLights(_lightsAddressList))
			{
				isInitOK = true;
			}
		}
	}
	return isInitOK;
}

bool DriverNetYeelight::startMusicModeServer()
{
	DebugIf(verbose, _log, "enabled [{:d}], _isDeviceReady [{:d}]", _isEnabled, _isDeviceReady);

	bool rc = false;
	if (_tcpMusicModeServer == nullptr)
	{
		_tcpMusicModeServer = new QTcpServer(this);
	}

	if (!_tcpMusicModeServer->isListening())
	{
		if (!_tcpMusicModeServer->listen())
		{
			QString errorReason = QString("(%1) %2").arg(_tcpMusicModeServer->serverError()).arg(_tcpMusicModeServer->errorString());
			Error(_log, "Error: MusicModeServer: {:s}", (errorReason));
			this->setInError(errorReason);

			Error(_log, "Failed to start music mode server");
		}
		else
		{
			for (const auto& _interface : QNetworkInterface::allInterfaces())
			{
				if ((_interface.type() != QNetworkInterface::InterfaceType::Ethernet &&
					_interface.type() != QNetworkInterface::InterfaceType::Wifi) ||
					_interface.humanReadableName().indexOf("virtual", 0, Qt::CaseInsensitive) >= 0 ||
					!_interface.flags().testFlag(QNetworkInterface::InterfaceFlag::IsRunning) ||
					!_interface.flags().testFlag(QNetworkInterface::InterfaceFlag::IsUp))
					continue;

				for (const auto& addressEntr : _interface.addressEntries())
				{
					auto address = addressEntr.ip();
					if (!address.isLoopback() && address.protocol() == QAbstractSocket::IPv4Protocol)
					{
						_musicModeServerAddress = address;
						break;
					}
				}
			}

			if (_musicModeServerAddress.isNull())
			{
				for (const auto& address : QNetworkInterface::allAddresses())
				{
					// is valid when, no loopback, IPv4
					if (!address.isLoopback() && address.protocol() == QAbstractSocket::IPv4Protocol)
					{
						_musicModeServerAddress = address;
						break;
					}
				}
			}

			if (_musicModeServerAddress.isNull())
			{
				Error(_log, "Failed to resolve IP for music mode server");
			}
		}
	}

	if (_tcpMusicModeServer->isListening())
	{
		_musicModeServerPort = _tcpMusicModeServer->serverPort();
		Debug(_log, "The music mode server is running at {:s}:{:d}", (_musicModeServerAddress.toString()), _musicModeServerPort);
		rc = true;
	}
	DebugIf(verbose, _log, "rc [{:d}], enabled [{:d}], _isDeviceReady [{:d}]", rc, _isEnabled, _isDeviceReady);
	return rc;
}

bool DriverNetYeelight::stopMusicModeServer()
{
	DebugIf(verbose, _log, "enabled [{:d}], _isDeviceReady [{:d}]", _isEnabled, _isDeviceReady);

	bool rc = false;
	if (_tcpMusicModeServer != nullptr)
	{
		Debug(_log, "Stop MusicModeServer");
		_tcpMusicModeServer->close();
		rc = true;
	}
	DebugIf(verbose, _log, "rc [{:d}], enabled [{:d}], _isDeviceReady [{:d}]", rc, _isEnabled, _isDeviceReady);
	return rc;
}

int DriverNetYeelight::open()
{
	DebugIf(verbose, _log, "enabled [{:d}], _isDeviceReady [{:d}]", _isEnabled, _isDeviceReady);
	int retval = -1;
	_isDeviceReady = false;

	// Open/Start LedDevice based on configuration
	if (!_lights.empty())
	{
		if (startMusicModeServer())
		{
			int lightsInError = 0;
			for (YeelightLight& light : _lights)
			{
				light.setTransitionEffect(_transitionEffect, _transitionDuration);
				light.setBrightnessConfig(_brightnessMin, _brightnessMax, _isBrightnessSwitchOffMinimum, _extraTimeDarkness, _brightnessFactor);
				light.setQuotaWaitTime(_waitTimeQuota);
				light.setDebuglevel(_debuglevel);

				if (!light.open())
				{
					Error(_log, "Failed to open [{:s}]", (light.getName()));
					++lightsInError;
				}
			}
			if (lightsInError < static_cast<int>(_lights.size()))
			{
				// Everything is OK -> enable device
				_isDeviceReady = true;
				retval = 0;
			}
			else
			{
				this->setInError("All Yeelights failed to be opened!");
			}
		}
	}
	else
	{
		// On error/exceptions, set LedDevice in error
	}

	DebugIf(verbose, _log, "retval [{:d}], enabled [{:d}], _isDeviceReady [{:d}]", retval, _isEnabled, _isDeviceReady);

	if (!_isDeviceReady)
	{
		setupRetry(3000);
	}

	return retval;
}

int DriverNetYeelight::close()
{
	DebugIf(verbose, _log, "enabled [{:d}], _isDeviceReady [{:d}]", _isEnabled, _isDeviceReady);
	int retval = 0;
	_isDeviceReady = false;

	// LedDevice specific closing activities

	//Close all Yeelight lights
	for (YeelightLight& light : _lights)
	{
		light.close();
	}

	//Close music mode server
	stopMusicModeServer();

	DebugIf(verbose, _log, "retval [{:d}], enabled [{:d}], _isDeviceReady [{:d}]", retval, _isEnabled, _isDeviceReady);
	return retval;
}

bool DriverNetYeelight::updateLights(const QVector<yeelightAddress>& list)
{
	bool rc = false;
	DebugIf(verbose, _log, "enabled [{:d}], _isDeviceReady [{:d}]", _isEnabled, _isDeviceReady);
	if (!_lightsAddressList.empty())
	{
		// search user light-id inside map and create light if found
		_lights.clear();

		_lights.reserve(static_cast<ulong>(_lightsAddressList.size()));

		for (auto& yeelightAddress : _lightsAddressList)
		{
			QString host = yeelightAddress.host;

			if (list.contains(yeelightAddress))
			{
				int port = yeelightAddress.port;

				Debug(_log, "Add Yeelight {:s}:{:d}", (host), port);
				_lights.emplace_back(_log, host, port);
			}
			else
			{
				Warning(_log, "Configured light-address {:s} is not available", (host));
			}
		}
		setLightsCount(static_cast<uint>(_lights.size()));
		rc = true;
	}
	return rc;
}

bool DriverNetYeelight::powerOn()
{
	if (_isDeviceReady)
	{
		//Power-on all Yeelights
		for (YeelightLight& light : _lights)
		{
			if (light.isReady() && !light.isInMusicMode())
			{
				light.setPower(true, YeelightLight::API_EFFECT_SMOOTH, 5000);
			}
		}
	}
	return true;
}

bool DriverNetYeelight::powerOff()
{
	if (_isDeviceReady)
	{
		writeBlack();

		//Power-off all Yeelights
		for (YeelightLight& light : _lights)
		{
			light.setPower(false, _transitionEffect, API_PARAM_DURATION_POWERONOFF.count());
		}
	}
	return true;
}

bool DriverNetYeelight::storeState()
{
	bool rc = true;

	for (YeelightLight& light : _lights)
	{
		light.storeState();
	}
	return rc;
}

bool DriverNetYeelight::restoreState()
{
	bool rc = true;

	for (YeelightLight& light : _lights)
	{
		light.restoreState();
		if (!light.wasOriginallyOn())
		{
			light.setPower(false, _transitionEffect, API_PARAM_DURATION_POWERONOFF.count());
		}
	}
	return rc;
}

QJsonObject DriverNetYeelight::discover(const QJsonObject& /*params*/)
{
	QJsonObject devicesDiscovered;
	devicesDiscovered.insert("ledDeviceType", _activeDeviceType);

	QJsonArray deviceList;

	// Discover Yeelight Devices
	SSDPDiscover discover;
	discover.setPort(SSDP_PORT);
	discover.skipDuplicateKeys(true);
	discover.setSearchFilter(SSDP_FILTER, SSDP_FILTER_HEADER);
	QString searchTarget = SSDP_ID;

	if (discover.discoverServices(searchTarget) > 0)
	{
		deviceList = discover.getServicesDiscoveredJson();
	}

	devicesDiscovered.insert("devices", deviceList);
	Debug(_log, "devicesDiscovered: [{:s}]", QString(QJsonDocument(devicesDiscovered).toJson(QJsonDocument::Compact)).toUtf8().constData());

	return devicesDiscovered;
}

QJsonObject DriverNetYeelight::getProperties(const QJsonObject& params)
{
	Debug(_log, "params: [{:s}]", QString(QJsonDocument(params).toJson(QJsonDocument::Compact)).toUtf8().constData());
	QJsonObject properties;

	QString apiHostname = params["hostname"].toString("");
	quint16 apiPort = static_cast<quint16>(params["port"].toInt(API_DEFAULT_PORT));
	Debug(_log, "apiHost [{:s}], apiPort [{:d}]", (apiHostname), apiPort);

	if (!apiHostname.isEmpty())
	{
		YeelightLight yeelight(_log, apiHostname, apiPort);

		//yeelight.setDebuglevel(3);
		if (yeelight.open())
		{
			properties.insert("properties", yeelight.getProperties());
			yeelight.close();
		}
	}
	Debug(_log, "properties: [{:s}]", QString(QJsonDocument(properties).toJson(QJsonDocument::Compact)).toUtf8().constData());

	return properties;
}

void DriverNetYeelight::identify(const QJsonObject& params)
{
	Debug(_log, "params: [{:s}]", QString(QJsonDocument(params).toJson(QJsonDocument::Compact)).toUtf8().constData());

	QString apiHostname = params["hostname"].toString("");
	quint16 apiPort = static_cast<quint16>(params["port"].toInt(API_DEFAULT_PORT));
	Debug(_log, "apiHost [{:s}], apiPort [{:d}]", (apiHostname), apiPort);

	if (!apiHostname.isEmpty())
	{
		YeelightLight yeelight(_log, apiHostname, apiPort);
		//yeelight.setDebuglevel(3);

		if (yeelight.open())
		{
			yeelight.identify();
			yeelight.close();
		}
	}
}

int DriverNetYeelight::writeFiniteColors(const std::vector<ColorRgb>& ledValues)
{
	//DebugIf(verbose, _log, "enabled [{:d}], _isDeviceReady [{:d}]", _isEnabled, _isDeviceReady);
	int rc = -1;

	//Update on all Yeelights by iterating through lights and set colors.
	unsigned int idx = 0;
	int lightsInError = 0;
	for (YeelightLight& light : _lights)
	{
		// Get color
		ColorRgb color = ledValues.at(idx);

		if (light.isReady())
		{
			bool skipWrite = false;
			if (!light.isInMusicMode())
			{
				if (light.setMusicMode(true, _musicModeServerAddress, _musicModeServerPort))
				{
					// Wait for callback of the device to establish streaming socket
					if (_tcpMusicModeServer->waitForNewConnection(CONNECT_STREAM_TIMEOUT.count()))
					{
						light.setStreamSocket(_tcpMusicModeServer->nextPendingConnection());
					}
					else
					{
						QString errorReason = QString("(%1) %2").arg(_tcpMusicModeServer->serverError()).arg(_tcpMusicModeServer->errorString());
						if (_tcpMusicModeServer->serverError() == QAbstractSocket::TemporaryError)
						{
							Info(_log, "Ignore write Error [{:s}]: _tcpMusicModeServer: {:s}", (light.getName()), (errorReason));
							skipWrite = true;
						}
						else
						{
							Warning(_log, "write Error [{:s}]: _tcpMusicModeServer: {:s}", (light.getName()), (errorReason));
							light.setInError("Failed to get stream socket");
						}
					}
				}
				else
				{
					DebugIf(verbose, _log, "setMusicMode failed due to command quota issue, skip write and try with next");
					skipWrite = true;
				}
			}

			if (!skipWrite)
			{
				// Update light with given color
				if (_outputColorModel == MODEL_RGB)
				{
					light.setColorRGB(color);
				}
				else
				{
					light.setColorHSV(color);
				}
			}
		}
		else
		{
			++lightsInError;
		}
		++idx;
	}

	if (!(lightsInError < static_cast<int>(_lights.size())))
	{
		this->setInError("All Yeelights in error - stopping device!");
	}
	else
	{
		// Minimum one Yeelight device is working, continue updating devices
		rc = 0;
	}

	//DebugIf(verbose, _log, "rc [{:d}]", rc );

	if (_isDeviceInError)
	{
		setupRetry(3000);
	}

	return rc;
}

LedDevice* DriverNetYeelight::construct(const QJsonObject& deviceConfig)
{
	return new DriverNetYeelight(deviceConfig);
}

bool DriverNetYeelight::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("yeelight", "leds_group_2_network", DriverNetYeelight::construct);
