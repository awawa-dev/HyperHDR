/* V4L2Grabber.cpp
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

#include <QDirIterator>
#include <QFileInfo>
#include <QSocketNotifier>

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <limits.h>

#include <base/HyperHdrInstance.h>
#include <utils/GlobalSignals.h>

#include <grabber/linux/v4l2/V4L2Grabber.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

#ifndef V4L2_CAP_META_CAPTURE
	#define V4L2_CAP_META_CAPTURE 0x00800000 // Specified in kernel header v4.16. Required for backward compatibility.
#endif

// some stuff for HDR tone mapping
#define LUT_FILE_SIZE 50331648

static const V4L2Grabber::HyperHdrFormat supportedFormats[] =
{
	{ V4L2_PIX_FMT_YUYV,   PixelFormat::YUYV },
	{ V4L2_PIX_FMT_XRGB32, PixelFormat::XRGB },
	{ V4L2_PIX_FMT_RGB24,  PixelFormat::RGB24 },
	{ V4L2_PIX_FMT_YUV420, PixelFormat::I420 },
	{ V4L2_PIX_FMT_NV12,   PixelFormat::NV12 },
	{ V4L2_PIX_FMT_MJPEG,  PixelFormat::MJPEG }
};


V4L2Grabber::V4L2Grabber(const QString& device, const QString& configurationPath)
	: Grabber(configurationPath, "V4L2:" + device.left(14))
	, _fileDescriptor(-1)
	, _buffers()
	, _streamNotifier(nullptr)

{
	// Refresh devices
	getV4L2devices();
}

QString V4L2Grabber::GetSharedLut()
{
	char result[PATH_MAX];

	ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
	if (count < 0)
	{
		Debug(_log, "Readlink failed");
		return "";
	}

	std::string appPath = std::string(result, (count > 0) ? count : 0);
	std::size_t found = appPath.find_last_of("/\\");

	QString   ret = QString("%1%2").arg(QString::fromStdString(appPath.substr(0, found))).arg("/../lut");
	QFileInfo info(ret);

	ret = info.absoluteFilePath();
	Debug(_log, "LUT folder location: '%s'", QSTRING_CSTR(ret));
	return ret;
}

void V4L2Grabber::loadLutFile(PixelFormat color)
{
	// load lut table
	QString fileName1 = QString("%1%2").arg(_configurationPath).arg("/lut_lin_tables.3d");
	QString fileName2 = QString("%1%2").arg(GetSharedLut()).arg("/lut_lin_tables.3d");
	QString fileName3 = QString("/usr/share/hyperhdr/lut/lut_lin_tables.3d");

	Grabber::loadLutFile(_log, color, QList<QString>{fileName1, fileName2, fileName3});
}

void V4L2Grabber::setHdrToneMappingEnabled(int mode)
{
	if (_hdrToneMappingEnabled != mode || _lut.data() == nullptr)
	{
		_hdrToneMappingEnabled = mode;
		if (_lut.data() != nullptr || !mode)
			Debug(_log, "setHdrToneMappingMode to: %s", (mode == 0) ? "Disabled" : ((mode == 1) ? "Fullscreen" : "Border mode"));
		else
			Warning(_log, "setHdrToneMappingMode to: enable, but the LUT file is currently unloaded");

		if (_V4L2WorkerManager.isActive())
		{
			Debug(_log, "setHdrToneMappingMode replacing LUT and restarting");
			_V4L2WorkerManager.Stop();
			if ((_actualVideoFormat == PixelFormat::YUYV) || (_actualVideoFormat == PixelFormat::I420) || (_actualVideoFormat == PixelFormat::NV12) || (_actualVideoFormat == PixelFormat::MJPEG))
				loadLutFile(PixelFormat::YUYV);
			else
				loadLutFile(PixelFormat::RGB24);
			_V4L2WorkerManager.Start();
		}
		emit SignalSetNewComponentStateToAllInstances(hyperhdr::Components::COMP_HDR, (mode != 0));
	}
	else
		Debug(_log, "setHdrToneMappingMode nothing changed: %s", (mode == 0) ? "Disabled" : ((mode == 1) ? "Fullscreen" : "Border mode"));
}

V4L2Grabber::~V4L2Grabber()
{
	uninit();
}

void V4L2Grabber::uninit()
{
	// stop if the grabber was not stopped
	if (_initialized)
	{
		Debug(_log, "Uninit grabber: %s", QSTRING_CSTR(_deviceName));
		stop();
	}
}

bool V4L2Grabber::init()
{
	if (!_initialized)
	{
		QString foundDevice = "";
		int     foundIndex = -1;
		int     bestGuess = -1;
		int     bestGuessMinX = INT_MAX;
		int     bestGuessMinFPS = INT_MAX;
		bool    autoDiscovery = (QString::compare(_deviceName, Grabber::AUTO_SETTING, Qt::CaseInsensitive) == 0);

		enumerateV4L2devices(true);

		if (!autoDiscovery && !_deviceProperties.contains(_deviceName))
		{

			for (auto it = _deviceProperties.begin(); it != _deviceProperties.end(); ++it)
				if (it.value().name == _deviceName)
				{
					foundDevice = it.key();
					Debug(_log, "Using new name %s for %s", QSTRING_CSTR(foundDevice), QSTRING_CSTR(_deviceName));
					_deviceName = foundDevice;
				}

			if (foundDevice.isNull() || foundDevice.isEmpty())
			{
				Debug(_log, "Device %s is not available. Changing to auto.", QSTRING_CSTR(_deviceName));
				autoDiscovery = true;
			}
		}

		if (autoDiscovery)
		{
			Debug(_log, "Forcing auto discovery device");
			for (auto it = _deviceProperties.begin(); it != _deviceProperties.end(); ++it)
				if (it.value().valid.count() > 0)
				{
					foundDevice = it.key();
					_deviceName = foundDevice;
					Debug(_log, "Auto discovery set to %s", QSTRING_CSTR(_deviceName));
				}
		}
		else
			foundDevice = _deviceName;

		if (foundDevice.isNull() || foundDevice.isEmpty() || !_deviceProperties.contains(foundDevice))
		{
			Error(_log, "Could not find any capture device %s", QSTRING_CSTR(foundDevice));
			return false;
		}

		DeviceProperties dev = _deviceProperties[foundDevice];

		Debug(_log, "Searching for %s %d x %d @ %d fps, input: %i (%s)", QSTRING_CSTR(foundDevice), _width, _height, _fps, _input, QSTRING_CSTR(pixelFormatToString(_enc)));

		for (int i = 0; i < dev.valid.count() && foundIndex < 0; i++)
		{
			bool strict = false;
			const auto& val = dev.valid[i];

			if (bestGuess == -1 ||
				(val.x <= bestGuessMinX && val.x >= 640 &&
				 ((val.x > 800 && val.fps <= bestGuessMinFPS && val.fps >= 10) ||
				  (val.x <= 800 && val.fps > bestGuessMinFPS))))
			{
				bestGuess = i;
				bestGuessMinFPS = val.fps;
				bestGuessMinX = val.x;
			}

			if (_width && _height)
			{
				strict = true;
				if (val.x != _width || val.y != _height)
					continue;
			}

			if (_fps && _fps != Grabber::AUTO_FPS)
			{
				strict = true;
				if (val.fps != _fps)
					continue;
			}

			if (_input >= 0)
			{
				strict = true;
				if (val.input != _input)
					continue;
			}

			if (_enc != PixelFormat::NO_CHANGE)
			{
				strict = true;
				if (val.pf != _enc)
					continue;
			}

			if (strict && (val.fps <= 60 || _fps != Grabber::AUTO_FPS))
			{
				foundIndex = i;
			}
		}

		if (foundIndex < 0 && bestGuess >= 0)
		{
			if (!autoDiscovery)
				Warning(_log, "Selected resolution not found in supported modes. Forcing best guess");
			else
				Warning(_log, "Forcing best guess");

			foundIndex = bestGuess;
		}

		if (foundIndex >= 0)
		{
			try
			{
				Info(_log, "*************************************************************************************************");
				Info(_log, "Starting V4L2 grabber. Selected: %s [%s] %d x %d @ %d fps %s input %d", QSTRING_CSTR(foundDevice), QSTRING_CSTR(dev.name),
					dev.valid[foundIndex].x, dev.valid[foundIndex].y, dev.valid[foundIndex].fps,
					QSTRING_CSTR(pixelFormatToString(dev.valid[foundIndex].pf)), _input);
				Info(_log, "*************************************************************************************************");

				if (init_device(foundDevice, dev.valid[foundIndex]))
				{
					_initialized = true;
				}
			}
			catch (std::exception& e)
			{
				Error(_log, "V4l2 init failed (%s). Closing...", e.what());

				uninit_device();
				close_device();
			}
		}
		else
			Error(_log, "Could not find any capture device settings");
	}

	return _initialized;
}



QString V4L2Grabber::formatRes(int w, int h, QString format)
{
	QString ws = QString::number(w);
	QString hs = QString::number(h);

	if (format.isEmpty())
		return ws + "x" + hs;
	else
	{
		while (ws.length() < 4)
			ws = " " + ws;
		while (hs.length() < 4)
			hs = " " + hs;

		return ws + "x" + hs + " " + format;
	}
}

QString V4L2Grabber::formatFrame(int fr)
{
	QString frame = QString::number(fr);

	while (frame.length() < 2)
		frame = " " + frame;

	return frame;
}

PixelFormat V4L2Grabber::identifyFormat(__u32 format)
{
	for (size_t i = 0; i < sizeof(supportedFormats) / sizeof(supportedFormats[0]); i++)
		if (format == supportedFormats[i].v4l2Format)
		{
			return supportedFormats[i].innerFormat;
		}

	return PixelFormat::NO_CHANGE;
}

void V4L2Grabber::getV4L2devices()
{
	enumerateV4L2devices(false);
}

void V4L2Grabber::enumerateV4L2devices(bool silent)
{

	QDirIterator it("/sys/class/video4linux/", QDirIterator::NoIteratorFlags);
	_deviceProperties.clear();
	while (it.hasNext())
	{
		QString dev = it.next();
		if (it.fileName().startsWith("video"))
		{
			QString devName = "/dev/" + it.fileName();

			int fd = open(QSTRING_CSTR(devName), O_RDWR | O_NONBLOCK, 0);

			if (fd < 0)
			{
				throw_errno_exception("Cannot open '" + devName + "'");
				continue;
			}

			struct v4l2_capability cap;
			CLEAR(cap);

			if (xioctl(fd, VIDIOC_QUERYCAP, &cap) < 0)
			{
				throw_errno_exception("'" + devName + "' is no V4L2 device");
				close(fd);
				continue;
			}

			if (cap.device_caps & V4L2_CAP_META_CAPTURE)
			{
				close(fd);
				continue;
			}

			// get the current settings
			struct v4l2_format fmt;
			CLEAR(fmt);

			fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			if (xioctl(fd, VIDIOC_G_FMT, &fmt) < 0)
			{
				close(fd);
				continue;
			}

			DeviceProperties properties;
			properties.name = devName;

			Info(_log, "Found capture device: %s", QSTRING_CSTR(devName));

			// get device capabilities			
			if (getControl(fd, V4L2_CID_BRIGHTNESS, properties.brightness.minVal, properties.brightness.maxVal, properties.brightness.defVal))
			{
				properties.brightness.enabled = true;
				Debug(_log, "Device has 'brightness' control => min: %i, max: %i, default: %i", int(properties.brightness.minVal), int(properties.brightness.maxVal), int(properties.brightness.defVal));
			}

			if (getControl(fd, V4L2_CID_CONTRAST, properties.contrast.minVal, properties.contrast.maxVal, properties.contrast.defVal))
			{
				properties.contrast.enabled = true;
				Debug(_log, "Device has 'contrast' control => min: %i, max: %i, default: %i", int(properties.contrast.minVal), int(properties.contrast.maxVal), int(properties.contrast.defVal));
			}

			if (getControl(fd, V4L2_CID_SATURATION, properties.saturation.minVal, properties.saturation.maxVal, properties.saturation.defVal))
			{
				properties.saturation.enabled = true;
				Debug(_log, "Device has 'saturation' control => min: %i, max: %i, default: %i", int(properties.saturation.minVal), int(properties.saturation.maxVal), int(properties.saturation.defVal));
			}

			if (getControl(fd, V4L2_CID_HUE, properties.hue.minVal, properties.hue.maxVal, properties.hue.defVal))
			{
				properties.hue.enabled = true;
				Debug(_log, "Device has 'hue' control => min: %i, max: %i, default: %i", int(properties.hue.minVal), int(properties.hue.maxVal), int(properties.hue.defVal));
			}

			// collect available device inputs (index & name)
			int inputIndex;
			if (xioctl(fd, VIDIOC_G_INPUT, &inputIndex) == 0)
			{
				struct v4l2_input input;
				CLEAR(input);

				input.index = 0;
				while (xioctl(fd, VIDIOC_ENUMINPUT, &input) >= 0)
				{
					properties.inputs.insert(QString((char*)input.name), input.index);

					struct v4l2_fmtdesc formatDesc;
					CLEAR(formatDesc);

					formatDesc.index = 0;
					formatDesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

					while (xioctl(fd, VIDIOC_ENUM_FMT, &formatDesc) == 0)
					{
						// collect available device resolutions & frame rates
						struct v4l2_frmsizeenum frmsizeenum;
						CLEAR(frmsizeenum);

						frmsizeenum.index = 0;
						frmsizeenum.pixel_format = formatDesc.pixelformat;
						while (xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsizeenum) >= 0)
						{
							switch (frmsizeenum.type)
							{
								case V4L2_FRMSIZE_TYPE_DISCRETE:
								{
									struct v4l2_frmivalenum searchFrameEnum;
									CLEAR(searchFrameEnum);

									QString pixelFormat = pixelFormatToString(identifyFormat(formatDesc.pixelformat));
									QString resolution = formatRes(frmsizeenum.discrete.width, frmsizeenum.discrete.height, pixelFormat);
									QString displayResolutions = formatRes(frmsizeenum.discrete.width, frmsizeenum.discrete.height, "");

									searchFrameEnum.index = 0;
									searchFrameEnum.pixel_format = frmsizeenum.pixel_format;
									searchFrameEnum.width = frmsizeenum.discrete.width;
									searchFrameEnum.height = frmsizeenum.discrete.height;

									while (xioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &searchFrameEnum) >= 0)
									{
										int frameRate = -1;
										switch (searchFrameEnum.type)
										{
											case V4L2_FRMSIZE_TYPE_DISCRETE:
												if (searchFrameEnum.discrete.numerator != 0)
												{
													frameRate = searchFrameEnum.discrete.denominator / searchFrameEnum.discrete.numerator;
												}
												break;
											case V4L2_FRMSIZE_TYPE_CONTINUOUS:
											case V4L2_FRMSIZE_TYPE_STEPWISE:
												if (searchFrameEnum.stepwise.min.denominator != 0)
												{
													frameRate = searchFrameEnum.stepwise.min.denominator / searchFrameEnum.stepwise.min.numerator;
												}
												break;
										}

										if (frameRate > 0)
										{
											QString sFrame = formatFrame(frameRate);

											if (!properties.displayResolutions.contains(displayResolutions))
												properties.displayResolutions << displayResolutions;

											if (!properties.framerates.contains(sFrame))
												properties.framerates << sFrame;

											DevicePropertiesItem di;
											di.x = frmsizeenum.discrete.width;
											di.y = frmsizeenum.discrete.height;
											di.fps = frameRate;
											di.pf = identifyFormat(formatDesc.pixelformat);
											di.v4l2PixelFormat = formatDesc.pixelformat;
											di.input = input.index;

											if (di.pf == PixelFormat::NO_CHANGE)
												Debug(_log, "%s %d x %d @ %d fps %s (unsupported)", QSTRING_CSTR(properties.name), di.x, di.y, di.fps, QSTRING_CSTR(pixelFormat));
											else
												properties.valid.append(di);
										}
										searchFrameEnum.index++;
									}
								}
								break;
							}
							frmsizeenum.index++;
						}
						formatDesc.index++;
					}

					input.index++;
				}
			}

			if (close(fd) < 0)
			{
				Warning(_log, "Error closing '%s' device. Skipping...", QSTRING_CSTR(devName));
				continue;
			}

			QFile devNameFile(dev + "/name");
			QString realName = properties.name;

			if (devNameFile.exists())
			{
				devNameFile.open(QFile::ReadOnly);

				realName.replace("/dev/", "");
				realName = QString("%1 (%2)").arg(QString(devNameFile.readLine().trimmed()))
					.arg(realName);

				devNameFile.close();
			}

			// UTV007 workaround
			if (properties.valid.size() == 0 && realName.indexOf("usbtv ", 0, Qt::CaseInsensitive) == 0)
			{
				Warning(_log, "To have proper colors when using UTV007 grabber, you may need to add 'sudo systemctl stop hyperhdr@pi && v4l2-ctl -s pal-B && sudo systemctl start hyperhdr@pi' to /etc/rc.local or run it manually to set the PAL standard");
				for (int input = 0; input < properties.inputs.size(); input++)
				{
					{ DevicePropertiesItem diL; diL.x = 320; diL.y = 240; diL.fps = 30; diL.pf = identifyFormat(V4L2_PIX_FMT_YUYV); diL.v4l2PixelFormat = V4L2_PIX_FMT_YUYV; diL.input = input; properties.valid.append(diL); }
					{ DevicePropertiesItem diL; diL.x = 320; diL.y = 288; diL.fps = 25; diL.pf = identifyFormat(V4L2_PIX_FMT_YUYV); diL.v4l2PixelFormat = V4L2_PIX_FMT_YUYV; diL.input = input; properties.valid.append(diL); }
					{ DevicePropertiesItem diL; diL.x = 360; diL.y = 240; diL.fps = 30; diL.pf = identifyFormat(V4L2_PIX_FMT_YUYV); diL.v4l2PixelFormat = V4L2_PIX_FMT_YUYV; diL.input = input; properties.valid.append(diL); }
					{ DevicePropertiesItem diL; diL.x = 720; diL.y = 480; diL.fps = 30; diL.pf = identifyFormat(V4L2_PIX_FMT_YUYV); diL.v4l2PixelFormat = V4L2_PIX_FMT_YUYV; diL.input = input; properties.valid.append(diL); }
					{ DevicePropertiesItem diL; diL.x = 720; diL.y = 576; diL.fps = 25; diL.pf = identifyFormat(V4L2_PIX_FMT_YUYV); diL.v4l2PixelFormat = V4L2_PIX_FMT_YUYV; diL.input = input; properties.valid.append(diL); }
				}
			}

			_deviceProperties.insert(realName, properties);

			if (!silent)
			{
				for (int i = 0; i < properties.valid.count(); i++)
				{
					const auto& di = properties.valid[i];
					Info(_log, "%s [%s] %d x %d @ %d fps %s input %d", QSTRING_CSTR(realName), QSTRING_CSTR(properties.name), di.x, di.y, di.fps, QSTRING_CSTR(pixelFormatToString(di.pf)), di.input);
				}
			}
		}
	}
}

bool V4L2Grabber::start()
{
	try
	{
		resetCounter(InternalClock::now());
		_V4L2WorkerManager.Start();

		if (_V4L2WorkerManager.workersCount <= 1)
			Info(_log, "Multithreading for V4L2 is disabled. Available thread's count %d", _V4L2WorkerManager.workersCount);
		else
			Info(_log, "Multithreading for V4L2 is enabled. Available thread's count %d", _V4L2WorkerManager.workersCount);

		if (init() && _streamNotifier != nullptr && !_streamNotifier->isEnabled())
		{
			_streamNotifier->setEnabled(true);
			start_capturing();
			Info(_log, "Started");
			return true;
		}
	}
	catch (std::exception& e)
	{
		Error(_log, "Video capture fails to start (%s)", e.what());
	}

	return false;
}

void V4L2Grabber::stop()
{
	if (_streamNotifier != nullptr && _streamNotifier->isEnabled())
	{
		_V4L2WorkerManager.Stop();

		stop_capturing();
		_streamNotifier->setEnabled(false);
		uninit_device();
		close_device();
		_initialized = false;
		Info(_log, "Stopped");
	}
}

void V4L2Grabber::close_device()
{
	if (close(_fileDescriptor) == -1)
	{
		throw_errno_exception("Closing video device failed");
		return;
	}

	_fileDescriptor = -1;

	delete _streamNotifier;
	_streamNotifier = nullptr;
}

bool V4L2Grabber::init_mmap()
{
	struct v4l2_requestbuffers req;

	CLEAR(req);

	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (xioctl(VIDIOC_REQBUFS, &req) == -1)
	{
		if (EINVAL == errno)
		{
			throw_exception("'" + _deviceName + "' does not support memory mapping");
			return false;
		}
		else
		{
			throw_errno_exception("VIDIOC_REQBUFS");
			return false;
		}
	}

	if (req.count < 2)
	{
		throw_exception("Insufficient buffer memory on " + _deviceName);
		return false;
	}

	_buffers.resize(req.count);

	for (size_t n_buffers = 0; n_buffers < req.count; ++n_buffers)
	{
		struct v4l2_buffer buf;

		CLEAR(buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = n_buffers;

		if (xioctl(VIDIOC_QUERYBUF, &buf) == -1)
		{
			throw_errno_exception("VIDIOC_QUERYBUF");
			return false;
		}

		_buffers[n_buffers].length = buf.length;
		_buffers[n_buffers].start = mmap(
			NULL,
			buf.length,
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			_fileDescriptor,
			buf.m.offset
		);

		if (MAP_FAILED == _buffers[n_buffers].start)
		{
			throw_errno_exception("mmap");
			return false;
		}
	}

	return true;
}


bool V4L2Grabber::getControl(int _fd, __u32 controlId, long& minVal, long& maxVal, long& defVal)
{
	struct v4l2_queryctrl queryCtrl;

	memset(&queryCtrl, 0, sizeof(queryCtrl));
	queryCtrl.id = controlId;

	if (-1 == xioctl(_fd, VIDIOC_QUERYCTRL, &queryCtrl))
	{
		return false;
	}
	else if (queryCtrl.flags & V4L2_CTRL_FLAG_DISABLED)
	{
		return false;
	}
	else
	{
		minVal = queryCtrl.minimum;
		maxVal = queryCtrl.maximum;
		defVal = queryCtrl.default_value;
	}

	return true;
}

bool V4L2Grabber::setControl(__u32 controlId, __s32 newValue)
{
	struct v4l2_ext_control ctrl[1];
	struct v4l2_ext_controls ctrls;
	int ret;

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl[0].id = controlId;

	memset(&ctrls, 0, sizeof(ctrls));
	ctrls.count = 1;
	ctrls.controls = ctrl;


	ctrl[0].value = newValue;
	ret = xioctl(VIDIOC_S_EXT_CTRLS, &ctrls);

	if (ret < 0)
		return false;
	else
		return true;
}

bool V4L2Grabber::init_device(QString selectedDeviceName, DevicePropertiesItem props)
{
	struct stat st;

	DeviceProperties actDevice = _deviceProperties[selectedDeviceName];

	if (-1 == stat(QSTRING_CSTR(actDevice.name), &st))
	{
		throw_errno_exception("Cannot identify '" + actDevice.name + "' as a device");
		return false;
	}

	if (!S_ISCHR(st.st_mode))
	{
		throw_exception("Selected '" + actDevice.name + "' is not a device");
		return false;
	}

	_fileDescriptor = open(QSTRING_CSTR(actDevice.name), O_RDWR | O_NONBLOCK, 0);

	if (_fileDescriptor == -1)
	{
		throw_errno_exception("Cannot open '" + actDevice.name + "' device");
		return false;
	}

	// create the notifier for when a new frame is available
	_streamNotifier = new QSocketNotifier(_fileDescriptor, QSocketNotifier::Read);
	_streamNotifier->setEnabled(false);
	connect(_streamNotifier, &QSocketNotifier::activated, this, &V4L2Grabber::read_frame);

	struct v4l2_capability cap;
	CLEAR(cap);

	if (xioctl(VIDIOC_QUERYCAP, &cap) == -1)
	{
		if (EINVAL == errno)
		{
			throw_exception("'" + actDevice.name + "' is no V4L2 device");
			return false;
		}
		else
		{
			throw_errno_exception("VIDIOC_QUERYCAP");
			return false;
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		throw_exception("'" + actDevice.name + "' is no video capture device");
		return false;
	}


	if (!(cap.capabilities & V4L2_CAP_STREAMING))
	{
		throw_exception("'" + actDevice.name + "' does not support streaming i/o");
		return false;
	}


	/* Select video input, video standard and tune here. */
	struct v4l2_cropcap cropcap;
	CLEAR(cropcap);

	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (xioctl(VIDIOC_CROPCAP, &cropcap) == 0)
	{
		struct v4l2_crop crop;
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect;

		if (xioctl(VIDIOC_S_CROP, &crop) == -1)
		{
			Debug(_log, "Hardware cropping is not supported: ignoring");
		}
	}
	else
	{
		Debug(_log, "Hardware cropping is not supported: ignoring");
	}

	// set input if needed and supported
	struct v4l2_input v4l2Input;
	CLEAR(v4l2Input);

	v4l2Input.index = props.input;

	if (props.input >= 0 && (xioctl(VIDIOC_ENUMINPUT, &v4l2Input) == 0))
	{
		int inputTemp = props.input;
		if (xioctl(VIDIOC_S_INPUT, &inputTemp) == -1)
			Error(_log, "Input settings are not supported.");
		else
			Info(_log, "Set device input to: %s", v4l2Input.name);
	}

	// get the current settings
	struct v4l2_format fmt;
	CLEAR(fmt);

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl(VIDIOC_G_FMT, &fmt))
	{
		throw_errno_exception("VIDIOC_G_FMT");
		return false;
	}

	// set the requested pixel format
	fmt.fmt.pix.pixelformat = props.v4l2PixelFormat;

	if (props.pf == PixelFormat::MJPEG)
		fmt.fmt.pix.field = V4L2_FIELD_ANY;


	// set custom resolution for width and height	
	fmt.fmt.pix.width = props.x;
	fmt.fmt.pix.height = props.y;


	// set the settings
	if (-1 == xioctl(VIDIOC_S_FMT, &fmt))
	{
		throw_errno_exception("VIDIOC_S_FMT");
		return false;
	}


	// display the used width and height
	Info(_log, "Set resolution to: %d x %d", props.x, props.y);

	// Trying to set frame rate
	struct v4l2_streamparm streamparms;
	CLEAR(streamparms);

	streamparms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	// Check that the driver knows about framerate get/set
	if (xioctl(VIDIOC_G_PARM, &streamparms) >= 0)
	{
		// Check if the device is able to accept a capture framerate set.
		if (streamparms.parm.capture.capability == V4L2_CAP_TIMEPERFRAME)
		{
			streamparms.parm.capture.timeperframe.numerator = 1;
			streamparms.parm.capture.timeperframe.denominator = props.fps;

			if (xioctl(VIDIOC_S_PARM, &streamparms) == -1)
				Error(_log, "Frame rate settings not supported.");
			else
				Info(_log, "Set framerate to %d FPS", streamparms.parm.capture.timeperframe.denominator);
		}
	}

	// set the line length
	_lineLength = fmt.fmt.pix.bytesperline;

	if (actDevice.brightness.enabled)
	{
		long selVal = (_brightness != 0) ? _brightness : actDevice.brightness.defVal;

		if (setControl(V4L2_CID_BRIGHTNESS, selVal))
			Info(_log, "Brightness set to: %i (%s)", selVal, (selVal == actDevice.brightness.defVal) ? "default" : "user");
		else
			Error(_log, "Could not set brightness to: %i", selVal);
	}

	if (actDevice.contrast.enabled)
	{
		long selVal = (_contrast != 0) ? _contrast : actDevice.contrast.defVal;

		if (setControl(V4L2_CID_CONTRAST, selVal))
			Info(_log, "Contrast set to: %i (%s)", selVal, (selVal == actDevice.contrast.defVal) ? "default" : "user");
		else
			Error(_log, "Could not set contrast to: %i", selVal);
	}

	if (actDevice.saturation.enabled)
	{
		long selVal = (_saturation != 0) ? _saturation : actDevice.saturation.defVal;

		if (setControl(V4L2_CID_SATURATION, selVal))
			Info(_log, "Saturation set to: %i (%s)", selVal, (selVal == actDevice.saturation.defVal) ? "default" : "user");
		else
			Error(_log, "Could not set saturation to: %i", selVal);
	}

	if (actDevice.hue.enabled)
	{
		long selVal = (_hue != 0) ? _hue : actDevice.hue.defVal;

		if (setControl(V4L2_CID_HUE, selVal))
			Info(_log, "Hue set to: %i (%s)", selVal, (selVal == actDevice.hue.defVal) ? "default" : "user");
		else
			Error(_log, "Could not set hue to: %i", selVal);
	}

	// check pixel format and frame size
	switch (fmt.fmt.pix.pixelformat)
	{
		case V4L2_PIX_FMT_YUYV:
		{
			loadLutFile(PixelFormat::YUYV);
			_actualVideoFormat = PixelFormat::YUYV;
			_frameByteSize = props.x * props.y * 2;
			Info(_log, "Video pixel format is set to: YUYV");
		}
		break;

		case V4L2_PIX_FMT_XRGB32:
		{
			loadLutFile(PixelFormat::RGB24);
			_actualVideoFormat = PixelFormat::XRGB;
			_frameByteSize = props.x * props.y * 4;
			Info(_log, "Video pixel format is set to: XRGB");
		}
		break;

		case V4L2_PIX_FMT_RGB24:
		{
			loadLutFile(PixelFormat::RGB24);
			_actualVideoFormat = PixelFormat::RGB24;
			_frameByteSize = props.x * props.y * 3;
			Info(_log, "Video pixel format is set to: RGB24");
		}
		break;

		case V4L2_PIX_FMT_YUV420:
		{
			loadLutFile(PixelFormat::YUYV);
			_actualVideoFormat = PixelFormat::I420;
			_frameByteSize = (props.x * props.y * 6) / 4;
			Info(_log, "Video pixel format is set to: I420");
		}
		break;

		case V4L2_PIX_FMT_NV12:
		{
			loadLutFile(PixelFormat::YUYV);
			_actualVideoFormat = PixelFormat::NV12;
			_frameByteSize = (props.x * props.y * 6) / 4;
			Info(_log, "Video pixel format is set to: NV12");
		}
		break;

		case V4L2_PIX_FMT_MJPEG:
		{
			loadLutFile(PixelFormat::YUYV);
			_actualVideoFormat = PixelFormat::MJPEG;
			Info(_log, "Video pixel format is set to: MJPEG");
		}
		break;

		default:
		{
			throw_exception("Only pixel formats MJPEG, YUYV, RGB24, XRGB, I420 and NV12 are supported");

			return false;
		}
	}

	_actualWidth = props.x;
	_actualHeight = props.y;
	_actualFPS = props.fps;
	_actualDeviceName = selectedDeviceName;

	return init_mmap();
}

