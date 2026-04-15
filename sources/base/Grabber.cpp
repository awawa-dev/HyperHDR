/* Grabber.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2025 awawa-dev
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
	#include <QFile>
	#include <QJsonArray>
	#include <QJsonObject>
#endif

#include <base/Grabber.h>
#include <utils/GlobalSignals.h>

const QString Grabber::AUTO_SETTING = QString("auto");
const int	  Grabber::AUTO_INPUT = -1;
const int	  Grabber::AUTO_FPS = 0;

namespace
{
	const unsigned char pleasewait[] = { 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfc,0x3f,0xff,0xff,0xff,0xff,0xff,0xff,0xfd,0xdf,0xff,0xff,0xff,0xff,0xff,0xff,0xfc,0x1f,0xff,0xff,0xff,0x31,0x88,0x25,0xb5,0xfe,0x21,0x9f,0xfe,0xdb,0x7b,0x6d,0xb6,0x7f,0x6d,0xbf,0xfe,0xfb,0xc,0x6d,0xb5,0xbf,0x6d,0xbf,0xfe,0xdb,0x6f,0x6d,0xb5,0xbf,0x6d,0xbf,0xff,0x30,0x98,0xc5,0x6,0x3f,0x6d,0x1f,0xff,0xff,0xff,0xef,0xff,0xff,0x7f,0xbf,0xff,0xff,0xff,0xef,0xff,0xff,0x7f,0xbf,0xff,0xff,0xff,0xfd,0xff,0xfe,0x7f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0x3c,0x44,0x13,0x9f,0xdb,0x84,0x4f,0xff,0x7e,0xbd,0xbd,0x7f,0xdb,0xb6,0xdf,0xff,0x7e,0x86,0x3b,0xf,0xa5,0xc6,0xdf,0xff,0xe,0xb7,0xb7,0x6f,0xb6,0xf6,0xdf,0xff,0x76,0xcc,0x79,0x9f,0x24,0x8c,0x8f,0xff,0x76,0xff,0xff,0xff,0xff,0xff,0xdf,0xff,0x76,0xff,0xff,0xff,0xff,0xff,0xdf,0xfe,0xc,0xff,0xff,0xff,0xff,0xfe,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff };
};


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
	, _log(grabberName.toUpper())
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
	, _lutCalibrationOverrideActive(false)
	, _fpsSoftwareDecimation(1)
	, _hardware(false)
	, _actualVideoFormat(PixelFormat::NO_CHANGE)
	, _actualWidth(0)
	, _actualHeight(0)
	, _actualFPS(0)
	, _actualDeviceName("")
	, _targetMonitorNits(200)
	, _reorderDisplays(0)
	, _lineLength(-1)
	, _frameByteSize(-1)
	, _signalDetectionEnabled(false)
	, _signalAutoDetectionEnabled(false)
	, _synchro(1)
	, _lutInterpolationBlend(0)
	, _lutMemoryCacheEnabled(true)
	, _lutRuntimeTransitionInterpolationBlend(0)
	, _lutRuntimeTransitionBlend(0)
	, _lutRuntimeTransitionFrameStep(0)
	, _lutRuntimeTransitionActive(false)
	, _lutRuntimeCurrentUsesTransitionSlot(false)
	, _lutRuntimeTransitionTargetUsesTransitionSlot(false)
{
	connect(GlobalSignals::getInstance(), &GlobalSignals::SignalSetLut, this, &Grabber::signalSetLutHandler, Qt::BlockingQueuedConnection);
	connect(GlobalSignals::getInstance(), &GlobalSignals::SignalSetCalibrationLutOverride, this, &Grabber::signalSetCalibrationLutOverrideHandler, Qt::BlockingQueuedConnection);
}

Grabber::~Grabber()
{
	disconnect(GlobalSignals::getInstance(), &GlobalSignals::SignalSetLut, this, &Grabber::signalSetLutHandler);
	disconnect(GlobalSignals::getInstance(), &GlobalSignals::SignalSetCalibrationLutOverride, this, &Grabber::signalSetCalibrationLutOverrideHandler);
}

namespace
{
	constexpr int kLutRuntimeTransitionDurationMs = 180;
	constexpr int kLutRuntimeTransitionMinFrames = 3;
	constexpr int kLutRuntimeTransitionMaxFrames = 12;

	QString normalizeLutPath(const QString& path)
	{
		return QDir::cleanPath(path.trimmed());
	}

	QString resolveSelectedCandidate(const QStringList& configuredCandidates)
	{
		for (const QString& candidate : configuredCandidates)
		{
			const QString normalizedCandidate = normalizeLutPath(candidate);
			if (!normalizedCandidate.isEmpty() && QFileInfo::exists(normalizedCandidate))
				return normalizedCandidate;
		}
		return QString();
	}

	bool selectedCandidateMatchesLoadedFile(const QString& selectedCandidate, const QString& loadedFile)
	{
		if (selectedCandidate.isEmpty())
			return false;
		const QString normalizedLoaded = normalizeLutPath(loadedFile);
		if (normalizedLoaded.isEmpty())
			return false;
		return normalizeLutPath(selectedCandidate).compare(normalizedLoaded, Qt::CaseInsensitive) == 0;
	}

	uint8_t calculateLutRuntimeTransitionFrameStep(const int actualFps)
	{
		const int boundedFps = std::max(actualFps, 1);
		const int transitionFrames = qBound(kLutRuntimeTransitionMinFrames,
			static_cast<int>(std::lround((static_cast<double>(boundedFps) * kLutRuntimeTransitionDurationMs) / 1000.0)),
			kLutRuntimeTransitionMaxFrames);
		return static_cast<uint8_t>(std::max(1, (255 + transitionFrames - 1) / transitionFrames));
	}
}

void Grabber::pleaseWaitForLut(bool videoGrabber)
{
	Image<ColorRgb> image(_actualWidth, _actualHeight);
	double sizeX = _actualWidth / 64.0;
	double sizeY = _actualHeight / 32.0;

	auto timer = InternalClock::now() / 1200;

	ColorRgb fgColor, bgColor;

	if (timer % 8 < 2)
	{
		bgColor = ColorRgb::WHITE;
		fgColor = ColorRgb::BLACK;
	}
	else if (timer % 8 < 4)
	{
		bgColor = ColorRgb::RED;
		fgColor = ColorRgb::BLACK;
	}
	else if (timer % 8 < 6)
	{
		bgColor = ColorRgb::GREEN;
		fgColor = ColorRgb::BLACK;
	}
	else
	{
		bgColor = ColorRgb::BLUE;
		fgColor = ColorRgb::WHITE;
	}

	image.fastBox(0, 0, image.width(), image.height(), bgColor.red, bgColor.green, bgColor.blue);

	for (int y = 0; y < 32; y++)
		for (int x = 0; x < 64; x++)
		{
			int index = (31 - y) * 8 + (x / 8);
			uint8_t bit = 1 << (7 - (x % 8));
			if (index < static_cast<int>(sizeof(pleasewait)) && !(pleasewait[index] & bit))
			{
				int fromX = sizeX * x;
				int fromY = sizeY * y;
				image.fastBox(fromX, fromY, fromX + sizeX, fromY + sizeY, fgColor.red, fgColor.green, fgColor.blue);
			}
		}

	if (videoGrabber)
	{
		emit GlobalSignals::getInstance()->SignalNewVideoImage(_deviceName, image);
	}
	else
	{
		emit GlobalSignals::getInstance()->SignalNewSystemImage(_deviceName, image);
	}

	emit GlobalSignals::getInstance()->SignalLutRequest();
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
	Info(_log, "Capture interface is now {:s}", enable ? "enabled" : "disabled");
	_enabled = enable;
}

void Grabber::setMonitorNits(int nits)
{
	if (static_cast<int>(_targetMonitorNits) != nits)
	{
		_targetMonitorNits = nits;

		Debug(_log, "Set nits to {:d}", _targetMonitorNits);

		if (_initialized && !_blocked)
		{
			Debug(_log, "Restarting video grabber");
			uninit();
			start();
		}
		else
		{
			Info(_log, "Delayed restart of the grabber due to change of monitor nits value");
			_restartNeeded = true;
		}
	}
}

void Grabber::setReorderDisplays(int order)
{
	if (_reorderDisplays != order)
	{
		_reorderDisplays = order;

		Debug(_log, "Set re-order display permutation to {:d}", _reorderDisplays);

		if (_initialized && !_blocked)
		{
			Debug(_log, "Restarting video grabber");
			uninit();
			start();
		}
		else
		{
			Info(_log, "Delayed restart of the grabber due to change of monitor display-order value");
			_restartNeeded = true;
		}
	}
}

void Grabber::setCropping(unsigned cropLeft, unsigned cropRight, unsigned cropTop, unsigned cropBottom)
{
	if (_width > 0 && _height > 0)
	{
		if (cropLeft + cropRight >= (unsigned)_width || cropTop + cropBottom >= (unsigned)_height)
		{
			Error(_log, "Rejecting invalid crop values: left: {:d}, right: {:d}, top: {:d}, bottom: {:d}, higher than height/width {:d}/{:d}", cropLeft, cropRight, cropTop, cropBottom, _height, _width);
			return;
		}
	}

	_cropLeft = cropLeft;
	_cropRight = cropRight;
	_cropTop = cropTop;
	_cropBottom = cropBottom;

	if (cropLeft >= 0 || cropRight >= 0 || cropTop >= 0 || cropBottom >= 0)
	{
		Info(_log, "Cropping image: width={:d} height={:d}; crop: left={:d} right={:d} top={:d} bottom={:d} ", _width, _height, cropLeft, cropRight, cropTop, cropBottom);
	}
}

void Grabber::enableHardwareAcceleration(bool hardware)
{
	if (_hardware != hardware)
	{
		_hardware = hardware;

		Debug(_log, "Set hardware acceleration to {:s}", _hardware ? "enabled" : "disabled");

		if (_initialized && !_blocked)
		{
			Debug(_log, "Restarting video grabber");
			uninit();
			start();
		}
		else
		{
			Info(_log, "Delayed restart of the grabber due to change of the hardware acceleration");
			_restartNeeded = true;
		}
	}
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
			Error(_log, "Rejecting invalid width/height values as it collides with image cropping: width: {:d}, height: {:d}", width, height);
			return false;
		}

		Debug(_log, "Set new width: {:d}, height: {:d} for capture", width, height);
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
	Debug(_log, "setFpsSoftwareDecimation to: {:d}", decimation);
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
	Debug(_log, "Force encoding to: {:s} (old: {:s})", (pixelFormatToString(_enc)), (pixelFormatToString(_oldEnc)));

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

void Grabber::setLutCandidateFiles(const QStringList& files)
{
	QStringList normalized;
	for (const QString& file : files)
	{
		const QString cleaned = QDir::cleanPath(file.trimmed());
		if (cleaned.isEmpty() || normalized.contains(cleaned, Qt::CaseInsensitive))
			continue;
		normalized.append(cleaned);
	}
	_lutCandidateFiles = normalized;
}

QStringList Grabber::getLutCandidateFiles() const
{
	return _lutCandidateFiles;
}

void Grabber::setLutInterpolationCandidateFiles(const QStringList& files, int blend)
{
	QStringList normalized;
	for (const QString& file : files)
	{
		const QString cleaned = QDir::cleanPath(file.trimmed());
		if (cleaned.isEmpty() || normalized.contains(cleaned, Qt::CaseInsensitive))
			continue;
		normalized.append(cleaned);
	}

	_lutInterpolationCandidateFiles = normalized;
	_lutInterpolationBlend = static_cast<uint8_t>(qBound(0, blend, 255));
	if (_lutInterpolationCandidateFiles.isEmpty())
		_lutInterpolationBlend = 0;
}

QStringList Grabber::getLutInterpolationCandidateFiles() const
{
	return _lutInterpolationCandidateFiles;
}

int Grabber::getLutInterpolationBlend() const
{
	return _lutInterpolationBlend;
}

const uint8_t* Grabber::getCurrentLutBuffer() const
{
	if (_lutCalibrationOverrideActive)
		return (_lutBufferInit && _lut.data() != nullptr) ? _lut.data() : nullptr;

	const LutLoader& currentLoader = _lutRuntimeCurrentUsesTransitionSlot ? _lutRuntimeTransitionLoader : static_cast<const LutLoader&>(*this);
	return (currentLoader._lutBufferInit && currentLoader._lut.data() != nullptr) ? currentLoader._lut.data() : nullptr;
}

const uint8_t* Grabber::getCurrentLutInterpolationBuffer() const
{
	if (_lutCalibrationOverrideActive)
		return nullptr;

	const LutLoader& currentLoader = _lutRuntimeCurrentUsesTransitionSlot ? _lutRuntimeTransitionInterpolationLoader : _lutInterpolationLoader;
	return (currentLoader._lutBufferInit && currentLoader._lut.data() != nullptr) ? currentLoader._lut.data() : nullptr;
}

uint8_t Grabber::getCurrentLutInterpolationBlend() const
{
	if (_lutCalibrationOverrideActive)
		return 0;

	return _lutRuntimeCurrentUsesTransitionSlot ? _lutRuntimeTransitionInterpolationBlend : _lutInterpolationBlend;
}

const uint8_t* Grabber::getTransitionTargetLutBuffer() const
{
	if (_lutCalibrationOverrideActive || !_lutRuntimeTransitionActive)
		return nullptr;

	const LutLoader& targetLoader = _lutRuntimeTransitionTargetUsesTransitionSlot ? _lutRuntimeTransitionLoader : static_cast<const LutLoader&>(*this);
	return (targetLoader._lutBufferInit && targetLoader._lut.data() != nullptr) ? targetLoader._lut.data() : nullptr;
}

const uint8_t* Grabber::getTransitionTargetLutInterpolationBuffer() const
{
	if (_lutCalibrationOverrideActive || !_lutRuntimeTransitionActive)
		return nullptr;

	const LutLoader& targetLoader = _lutRuntimeTransitionTargetUsesTransitionSlot ? _lutRuntimeTransitionInterpolationLoader : _lutInterpolationLoader;
	return (targetLoader._lutBufferInit && targetLoader._lut.data() != nullptr) ? targetLoader._lut.data() : nullptr;
}

uint8_t Grabber::getTransitionTargetLutInterpolationBlend() const
{
	if (_lutCalibrationOverrideActive || !_lutRuntimeTransitionActive)
		return 0;
	return _lutRuntimeTransitionTargetUsesTransitionSlot ? _lutRuntimeTransitionInterpolationBlend : _lutInterpolationBlend;
}

uint8_t Grabber::getLutRuntimeTransitionBlend() const
{
	if (_lutCalibrationOverrideActive)
		return 0;

	return _lutRuntimeTransitionActive ? _lutRuntimeTransitionBlend : 0;
}

bool Grabber::isLutRuntimeTransitionActive() const
{
	return !_lutCalibrationOverrideActive && _lutRuntimeTransitionActive;
}

bool Grabber::isCalibrationLutOverrideActive() const
{
	return _lutCalibrationOverrideActive;
}

void Grabber::advanceLutRuntimeTransition()
{
	if (!_lutRuntimeTransitionActive)
		return;

	const int nextBlend = std::min(255, static_cast<int>(_lutRuntimeTransitionBlend) + std::max(1, static_cast<int>(_lutRuntimeTransitionFrameStep)));
	_lutRuntimeTransitionBlend = static_cast<uint8_t>(nextBlend);
	if (_lutRuntimeTransitionBlend < 255)
		return;

	// Propagate the target's interpolation blend into the slot that is about
	// to become "current" so getCurrentLutInterpolationBlend() returns the
	// correct value immediately after the flip.
	const uint8_t targetInterpolation = _lutRuntimeTransitionTargetUsesTransitionSlot
		? _lutRuntimeTransitionInterpolationBlend
		: _lutInterpolationBlend;

	_lutRuntimeCurrentUsesTransitionSlot = _lutRuntimeTransitionTargetUsesTransitionSlot;
	_lutRuntimeTransitionActive = false;
	_lutRuntimeTransitionBlend = 0;
	_lutRuntimeTransitionFrameStep = 0;
	_lutRuntimeTransitionTargetUsesTransitionSlot = _lutRuntimeCurrentUsesTransitionSlot;

	// Write the propagated blend into the now-current slot.
	if (_lutRuntimeCurrentUsesTransitionSlot)
		_lutRuntimeTransitionInterpolationBlend = targetInterpolation;
	else
		_lutInterpolationBlend = targetInterpolation;
}

void Grabber::setLutMemoryCacheEnabled(bool enabled)
{
	_lutMemoryCacheEnabled = enabled;
}

bool Grabber::isLutMemoryCacheEnabled() const
{
	return _lutMemoryCacheEnabled;
}

void Grabber::clearLutMemoryCache()
{
	const int count = LutLoader::getMemoryCacheEntryCount();
	LutLoader::clearLutMemoryCache();
	if (count > 0)
		Info(_log, "Cleared LUT memory cache ({:d} entries released)", count);
}

PixelFormat Grabber::getCurrentLutPixelFormatHint() const
{
	const PixelFormat format = (_actualVideoFormat != PixelFormat::NO_CHANGE) ? _actualVideoFormat : _enc;
	switch (format)
	{
		case PixelFormat::YUYV:
		case PixelFormat::UYVY:
		case PixelFormat::I420:
		case PixelFormat::NV12:
		case PixelFormat::P010:
		case PixelFormat::MJPEG:
			return PixelFormat::YUYV;
		default:
			return PixelFormat::RGB24;
	}
}

bool Grabber::reloadLut(QString& error)
{
	if (tryPrepareLutRuntimeTransition(_log, getCurrentLutPixelFormatHint(),
		_lutCandidateFiles, _lutInterpolationCandidateFiles, _lutInterpolationBlend, error))
		return error.isEmpty();

	return loadConfiguredLutSelection(_log, getCurrentLutPixelFormatHint(), error);
}

bool Grabber::tryPrepareLutRuntimeTransition(const LoggerName& log, PixelFormat color,
	const QStringList& primaryCandidates, const QStringList& secondaryCandidates, uint8_t interpolationBlend, QString& error)
{
	if (_lutCalibrationOverrideActive || !_lutMemoryCacheEnabled || getCurrentLutBuffer() == nullptr || primaryCandidates.isEmpty())
		return false;

	const bool interpolationRequested = interpolationBlend > 0 && !secondaryCandidates.isEmpty();
	const QString resolvedPrimaryCandidate = resolveSelectedCandidate(primaryCandidates);
	const QString resolvedInterpolationCandidate = interpolationRequested ? resolveSelectedCandidate(secondaryCandidates) : QString();
	if (resolvedPrimaryCandidate.isEmpty())
		return false;

	const QString currentPrimaryFile = _lutRuntimeCurrentUsesTransitionSlot ? _lutRuntimeTransitionLoader.getLoadedLutFile() : _loadedLutFile;
	const QString currentInterpolationFile = _lutRuntimeCurrentUsesTransitionSlot ? _lutRuntimeTransitionInterpolationLoader.getLoadedLutFile() : _lutInterpolationLoader.getLoadedLutFile();
	const int currentLoadedHdrMode = _lutRuntimeCurrentUsesTransitionSlot ? _lutRuntimeTransitionLoader._loadedHdrToneMappingMode : _loadedHdrToneMappingMode;
	if (selectedCandidateMatchesLoadedFile(resolvedPrimaryCandidate, currentPrimaryFile) &&
		(!interpolationRequested || selectedCandidateMatchesLoadedFile(resolvedInterpolationCandidate, currentInterpolationFile)) &&
		currentLoadedHdrMode == _hdrToneMappingEnabled)
	{
		if (_lutRuntimeCurrentUsesTransitionSlot)
			_lutRuntimeTransitionInterpolationBlend = interpolationRequested ? interpolationBlend : 0;
		else
			_lutInterpolationBlend = interpolationRequested ? interpolationBlend : 0;

		_lutRuntimeTransitionActive = false;
		_lutRuntimeTransitionBlend = 0;
		_lutRuntimeTransitionFrameStep = 0;
		error.clear();
		return true;
	}

	const bool targetUsesTransitionSlot = !_lutRuntimeCurrentUsesTransitionSlot;
	LutLoader& targetPrimaryLoader = targetUsesTransitionSlot ? _lutRuntimeTransitionLoader : static_cast<LutLoader&>(*this);
	LutLoader* targetInterpolationLoader = targetUsesTransitionSlot ? &_lutRuntimeTransitionInterpolationLoader : &_lutInterpolationLoader;
	if (!loadLutSelection(log, color, _hdrToneMappingEnabled,
		primaryCandidates, secondaryCandidates, interpolationBlend, true,
		targetPrimaryLoader, targetInterpolationLoader, error))
	{
		return true;
	}

	if (targetUsesTransitionSlot)
		_lutRuntimeTransitionInterpolationBlend = interpolationRequested ? interpolationBlend : 0;
	else
		_lutInterpolationBlend = interpolationRequested ? interpolationBlend : 0;

	// Clear the current slot's interpolation blend so the outgoing side of the
	// crossfade does not keep decoding with a stale DV bucket blend value.
	if (_lutRuntimeCurrentUsesTransitionSlot)
		_lutRuntimeTransitionInterpolationBlend = 0;
	else
		_lutInterpolationBlend = 0;

	_lutRuntimeTransitionTargetUsesTransitionSlot = targetUsesTransitionSlot;
	_lutRuntimeTransitionBlend = 0;
	_lutRuntimeTransitionFrameStep = calculateLutRuntimeTransitionFrameStep((_actualFPS > 0) ? _actualFPS : _fps);
	_lutRuntimeTransitionActive = true;
	Debug(log, "Starting cached LUT runtime transition: currentSlot={:s}, targetSlot={:s}, step={:d}",
		_lutRuntimeCurrentUsesTransitionSlot ? "transition" : "base",
		_lutRuntimeTransitionTargetUsesTransitionSlot ? "transition" : "base",
		_lutRuntimeTransitionFrameStep);
		error.clear();
	return true;
}

QJsonObject Grabber::getLutRuntimeInfo() const
{
	QJsonObject runtime;
	QJsonArray candidates;
	QJsonArray interpolationCandidates;
	for (const QString& file : _lutCandidateFiles)
		candidates.append(file);
	for (const QString& file : _lutInterpolationCandidateFiles)
		interpolationCandidates.append(file);

	runtime["candidateFiles"] = candidates;
	runtime["interpolationCandidateFiles"] = interpolationCandidates;
	const QString currentLoadedFile = _lutRuntimeCurrentUsesTransitionSlot ? _lutRuntimeTransitionLoader.getLoadedLutFile() : _loadedLutFile;
	const QString currentInterpolationFile = _lutRuntimeCurrentUsesTransitionSlot ? _lutRuntimeTransitionInterpolationLoader.getLoadedLutFile() : _lutInterpolationLoader.getLoadedLutFile();
	runtime["lutBufferInit"] = (getCurrentLutBuffer() != nullptr);
	runtime["hdrToneMappingEnabled"] = (_hdrToneMappingEnabled != 0);
	runtime["loadedFile"] = currentLoadedFile;
	runtime["loadedFileExists"] = !currentLoadedFile.isEmpty() && QFileInfo::exists(currentLoadedFile);
	runtime["loadedCompressed"] = _lutRuntimeCurrentUsesTransitionSlot ? _lutRuntimeTransitionLoader.isLoadedLutCompressed() : _loadedLutCompressed;
	runtime["memoryCacheEnabled"] = _lutMemoryCacheEnabled;
	runtime["memoryCacheEntries"] = LutLoader::getMemoryCacheEntryCount();
	runtime["memoryCacheMaxEntries"] = LutLoader::getMemoryCacheMaxEntries();
	runtime["interpolationBlend"] = getCurrentLutInterpolationBlend();
	runtime["interpolationBlendRatio"] = static_cast<double>(getCurrentLutInterpolationBlend()) / 255.0;
	runtime["loadedInterpolationFile"] = currentInterpolationFile;
	runtime["loadedInterpolationFileExists"] = !currentInterpolationFile.isEmpty() && QFileInfo::exists(currentInterpolationFile);
	runtime["loadedInterpolationCompressed"] = _lutRuntimeCurrentUsesTransitionSlot ? _lutRuntimeTransitionInterpolationLoader.isLoadedLutCompressed() : _lutInterpolationLoader.isLoadedLutCompressed();
	runtime["interpolationActive"] = getCurrentLutInterpolationBlend() > 0 && getCurrentLutInterpolationBuffer() != nullptr;
	runtime["runtimeTransitionActive"] = _lutRuntimeTransitionActive;
	runtime["runtimeTransitionBlend"] = _lutRuntimeTransitionBlend;
	runtime["runtimeTransitionBlendRatio"] = static_cast<double>(_lutRuntimeTransitionBlend) / 255.0;
	runtime["runtimeTransitionFrameStep"] = _lutRuntimeTransitionFrameStep;
	if (_lutRuntimeTransitionActive)
	{
		const QString targetLoadedFile = _lutRuntimeTransitionTargetUsesTransitionSlot ? _lutRuntimeTransitionLoader.getLoadedLutFile() : _loadedLutFile;
		const QString targetInterpolationFile = _lutRuntimeTransitionTargetUsesTransitionSlot ? _lutRuntimeTransitionInterpolationLoader.getLoadedLutFile() : _lutInterpolationLoader.getLoadedLutFile();
		runtime["runtimeTransitionTargetFile"] = targetLoadedFile;
		runtime["runtimeTransitionTargetInterpolationFile"] = targetInterpolationFile;
		runtime["runtimeTransitionTargetInterpolationBlend"] = getTransitionTargetLutInterpolationBlend();
		runtime["runtimeTransitionTargetInterpolationBlendRatio"] = static_cast<double>(getTransitionTargetLutInterpolationBlend()) / 255.0;
	}

	if (getCurrentLutBuffer() != nullptr)
	{
		uint32_t checkSum = 0;
		for (int i = 0; i < 256; i += 2)
			for (int j = 32; j <= 160; j += 64)
				checkSum ^= *(reinterpret_cast<const uint32_t*>(&(getCurrentLutBuffer()[LUT_INDEX(j, i, (255 - i))])));
		runtime["lutFastCRC"] = "0x" + QString("%1").arg(checkSum, 4, 16).toUpper();
	}

	return runtime;
}

bool Grabber::loadConfiguredLutSelection(const LoggerName& log, PixelFormat color, QString& error)
{
	return loadLutSelection(log, color, _hdrToneMappingEnabled,
		_lutCandidateFiles, _lutInterpolationCandidateFiles, _lutInterpolationBlend, _lutMemoryCacheEnabled,
		*this, &_lutInterpolationLoader, error);
}

void Grabber::clearCalibrationLutOverride()
{
	_lutCalibrationOverrideActive = false;
}

bool Grabber::loadLutSelection(const LoggerName& log, PixelFormat color, int hdrToneMappingEnabled,
	const QStringList& primaryCandidates, const QStringList& secondaryCandidates, uint8_t interpolationBlend, bool useMemoryCache,
	LutLoader& primaryLoader, LutLoader* secondaryLoader, QString& error)
{
	if (primaryCandidates.isEmpty())
	{
		error = "No LUT candidate files configured";
		return false;
	}

	primaryLoader._hdrToneMappingEnabled = hdrToneMappingEnabled;
	primaryLoader.loadLutFile(log, color, primaryCandidates, useMemoryCache);
	if (!primaryLoader._lutBufferInit)
	{
		error = "Could not load any required LUT file";
		return false;
	}

	if (secondaryLoader != nullptr)
	{
		secondaryLoader->_lutBufferInit = false;
		secondaryLoader->_loadedLutFile.clear();
		secondaryLoader->_loadedLutCompressed = false;
		secondaryLoader->_lut.releaseMemory();

		if (interpolationBlend > 0 && !secondaryCandidates.isEmpty())
		{
			secondaryLoader->_hdrToneMappingEnabled = hdrToneMappingEnabled;
			secondaryLoader->loadLutFile(log, color, secondaryCandidates, useMemoryCache);
			if (!secondaryLoader->_lutBufferInit)
			{
				error = "Could not load the interpolation LUT file";
				return false;
			}
		}
	}

	error.clear();
	return true;
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
				Debug(_log, "Reset brightness to default: {:d} (user value: 0)", brightness);
			}

			if (_contrast != contrast && contrast == 0 && dev.contrast.enabled)
			{
				contrast = (int)dev.contrast.defVal;
				Debug(_log, "Reset contrast to default: {:d} (user value: 0)", contrast);
			}

			if (_saturation != saturation && saturation == 0 && dev.saturation.enabled)
			{
				saturation = (int)dev.saturation.defVal;
				Debug(_log, "Reset saturation to default: {:d} (user value: 0)", saturation);
			}

			if (_hue != hue && hue == 0 && dev.hue.enabled)
			{
				hue = (int)dev.hue.defVal;
				Debug(_log, "Reset hue to default: {:d} (user value: 0)", hue);
			}
		}

		_brightness = brightness;
		_contrast = contrast;
		_saturation = saturation;
		_hue = hue;

		Debug(_log, "Set brightness to {:d}, contrast to {:d}, saturation to {:d}, hue to {:d}", _brightness, _contrast, _saturation, _hue);

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
	Info(_log, "{:s}", (QString("setQFrameDecimation is now: %1").arg(_qframe ? "enabled" : "disabled")));
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
		Debug(_log, "setWidthHeight preparing to restarting video grabber {:d}", _initialized);

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
		Debug(_log, "setDeviceVideoStandard preparing to restart video grabber. Old: '{:s}' new: '{:s}'", (_deviceName), (device));

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

int Grabber::getTargetSystemFrameDimension(int actualWidth, int actualHeight, int& targetSizeX, int& targetSizeY)
{
	int realSizeX = actualWidth;
	int realSizeY = actualHeight;

	if (realSizeX <= 16 || realSizeY <= 16)
	{
		realSizeX = actualWidth;
		realSizeY = actualHeight;
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

	FrameDecoder::processSystemImageBGRA(image, targetSizeX, targetSizeY, _cropLeft, _cropTop, source, _actualWidth, _actualHeight, divide, (_hdrToneMappingEnabled == 0 || getCurrentLutBuffer() == nullptr || !useLut) ? nullptr : const_cast<uint8_t*>(getCurrentLutBuffer()), lineSize);

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

	FrameDecoder::processSystemImageBGR(image, targetSizeX, targetSizeY, _cropLeft, _cropTop, source, _actualWidth, _actualHeight, divide, (_hdrToneMappingEnabled == 0 || getCurrentLutBuffer() == nullptr) ? nullptr : const_cast<uint8_t*>(getCurrentLutBuffer()), lineSize);

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

	FrameDecoder::processSystemImageBGR16(image, targetSizeX, targetSizeY, _cropLeft, _cropTop, source, _actualWidth, _actualHeight, divide, (_hdrToneMappingEnabled == 0 || getCurrentLutBuffer() == nullptr) ? nullptr : const_cast<uint8_t*>(getCurrentLutBuffer()), lineSize);

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

	FrameDecoder::processSystemImageRGBA(image, targetSizeX, targetSizeY, _cropLeft, _cropTop, source, _actualWidth, _actualHeight, divide, (_hdrToneMappingEnabled == 0 || getCurrentLutBuffer() == nullptr) ? nullptr : const_cast<uint8_t*>(getCurrentLutBuffer()), lineSize);

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

	FrameDecoder::processSystemImagePQ10(image, targetSizeX, targetSizeY, _cropLeft, _cropTop, source, _actualWidth, _actualHeight, divide, (_hdrToneMappingEnabled == 0 || getCurrentLutBuffer() == nullptr) ? nullptr : const_cast<uint8_t*>(getCurrentLutBuffer()), lineSize);

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
		Info(_log, "Signal detection is now {:s}", enable ? "enabled" : "disabled");
	}
}

void Grabber::setAutoSignalDetectionEnable(bool enable)
{
	if (_signalAutoDetectionEnabled != enable)
	{
		_signalAutoDetectionEnabled = enable;
		Info(_log, "Automatic signal detection is now {:s}", enable ? "enabled" : "disabled");
	}
}

void Grabber::resetSignalDetection()
{
	resetStats();
	resetManualDetection();
}

void Grabber::revive()
{
	bool signalLostByDetection = false;

	if (_signalAutoDetectionEnabled)
	{
		signalLostByDetection = getDetectionAutoSignal();
	}
	else if (_signalDetectionEnabled)
	{
		signalLostByDetection = getDetectionManualSignal();
	}

	if (signalLostByDetection && _initialized)
	{
		Warning(_log, "The video control requested for the grabber restart due to signal lost, but it's caused by signal detection. No restart.");
		return;
	}

	if (_synchro.tryAcquire())
	{
		if (signalLostByDetection)
		{
			Warning(_log, "Signal detection reported lost but grabber is not initialized. Resetting detection state for recovery.");
			resetSignalDetection();
		}

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
		for (const auto& x : std::as_const(resolutions))
		{
			availableResolutions.append(x);
		}
		device.insert("resolutions", availableResolutions);

		QJsonArray availableVideoCodec;
		videoCodecs.sort();
		for (const auto& x : std::as_const(videoCodecs))
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

	const QJsonObject lutRuntime = getLutRuntimeInfo();
	for (auto iter = lutRuntime.begin(); iter != lutRuntime.end(); ++iter)
		grabbers.insert(iter.key(), iter.value());

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

void Grabber::handleNewFrame(unsigned int /*workerIndex*/, Image<ColorRgb> image, quint64 /*sourceCount*/, qint64 _frameBegin)
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
	}
	else
		frameStat.directAccess = true;
}

