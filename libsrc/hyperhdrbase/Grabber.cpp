#include <hyperhdrbase/Grabber.h>
#include <utils/ColorSys.h>
#include <QFile>

const QString Grabber::AUTO_SETTING = QString("auto");
const int	  Grabber::AUTO_INPUT = -1;
const int	  Grabber::AUTO_FPS   = 0;


Grabber::Grabber(const QString& grabberName, int width, int height, int cropLeft, int cropRight, int cropTop, int cropBottom)
	: _width(width)
	, _height(height)
	, _fps(Grabber::AUTO_FPS)
	, _input(Grabber::AUTO_INPUT)
	, _cropLeft(0)
	, _cropRight(0)
	, _cropTop(0)
	, _cropBottom(0)
	, _enabled(true)
	, _hdrToneMappingEnabled(0)	
	, _log(Logger::getInstance(grabberName.toUpper()))
	, _currentFrame(0)
	, _deviceName()
	, _enc(PixelFormat::NO_CHANGE)
	, _brightness(0)
	, _contrast(0)
	, _saturation(0)
	, _hue(0)
	, _qframe(false)
	, _blocked(false)
	, _restartNeeded(false)
	, _initialized(false)
	, _fpsSoftwareDecimation(1)
	, _actualVideoFormat(PixelFormat::NO_CHANGE)
	, _actualWidth(0)
	, _actualHeight(0)
	, _actualFPS(0)
	, _actualDeviceName("")
	, _lutBuffer(NULL)
	, _lutBufferInit(false)
	, _lineLength(-1)
	, _frameByteSize(-1)
	, _signalDetectionEnabled(false)
	, _signalAutoDetectionEnabled(false)
	, _synchro(1)
{
	Grabber::setCropping(cropLeft, cropRight, cropTop, cropBottom);
}

Grabber::~Grabber()
{
	if (_lutBuffer != NULL)
		free(_lutBuffer);
	_lutBuffer = NULL;
}

bool sortDevicePropertiesItem(const Grabber::DevicePropertiesItem& v1, const Grabber::DevicePropertiesItem& v2)
{
	if (v1.x != v2.x)
		return v1.x < v2.x;
	else if (v1.y != v2.y)
		return v1.y < v2.y;
	else
		return pixelFormatToString(v1.pf) < pixelFormatToString(v2.pf);
}

void Grabber::setEnabled(bool enable)
{
	Info(_log,"Capture interface is now %s", enable ? "enabled" : "disabled");
	_enabled = enable;
}

void Grabber::setCropping(unsigned cropLeft, unsigned cropRight, unsigned cropTop, unsigned cropBottom)
{
	if (_width>0 && _height>0)
	{
		if (cropLeft + cropRight >= (unsigned)_width || cropTop + cropBottom >= (unsigned)_height)
		{
			Error(_log, "Rejecting invalid crop values: left: %d, right: %d, top: %d, bottom: %d, higher than height/width %d/%d", cropLeft, cropRight, cropTop, cropBottom, _height, _width);
			return;
		}
	}

	_cropLeft   = cropLeft;
	_cropRight  = cropRight;
	_cropTop    = cropTop;
	_cropBottom = cropBottom;

	if (cropLeft >= 0 || cropRight >= 0 || cropTop >= 0 || cropBottom >= 0)
	{
		Info(_log, "Cropping image: width=%d height=%d; crop: left=%d right=%d top=%d bottom=%d ", _width, _height, cropLeft, cropRight, cropTop, cropBottom);
	}
}

bool Grabber::trySetInput(int input)
{
	if((input >= 0) && (_input != input))
	{
		_input = input;
		return true;
	}

	return false;
}

bool Grabber::trySetWidthHeight(int width, int height)
{
	// eval changes with crop
	if ( (width>=0 && height>=0) && (_width != width || _height != height) )
	{
		if ((width != 0 && height != 0) && (_cropLeft + _cropRight >= width || _cropTop + _cropBottom >= height))
		{
			Error(_log, "Rejecting invalid width/height values as it collides with image cropping: width: %d, height: %d", width, height);
			return false;
		}

		Debug(_log, "Set new width: %d, height: %d for capture", width, height);
		_width = width;
		_height = height;

		return true;
	}

	return false;
}

int Grabber::getImageWidth()
{
	return _width;
}

int Grabber::getImageHeight()
{
	return _height;
}

void Grabber::setFpsSoftwareDecimation(int decimation)
{
	_fpsSoftwareDecimation = decimation;
	Debug(_log, "setFpsSoftwareDecimation to: %i", decimation);
}

int Grabber::getFpsSoftwareDecimation()
{
	return _fpsSoftwareDecimation;
}

