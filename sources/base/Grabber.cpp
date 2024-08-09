/* Grabber.cpp
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
	#include <QJsonArray>
#endif

#include <base/Grabber.h>
#include <utils/GlobalSignals.h>

const QString Grabber::AUTO_SETTING = QString("auto");
const int	  Grabber::AUTO_INPUT = -1;
const int	  Grabber::AUTO_FPS = 0;


Grabber::Grabber(const QString& configurationPath, const QString& grabberName)
	: _configurationPath(configurationPath)
	, _width(0)
	, _height(0)
	, _fps(Grabber::AUTO_FPS)
	, _input(Grabber::AUTO_INPUT)
	, _cropLeft(0)
	, _cropRight(0)
	, _cropTop(0)
	, _cropBottom(0)
	, _enabled(true)
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
	, _hardware(false)
	, _actualVideoFormat(PixelFormat::NO_CHANGE)
	, _actualWidth(0)
	, _actualHeight(0)
	, _actualFPS(0)
	, _actualDeviceName("")
	, _targetMonitorNits(200)
	, _lineLength(-1)
	, _frameByteSize(-1)
	, _signalDetectionEnabled(false)
	, _signalAutoDetectionEnabled(false)
	, _synchro(1)
	, _benchmarkStatus(-1)
	, _benchmarkMessage("")
{
}

Grabber::~Grabber()
{
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
	Info(_log, "Capture interface is now %s", enable ? "enabled" : "disabled");
	_enabled = enable;
}

void Grabber::setMonitorNits(int nits)
{
	_targetMonitorNits = nits;
}

void Grabber::setCropping(unsigned cropLeft, unsigned cropRight, unsigned cropTop, unsigned cropBottom)
{
	if (_width > 0 && _height > 0)
	{
		if (cropLeft + cropRight >= (unsigned)_width || cropTop + cropBottom >= (unsigned)_height)
		{
			Error(_log, "Rejecting invalid crop values: left: %d, right: %d, top: %d, bottom: %d, higher than height/width %d/%d", cropLeft, cropRight, cropTop, cropBottom, _height, _width);
			return;
		}
	}

	_cropLeft = cropLeft;
	_cropRight = cropRight;
	_cropTop = cropTop;
	_cropBottom = cropBottom;

	if (cropLeft >= 0 || cropRight >= 0 || cropTop >= 0 || cropBottom >= 0)
	{
		Info(_log, "Cropping image: width=%d height=%d; crop: left=%d right=%d top=%d bottom=%d ", _width, _height, cropLeft, cropRight, cropTop, cropBottom);
	}
}

void Grabber::enableHardwareAcceleration(bool hardware)
{
	_hardware = hardware;
}

bool Grabber::trySetInput(int input)
{
	if ((input >= 0) && (_input != input))
	{
		_input = input;
		return true;
	}

	return false;
}

bool Grabber::trySetWidthHeight(int width, int height)
{
	// eval changes with crop
	if ((width >= 0 && height >= 0) && (_width != width || _height != height))
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

		emit GlobalSignals::getInstance()->SignalPerformanceStateChanged(true, hyperhdr::PerformanceReportType::VIDEO_GRABBER, -1);
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
	frameStat.directAccess = false;
}

int Grabber::getHdrToneMappingEnabled()
{
	return _hdrToneMappingEnabled;
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

int Grabber::getTargetSystemFrameDimension(int& targetSizeX, int& targetSizeY)
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
	int divide = 1;

	while (checkWidth > _width)
	{
		divide++;
		checkWidth = realSizeX / divide;
	}

	targetSizeX = realSizeX / divide;
	targetSizeY = realSizeY / divide;

	return divide;
}

void Grabber::processSystemFrameBGRA(uint8_t* source, int lineSize, bool useLut)
{
	int targetSizeX, targetSizeY;
	int divide = getTargetSystemFrameDimension(targetSizeX, targetSizeY);
	Image<ColorRgb> image(targetSizeX, targetSizeY);

	FrameDecoder::processSystemImageBGRA(image, targetSizeX, targetSizeY, _cropLeft, _cropTop, source, _actualWidth, _actualHeight, divide, (_hdrToneMappingEnabled == 0 || !_lutBufferInit || !useLut) ? nullptr : _lut.data(), lineSize);

	if (_signalDetectionEnabled)
	{
		if (checkSignalDetectionManual(image))
			emit SignalNewCapturedFrame(image);
	}
	else
		emit SignalNewCapturedFrame(image);
}

void Grabber::processSystemFrameBGR(uint8_t* source, int lineSize)
{
	int targetSizeX, targetSizeY;
	int divide = getTargetSystemFrameDimension(targetSizeX, targetSizeY);
	Image<ColorRgb> image(targetSizeX, targetSizeY);

	FrameDecoder::processSystemImageBGR(image, targetSizeX, targetSizeY, _cropLeft, _cropTop, source, _actualWidth, _actualHeight, divide, (_hdrToneMappingEnabled == 0 || !_lutBufferInit) ? nullptr : _lut.data(), lineSize);

	if (_signalDetectionEnabled)
	{
		if (checkSignalDetectionManual(image))
			emit SignalNewCapturedFrame(image);
	}
	else
		emit SignalNewCapturedFrame(image);
}

void Grabber::processSystemFrameBGR16(uint8_t* source, int lineSize)
{
	int targetSizeX, targetSizeY;
	int divide = getTargetSystemFrameDimension(targetSizeX, targetSizeY);
	Image<ColorRgb> image(targetSizeX, targetSizeY);

	FrameDecoder::processSystemImageBGR16(image, targetSizeX, targetSizeY, _cropLeft, _cropTop, source, _actualWidth, _actualHeight, divide, (_hdrToneMappingEnabled == 0 || !_lutBufferInit) ? nullptr : _lut.data(), lineSize);

	if (_signalDetectionEnabled)
	{
		if (checkSignalDetectionManual(image))
			emit SignalNewCapturedFrame(image);
	}
	else
		emit SignalNewCapturedFrame(image);
}

void Grabber::processSystemFrameRGBA(uint8_t* source, int lineSize)
{
	int targetSizeX, targetSizeY;
	int divide = getTargetSystemFrameDimension(targetSizeX, targetSizeY);
	Image<ColorRgb> image(targetSizeX, targetSizeY);

	FrameDecoder::processSystemImageRGBA(image, targetSizeX, targetSizeY, _cropLeft, _cropTop, source, _actualWidth, _actualHeight, divide, (_hdrToneMappingEnabled == 0 || !_lutBufferInit) ? nullptr : _lut.data(), lineSize);

	if (_signalDetectionEnabled)
	{
		if (checkSignalDetectionManual(image))
			emit SignalNewCapturedFrame(image);
	}
	else
		emit SignalNewCapturedFrame(image);
}

void Grabber::processSystemFramePQ10(uint8_t* source, int lineSize)
{
	int targetSizeX, targetSizeY;
	int divide = getTargetSystemFrameDimension(targetSizeX, targetSizeY);
	Image<ColorRgb> image(targetSizeX, targetSizeY);

	FrameDecoder::processSystemImagePQ10(image, targetSizeX, targetSizeY, _cropLeft, _cropTop, source, _actualWidth, _actualHeight, divide, (_hdrToneMappingEnabled == 0 || !_lutBufferInit) ? nullptr : _lut.data(), lineSize);

	if (_signalDetectionEnabled)
	{
		if (checkSignalDetectionManual(image))
			emit SignalNewCapturedFrame(image);
	}
	else
		emit SignalNewCapturedFrame(image);
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

QJsonObject Grabber::getJsonInfo()
{
	QJsonObject grabbers;
	QJsonArray availableVideoDevices;

	for (const auto& devicePath : getVideoDevices())
	{
		QJsonObject device;
		device["device"] = devicePath;
		device["name"] = getVideoDeviceName(devicePath);

		QJsonArray availableInputs;
		QMultiMap<QString, int> inputs = getVideoDeviceInputs(devicePath);
		for (auto input = inputs.begin(); input != inputs.end(); ++input)
		{
			QJsonObject availableInput;
			availableInput["inputName"] = input.key();
			availableInput["inputIndex"] = input.value();
			availableInputs.append(availableInput);
		}
		device.insert("inputs", availableInputs);

		QJsonObject availableVideoControls;
		QMap<Grabber::VideoControls, int> videoControls = getVideoDeviceControls(devicePath);

		if (videoControls.contains(Grabber::VideoControls::BrightnessDef))
			availableVideoControls["BrightnessDef"] = QString("Brightness: <span class='text-info'>%1</span>").arg(QString::number(videoControls[Grabber::VideoControls::BrightnessDef]));
		else
			availableVideoControls["BrightnessDef"] = QString("Brightness: <span class='text-danger'>NO</span>");

		if (videoControls.contains(Grabber::VideoControls::ContrastDef))
			availableVideoControls["ContrastDef"] = QString("Contrast: <span class='text-info'>%1</span>").arg(QString::number(videoControls[Grabber::VideoControls::ContrastDef]));
		else
			availableVideoControls["ContrastDef"] = QString("Contrast: <span class='text-danger'>NO</span>");

		if (videoControls.contains(Grabber::VideoControls::SaturationDef))
			availableVideoControls["SaturationDef"] = QString("Saturation: <span class='text-info'>%1</span>").arg(QString::number(videoControls[Grabber::VideoControls::SaturationDef]));
		else
			availableVideoControls["SaturationDef"] = QString("Saturation: <span class='text-danger'>NO</span>");

		if (videoControls.contains(Grabber::VideoControls::HueDef))
			availableVideoControls["HueDef"] = QString("Hue: <span class='text-info'>%1</span>").arg(QString::number(videoControls[Grabber::VideoControls::HueDef]));
		else
			availableVideoControls["HueDef"] = QString("Hue: <span class='text-danger'>NO</span>");

		device.insert("videoControls", availableVideoControls);

		QList<Grabber::DevicePropertiesItem> videoModeList = getVideoDeviceModesFullInfo(devicePath);
		QJsonArray availableModeList;
		QStringList resolutions;
		QStringList videoCodecs;
		std::list<int> availableFrameratesList;

		for (const auto& videoMode : videoModeList)
		{
			QJsonObject videoModeJson;
			QString     resInfo = QString("%1x%2").arg(videoMode.x).arg(videoMode.y);
			QString     codecName = pixelFormatToString(videoMode.pf);
			if (!(std::find(std::begin(availableFrameratesList), std::end(availableFrameratesList), videoMode.fps) != std::end(availableFrameratesList)))
			{
				availableFrameratesList.push_back(videoMode.fps);
			}

			if (!resolutions.contains(resInfo))
			{
				resolutions.append(resInfo);
			}

			if (!videoCodecs.contains(codecName))
			{
				videoCodecs.append(codecName);
			}

			videoModeJson["width"] = videoMode.x;
			videoModeJson["height"] = videoMode.y;
			videoModeJson["fps"] = videoMode.fps;
			videoModeJson["pixel_format_id"] = (int)videoMode.pf;
			videoModeJson["pixel_format_info"] = codecName;

			availableModeList.append(videoModeJson);
		}
		device.insert("videoModeList", availableModeList);

		QJsonArray availableFramerates;
		availableFrameratesList.sort();
		for (int frameRate : availableFrameratesList)
		{
			availableFramerates.append(QString::number(frameRate));
		}
		device.insert("framerates", availableFramerates);


		QJsonArray availableResolutions;
		for (auto x : resolutions)
		{
			availableResolutions.append(x);
		}
		device.insert("resolutions", availableResolutions);

		QJsonArray availableVideoCodec;
		videoCodecs.sort();
		for (auto x : videoCodecs)
		{
			availableVideoCodec.append(x);
		}
		device.insert("videoCodecs", availableVideoCodec);

		availableVideoDevices.append(device);
	}

	grabbers["video_devices"] = availableVideoDevices;

	// current state
	QJsonObject current;
	auto currentInfo = getVideoCurrentMode();

	if (currentInfo.contains(Grabber::currentVideoModeInfo::device))
		current["device"] = currentInfo[Grabber::currentVideoModeInfo::device];
	else
		current["device"] = "";

	if (currentInfo.contains(Grabber::currentVideoModeInfo::resolution))
		current["videoMode"] = currentInfo[Grabber::currentVideoModeInfo::resolution];
	else
		current["videoMode"] = "";

	grabbers["current"] = current;

	if (_lut.data() != nullptr)
	{
		uint32_t checkSum = 0;
		for (int i = 0; i < 256; i += 2)
			for (int j = 32; j <= 160; j += 64)
			{
				checkSum ^= *(reinterpret_cast<uint32_t*>(&(_lut.data()[LUT_INDEX(j, i, (255 - i))])));
			}
		grabbers["lutFastCRC"] = "0x" + QString("%1").arg(checkSum, 4, 16).toUpper();
	}

	return grabbers;
}

QString Grabber::getConfigurationPath()
{
	return _configurationPath;
}

QString Grabber::getSignature()
{
	auto info = getVideoCurrentMode();
	return QString("%1 %2").
		arg(info[Grabber::currentVideoModeInfo::resolution]).
		arg(info[Grabber::currentVideoModeInfo::device]);
}

void Grabber::handleNewFrame(unsigned int workerIndex, Image<ColorRgb> image, quint64 sourceCount, qint64 _frameBegin)
{
	frameStat.goodFrame++;
	frameStat.averageFrame += InternalClock::nowPrecise() - _frameBegin;

	if (image.width() > 1 && image.height() > 1)
	{
		if (_signalAutoDetectionEnabled || isCalibrating())
		{
			if (checkSignalDetectionAutomatic(image))
				emit GlobalSignals::getInstance()->SignalNewVideoImage(_deviceName, image);
		}
		else if (_signalDetectionEnabled)
		{
			if (checkSignalDetectionManual(image))
				emit GlobalSignals::getInstance()->SignalNewVideoImage(_deviceName, image);
		}
		if (_benchmarkStatus >= 0)
		{
			ColorRgb pixel = image(image.width() / 2, image.height() / 2);
			if ((_benchmarkMessage == "white" && pixel.red > 120 && pixel.green > 120 && pixel.blue > 120) ||
				(_benchmarkMessage == "red" && pixel.red > 120 && pixel.green < 30 && pixel.blue < 30) ||
				(_benchmarkMessage == "green" && pixel.red < 30 && pixel.green > 120 && pixel.blue < 30) ||
				(_benchmarkMessage == "blue" && pixel.red < 30 && pixel.green < 40 && pixel.blue > 120) ||
				(_benchmarkMessage == "black" && pixel.red < 30 && pixel.green < 30 && pixel.blue < 30))

			{
				emit SignalBenchmarkUpdate(_benchmarkStatus, _benchmarkMessage);
				_benchmarkStatus = -1;
				_benchmarkMessage = "";
			}
		}
	}
	else
		frameStat.directAccess = true;
}

void Grabber::benchmarkCapture(int status, QString message)
{
	_benchmarkStatus = status;
	_benchmarkMessage = message;
}

bool Grabber::isInitialized()
{
	return _initialized;
}