bool Grabber::isInitialized()
{
	return _initialized;
}

void Grabber::signalSetLutHandler(MemoryBuffer<uint8_t>* lut)
{
	if (lut != nullptr && _lut.size() >= lut->size())
	{
		memcpy(_lut.data(), lut->data(), lut->size());
		_lutBufferInit = true;
		_lutCalibrationOverrideActive = true;
		_lutRuntimeTransitionActive = false;
		_lutRuntimeTransitionBlend = 0;
		_lutRuntimeTransitionFrameStep = 0;
		_lutRuntimeTransitionTargetUsesTransitionSlot = _lutRuntimeCurrentUsesTransitionSlot;
		Info(_log, "The byte array loaded into LUT");
	}
	else
		Error(_log, "Could not set LUT: current size = {:d}, incoming size = {:d}", _lut.size(), (lut != nullptr) ? lut->size() : 0);
}

void Grabber::signalSetCalibrationLutOverrideHandler(bool enabled)
{
	if (enabled)
		_lutCalibrationOverrideActive = true;
	else
		clearCalibrationLutOverride();
}

void Grabber::setAutomaticToneMappingConfig(bool enabled, const AutomaticToneMapping::ToneMappingThresholds& newConfig, int timeInSec, int timeToDisableInMSec)
{
	_automaticToneMapping.setConfig(enabled, newConfig, timeInSec, timeToDisableInMSec);
	if (_automaticToneMapping.prepare() && !_qframe)
		Error(_log, "Automatic tone mapping requires 'Scale frame size to 25%' mode enabled");
	if (_automaticToneMapping.prepare() && (_enc != PixelFormat::YUYV && _enc != PixelFormat::NV12 && _enc != PixelFormat::P010 ))
		Warning(_log, "Automatic tone mapping requires YUYV/NV12/P010 video format");

}

void Grabber::setAutoToneMappingCurrentStateEnabled(bool enabled)
{
	_automaticToneMapping.setToneMapping(enabled);
}