int Grabber::getActualFps()
{
	return _actualFPS;
}

void Grabber::setEncoding(QString enc)
{
	PixelFormat _oldEnc = _enc;

	_enc = parsePixelFormat(enc);
	Debug(_log, "Force encoding to: %s (old: %s)", QSTRING_CSTR(pixelFormatToString(_enc)), QSTRING_CSTR(pixelFormatToString(_oldEnc)));

	if (_oldEnc != _enc)
	{
		if (_initialized && !_blocked)
		{
			Debug(_log, "Restarting video grabber");
			uninit();
			start();
		}
		else
		{
			Info(_log, "Delayed restart of the grabber due to change of the video encoding format");
			_restartNeeded = true;
		}

	}
}

void Grabber::setBrightnessContrastSaturationHue(int brightness, int contrast, int saturation, int hue)
{
	if (_brightness != brightness || _contrast != contrast || _saturation != saturation || _hue != hue)
	{
		if (_initialized && _deviceProperties.contains(_actualDeviceName))
		{
			auto dev = _deviceProperties[_actualDeviceName];

			if (_brightness != brightness && brightness == 0 && dev.brightness.enabled)
			{
				brightness = (int)dev.brightness.defVal;
				Debug(_log, "Reset brightness to default: %i (user value: 0)", brightness);
			}

			if (_contrast != contrast && contrast == 0 && dev.contrast.enabled)
			{
				contrast = (int)dev.contrast.defVal;
				Debug(_log, "Reset contrast to default: %i (user value: 0)", contrast);
			}

			if (_saturation != saturation && saturation == 0 && dev.saturation.enabled)
			{
				saturation = (int)dev.saturation.defVal;
				Debug(_log, "Reset saturation to default: %i (user value: 0)", saturation);
			}

			if (_hue != hue && hue == 0 && dev.hue.enabled)
			{
				hue = (int)dev.hue.defVal;
				Debug(_log, "Reset hue to default: %i (user value: 0)", hue);
			}
		}		

		_brightness = brightness;
		_contrast = contrast;
		_saturation = saturation;
		_hue = hue;

		Debug(_log, "Set brightness to %i, contrast to %i, saturation to %i, hue to %i", _brightness, _contrast, _saturation, _hue);

		if (_initialized && !_blocked)
		{
			Debug(_log, "Restarting video grabber");
			uninit();
			start();
		}
		else
		{
			Info(_log, "Delayed restart of the grabber due to change of the Brightness/Contrast/Saturation/Hue");
			_restartNeeded = true;
		}

	}
	else
		Debug(_log, "setBrightnessContrastSaturationHue nothing changed");
}

void Grabber::setQFrameDecimation(int setQframe)
{
	_qframe = setQframe;
	Info(_log, QSTRING_CSTR(QString("setQFrameDecimation is now: %1").arg(_qframe ? "enabled" : "disabled")));
}

void Grabber::unblockAndRestart(bool running)
{
	if (_restartNeeded && running)
	{
		Debug(_log, "Planned restart of video grabber after reloading of the configuration");
		
		uninit();
		start();
	}

	_blocked = false;
	_restartNeeded = false;
}

void Grabber::setBlocked()
{
	Info(_log, "Restarting video grabber is now blocked due to reloading of the configuration");

	_blocked = true;
	_restartNeeded = false;
}

bool Grabber::setFramerate(int fps)
{
	int _oldFps = _fps;

	_fps = fps;

	if (_oldFps != _fps)
	{
		if (_initialized && !_blocked)
		{
			Debug(_log, "Restarting video grabber");
			uninit();
			start();
		}
		else
		{
			Info(_log, "Delayed restart of the grabber due to change of the framerate");
			_restartNeeded = true;
		}

		return true;
	}
	return false;
}

bool Grabber::setWidthHeight(int width, int height)
{
	if (Grabber::trySetWidthHeight(width, height))
	{
		Debug(_log, "setWidthHeight preparing to restarting video grabber %i", _initialized);

		if (_initialized && !_blocked)
		{
			Debug(_log, "Restarting video grabber");
			uninit();
			start();
		}
		else
		{
			Info(_log, "Delayed restart of the grabber due to change of the capturing resolution");
			_restartNeeded = true;
		}

		return true;
	}
	return false;
}

bool Grabber::setInput(int input)
{
	if (Grabber::trySetInput(input))
	{
		if (_initialized && !_blocked)
		{
			Debug(_log, "Restarting video grabber");
			uninit();
			start();
		}
		else
		{
			Info(_log, "Delayed restart of the grabber due to change of selected input");
			_restartNeeded = true;
		}

		return true;
	}
	return false;
}