void V4L2Grabber::uninit_device()
{
	for (size_t i = 0; i < _buffers.size(); i++)
		if (-1 == munmap(_buffers[i].start, _buffers[i].length))
		{
			throw_errno_exception("Error while releasing device: munmap");
			return;
		}

	_buffers.resize(0);
}

void V4L2Grabber::start_capturing()
{
	v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	for (size_t i = 0; i < _buffers.size(); ++i)
	{
		struct v4l2_buffer buf;

		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (-1 == xioctl(VIDIOC_QBUF, &buf))
		{
			throw_errno_exception("VIDIOC_QBUF failed");
			return;
		}
	}

	if (-1 == xioctl(VIDIOC_STREAMON, &type))
	{
		throw_errno_exception("VIDIOC_STREAMON failed");
		return;
	}
}

void V4L2Grabber::stop_capturing()
{
	enum v4l2_buf_type type;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ErrorIf((xioctl(VIDIOC_STREAMOFF, &type) == -1), _log, "VIDIOC_STREAMOFF  error code  %d, %s", errno, strerror(errno));
}

int V4L2Grabber::read_frame()
{
	bool rc = false;

	try
	{
		struct v4l2_buffer buf;

		CLEAR(buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;

		if (-1 == xioctl(VIDIOC_DQBUF, &buf))
		{
			switch (errno)
			{
				case EAGAIN:
					return 0;

				case EIO:
				default:
					{
						throw_errno_exception("VIDIOC_DQBUF error. Video stream is probably broken. Refreshing list of the devices.");
						stop();
						getV4L2devices();
					}
					return 0;
			}
		}

		assert(buf.index < _buffers.size());

		rc = process_image(&buf, _buffers[buf.index].start, buf.bytesused);

		if (!rc && -1 == xioctl(VIDIOC_QBUF, &buf))
		{
			throw_errno_exception("VIDIOC_QBUF error. Video stream is probably broken.");
			return 0;
		}
	}
	catch (std::exception& e)
	{
		emit SignalCapturingException(e.what());
		rc = false;
	}

	return rc ? 1 : 0;
}

bool V4L2Grabber::process_image(v4l2_buffer* buf, const void* frameImageBuffer, int size)
{
	bool		frameSend = false;
	uint64_t	processFrameIndex = _currentFrame++;

	// frame skipping
	if ((processFrameIndex % _fpsSoftwareDecimation != 0) && (_fpsSoftwareDecimation > 1))
		return frameSend;

	// We do want a new frame...
	if (size < _frameByteSize && _actualVideoFormat != PixelFormat::MJPEG)
	{
		Error(_log, "Frame too small: %d != %d", size, _frameByteSize);
	}
	else
	{
		if (_V4L2WorkerManager.isActive())
		{
			// stats
			int64_t now = InternalClock::now();
			int64_t diff = now - frameStat.frameBegin;
			int64_t prevToken = frameStat.token;

			if (frameStat.token <= 0 || diff < 0)
			{
				frameStat.token = PerformanceCounters::currentToken();
				resetCounter(now);
			}
			else if (prevToken != (frameStat.token = PerformanceCounters::currentToken()))
			{
				int total = (frameStat.badFrame + frameStat.goodFrame);
				int av = (frameStat.goodFrame > 0) ? frameStat.averageFrame / frameStat.goodFrame : 0;
				QString access = (frameStat.directAccess) ? " (direct)" : "";
				if (diff >= 59000 && diff <= 65000)
					emit GlobalSignals::getInstance()->SignalPerformanceNewReport(
					PerformanceReport(hyperhdr::PerformanceReportType::VIDEO_GRABBER, frameStat.token, this->_actualDeviceName + access, total / qMax(diff / 1000.0, 1.0), av, frameStat.goodFrame, frameStat.badFrame));

				resetCounter(now);

				QString currentCache = QString::fromStdString(Image<ColorRgb>::adjustCache());

				if (!currentCache.isEmpty())
					Info(_log, "%s", QSTRING_CSTR(currentCache));
			}

			if (_V4L2WorkerManager.workers == nullptr)
			{
				_V4L2WorkerManager.InitWorkers();
				Debug(_log, "Worker's thread count  = %d", _V4L2WorkerManager.workersCount);

				for (unsigned int i = 0; i < _V4L2WorkerManager.workersCount && _V4L2WorkerManager.workers != nullptr; i++)
				{
					V4L2Worker* _workerThread = _V4L2WorkerManager.workers[i];
					connect(_workerThread, &V4L2Worker::SignalNewFrameError, this, &V4L2Grabber::newWorkerFrameErrorHandler);
					connect(_workerThread, &V4L2Worker::SignalNewFrame, this, &V4L2Grabber::newWorkerFrameHandler);
				}
			}

			frameStat.segment |= (1 << buf->index);

			for (unsigned int i = 0; _V4L2WorkerManager.isActive() && i < _V4L2WorkerManager.workersCount && _V4L2WorkerManager.workers != nullptr; i++)
			{
				if (_V4L2WorkerManager.workers[i]->isFinished() || !_V4L2WorkerManager.workers[i]->isRunning())
				{
					if (_V4L2WorkerManager.workers[i]->isBusy() == false)
					{
						V4L2Worker* _workerThread = _V4L2WorkerManager.workers[i];

						if ((_actualVideoFormat == PixelFormat::YUYV || _actualVideoFormat == PixelFormat::I420 ||
							_actualVideoFormat == PixelFormat::NV12) && !_lutBufferInit)
						{
							loadLutFile();
						}

						bool directAccess = !(_signalAutoDetectionEnabled || _signalDetectionEnabled || isCalibrating() );
						_workerThread->setup(
							i,
							buf,
							_actualVideoFormat,
							(uint8_t*)frameImageBuffer, size, _actualWidth, _actualHeight, _lineLength,
							_cropLeft, _cropTop, _cropBottom, _cropRight,
							processFrameIndex, InternalClock::nowPrecise(), _hdrToneMappingEnabled,
							(_lutBufferInit) ? _lut.data() : nullptr, _qframe, directAccess, _deviceName);

						if (_V4L2WorkerManager.workersCount > 1)
							_V4L2WorkerManager.workers[i]->start();
						else
							_V4L2WorkerManager.workers[i]->startOnThisThread();

						frameSend = true;
						break;
					}
				}
			}
		}
	}

	return frameSend;
}

void V4L2Grabber::newWorkerFrameErrorHandler(unsigned int workerIndex, QString error, quint64 sourceCount)
{
	frameStat.badFrame++;
	if (error.indexOf(QString(UNSUPPORTED_DECODER)) == 0)
	{
		Error(_log, "Unsupported MJPEG/YUV format. Please contact HyperHDR developers! (info: %s)", QSTRING_CSTR(error));
	}
	//Debug(_log, "Error occured while decoding mjpeg frame %d = %s", sourceCount, QSTRING_CSTR(error));	

	// get next frame
	if (workerIndex > _V4L2WorkerManager.workersCount)
		Error(_log, "Frame index = %d, index out of range", sourceCount);

	else if (_V4L2WorkerManager.workers == nullptr ||
		_V4L2WorkerManager.isActive() == false ||
		xioctl(VIDIOC_QBUF, _V4L2WorkerManager.workers[workerIndex]->GetV4L2Buffer()))
	{
		Error(_log, "Frame index = %d, inactive or critical VIDIOC_QBUF error in v4l2 driver. Buf index = %d, worker = %d, is_active = %d.",
			sourceCount, _V4L2WorkerManager.workers[workerIndex]->GetV4L2Buffer()->index, workerIndex, _V4L2WorkerManager.isActive());
	}

	if (workerIndex <= _V4L2WorkerManager.workersCount)
		_V4L2WorkerManager.workers[workerIndex]->noBusy();
}


void V4L2Grabber::newWorkerFrameHandler(unsigned int workerIndex, Image<ColorRgb> image, quint64 sourceCount, qint64 _frameBegin)
{
	handleNewFrame(workerIndex, image, sourceCount, _frameBegin);

	// get next frame
	if (workerIndex > _V4L2WorkerManager.workersCount)
		Error(_log, "Frame index = %d, index out of range", sourceCount);

	else if (_V4L2WorkerManager.workers == nullptr ||
		_V4L2WorkerManager.isActive() == false ||
		xioctl(VIDIOC_QBUF, _V4L2WorkerManager.workers[workerIndex]->GetV4L2Buffer()))
	{
		Error(_log, "Frame index = %d, inactive or critical VIDIOC_QBUF error in v4l2 driver. Buf index = %d, worker = %d, is_active = %d.",
			sourceCount, _V4L2WorkerManager.workers[workerIndex]->GetV4L2Buffer()->index, workerIndex, _V4L2WorkerManager.isActive());
	}

	if (workerIndex <= _V4L2WorkerManager.workersCount)
		_V4L2WorkerManager.workers[workerIndex]->noBusy();
}

int V4L2Grabber::xioctl(int request, void* arg)
{
	int r;

	do
	{
		r = ioctl(_fileDescriptor, request, arg);
	} while (-1 == r && EINTR == errno);

	return r;
}

int V4L2Grabber::xioctl(int fileDescriptor, int request, void* arg)
{
	int r;

	do
	{
		r = ioctl(fileDescriptor, request, arg);
	} while (r < 0 && errno == EINTR);

	return r;
}

void V4L2Grabber::throw_exception(const QString& error)
{
	Error(_log, "Throws error: %s", QSTRING_CSTR(error));
}

void V4L2Grabber::throw_errno_exception(const QString& error)
{
	Error(_log, "Throws error nr: %s", QSTRING_CSTR(QString(error + " error code " + QString::number(errno) + ", " + strerror(errno))));
}