void Grabber::setDeviceVideoStandard(QString device)
{
	QString olddeviceName = _deviceName;
	if (_deviceName != device)
	{
		Debug(_log, "setDeviceVideoStandard preparing to restart video grabber. Old: '%s' new: '%s'", QSTRING_CSTR(_deviceName), QSTRING_CSTR(device));

		_deviceName = device;

		if (!olddeviceName.isEmpty() && !_blocked)
		{
			Debug(_log, "Restarting video grabber for new device");
			uninit();

			start();
		}
		else
		{
			Info(_log, "Delayed restart of the grabber due to change of selected device");
			_restartNeeded = true;
		}
	}
}

void Grabber::resetCounter(int64_t from)
{
	frameStat.frameBegin = from;
	frameStat.averageFrame = 0;
	frameStat.badFrame = 0;
	frameStat.goodFrame = 0;
	frameStat.segment = 0;
}

int Grabber::getHdrToneMappingEnabled()
{
	return _hdrToneMappingEnabled;
}

void Grabber::loadLutFile(PixelFormat color, const QList<QString>& files)
{
	bool is_yuv = (color == PixelFormat::YUYV);

	_lutBufferInit = false;

	if (color != PixelFormat::NO_CHANGE && color != PixelFormat::RGB24 && color != PixelFormat::YUYV)
	{
		Error(_log, "Unsupported mode for loading LUT table: %s", QSTRING_CSTR(pixelFormatToString(color)));
		return;
	}	

	if (color == PixelFormat::NO_CHANGE)
	{
		if (_lutBuffer == NULL)
			_lutBuffer = (uint8_t*)malloc(LUT_FILE_SIZE + 4);

		if (_lutBuffer != NULL)
		{
			for (int y = 0; y < 256; y++)
				for (int u = 0; u < 256; u++)
					for (int v = 0; v < 256; v++)
					{
						uint32_t ind_lutd = LUT_INDEX(y, u, v);
						ColorSys::yuv2rgb(y, u, v,
							_lutBuffer[ind_lutd],
							_lutBuffer[ind_lutd + 1],
							_lutBuffer[ind_lutd + 2]);
					}
			_lutBufferInit = true;
		}

		Error(_log, "You have forgotten to put lut_lin_tables.3d file in the HyperHDR configuration folder. Internal LUT table for YUV conversion has been created instead.");
		return;
	}

	if (_hdrToneMappingEnabled || is_yuv)
	{
		for(QString fileName3d : files)
		{
			QFile file(fileName3d);
			
			if (file.open(QIODevice::ReadOnly))
			{
				size_t length;
				Debug(_log, "LUT file found: %s", QSTRING_CSTR(fileName3d));
				
				length = file.size();

				if (length == LUT_FILE_SIZE * 3)
				{
					qint64 index = 0;

					if (is_yuv && _hdrToneMappingEnabled)
					{
						Debug(_log, "Index 1 for HDR YUV");
						index = LUT_FILE_SIZE;
					}
					else if (is_yuv)
					{
						Debug(_log, "Index 2 for YUV");
						index = LUT_FILE_SIZE * 2;
					}
					else
						Debug(_log, "Index 0 for HDR RGB");

					file.seek(index);

					if (_lutBuffer == NULL)
						_lutBuffer = (unsigned char*)malloc(length + 4);

					if (file.read((char*)_lutBuffer, LUT_FILE_SIZE) != LUT_FILE_SIZE)
					{
						Error(_log, "Error reading LUT file %s", QSTRING_CSTR(fileName3d));
					}
					else
					{
						_lutBufferInit = true;
						Info(_log, "Found and loaded LUT: '%s'", QSTRING_CSTR(fileName3d));
					}
				}
				else
					Error(_log, "LUT file has invalid length: %i %s. Please generate new one LUT table using the generator page.", length, QSTRING_CSTR(fileName3d));
				
				file.close();

				return;
			}
			else
				Warning(_log, "LUT file is not found here: %s", QSTRING_CSTR(fileName3d));
		}

		Error(_log, "Could not find any required LUT file");
	}
}

QMap<Grabber::VideoControls, int> Grabber::getVideoDeviceControls(const QString& devicePath)
{
	QMap<Grabber::VideoControls, int> retVal;

	if (!_deviceProperties.contains(devicePath))
		return retVal;

	auto dev = _deviceProperties[devicePath];

	if (dev.brightness.enabled)
		retVal[Grabber::VideoControls::BrightnessDef] = (int)dev.brightness.defVal;

	if (dev.contrast.enabled)
		retVal[Grabber::VideoControls::ContrastDef] = (int)dev.contrast.defVal;

	if (dev.saturation.enabled)
		retVal[Grabber::VideoControls::SaturationDef] = (int)dev.saturation.defVal;

	if (dev.hue.enabled)
		retVal[Grabber::VideoControls::HueDef] = (int)dev.hue.defVal;

	return retVal;
}

QList<Grabber::DevicePropertiesItem> Grabber::getVideoDeviceModesFullInfo(const QString& devicePath)
{
	QList<Grabber::DevicePropertiesItem> retVal;

	if (!_deviceProperties.contains(devicePath))
		return retVal;

	retVal = _deviceProperties[devicePath].valid;
	
	std::sort(retVal.begin(), retVal.end(), sortDevicePropertiesItem);


	return retVal;
}

QMultiMap<QString, int> Grabber::getVideoDeviceInputs(const QString& devicePath) const
{
	if (!_deviceProperties.contains(devicePath))
		return QMultiMap<QString, int>();

	return  _deviceProperties[devicePath].inputs;
}

QStringList Grabber::getVideoDevices() const
{
	QStringList result = QStringList();
	for (auto it = _deviceProperties.begin(); it != _deviceProperties.end(); ++it)
	{
		result << it.key();
	}
	return result;
}

QString Grabber::getVideoDeviceName(const QString& devicePath) const
{
	if (_deviceProperties.contains(devicePath))
		return _deviceProperties[devicePath].name;

	return QString(Grabber::AUTO_SETTING);
}

QMap<Grabber::currentVideoModeInfo, QString> Grabber::getVideoCurrentMode() const
{
	QMap<Grabber::currentVideoModeInfo, QString> retVal;

	if (_initialized)
	{
		QString  videoInfo = QString("%1x%2x%3 %4").arg(_actualWidth)
			.arg(_actualHeight)
			.arg(_actualFPS)
			.arg(pixelFormatToString(_actualVideoFormat).toUpper());
		retVal[Grabber::currentVideoModeInfo::resolution] = videoInfo;
		retVal[Grabber::currentVideoModeInfo::device] = _actualDeviceName;
	}

	return retVal;
}


void Grabber::processSystemFrameBGRA(uint8_t* source, int lineSize)
{	
	int startX = _cropLeft;
	int startY = _cropTop;
	int realSizeX = _actualWidth - startX - _cropRight;
	int realSizeY = _actualHeight - startY - _cropBottom;

	if (realSizeX <= 16 || realSizeY <= 16)
	{
		realSizeX = _actualWidth;
		realSizeY = _actualHeight;
	}

	int checkWidth = realSizeX;
	int division = 1;

	while (checkWidth > _width)
	{		
		division++;
		checkWidth = realSizeX / division;
	}

	int targetSizeX = realSizeX / division;
	int targetSizeY = realSizeY / division;

	Image<ColorRgb> image(targetSizeX, targetSizeY);

	ImageResampler::processSystemImageBGRA(image, targetSizeX, targetSizeY, startX, startY, source, _actualWidth, _actualHeight, division, (_hdrToneMappingEnabled == 0 || !_lutBufferInit) ? nullptr : _lutBuffer, lineSize);
	
	if (_signalDetectionEnabled)
	{
		if (checkSignalDetectionManual(image))
			emit newFrame(image);
	}
	else
		emit newFrame(image);
}

void Grabber::setSignalDetectionEnable(bool enable)
{
	if (_signalDetectionEnabled != enable)
	{
		_signalDetectionEnabled = enable;
		Info(_log, "Signal detection is now %s", enable ? "enabled" : "disabled");
	}
}

void Grabber::setAutoSignalDetectionEnable(bool enable)
{
	if (_signalAutoDetectionEnabled != enable)
	{
		_signalAutoDetectionEnabled = enable;
		Info(_log, "Automatic signal detection is now %s", enable ? "enabled" : "disabled");
	}
}

void Grabber::revive()
{
	bool checkSignal = false;

	if (_signalAutoDetectionEnabled)
	{
		checkSignal = getDetectionAutoSignal();
	}
	else if (_signalDetectionEnabled)
	{
		checkSignal = getDetectionManualSignal();
	}

	if (checkSignal)
	{
		Warning(_log, "The video control requested for the grabber restart due to signal lost, but it's caused by signal detection. No restart.");
		return;
	}

	if (_synchro.tryAcquire())
	{
		Warning(_log, "The video control requested for the grabber restart due to signal lost");
		stop();
		start();
		_synchro.release();
	}
	else
		Warning(_log, "The video control requested for the grabber restart due to signal lost, but other instance already handled it");
}
