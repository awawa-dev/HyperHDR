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

#include <hyperion/Hyperion.h>
#include <hyperion/HyperionIManager.h>

#include <QDirIterator>
#include <QFileInfo>

#include "grabber/V4L2Grabber.h"
#include "utils/ColorSys.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))

#ifndef V4L2_CAP_META_CAPTURE
#define V4L2_CAP_META_CAPTURE 0x00800000 // Specified in kernel header v4.16. Required for backward compatibility.
#endif

// some stuff for HDR tone mapping
#define LUT_FILE_SIZE 50331648

V4L2Grabber::V4L2Grabber(const QString & device
		, unsigned width
		, unsigned height
		, unsigned fps
		, unsigned input
		, VideoStandard videoStandard
		, PixelFormat pixelFormat
		, int pixelDecimation
		, const QString & configurationPath
		)
	: Grabber("V4L2:"+device)
	, _deviceName()
	, _videoStandard(videoStandard)
	, _ioMethod(IO_METHOD_MMAP)
	, _fileDescriptor(-1)
	, _buffers()
	, _pixelFormat(pixelFormat)
	, _pixelDecimation(-1)
	, _lineLength(-1)
	, _frameByteSize(-1)
	, _noSignalCounterThreshold(40)
	, _noSignalThresholdColor(ColorRgb{0,0,0})
	, _signalDetectionEnabled(true)
	, _cecDetectionEnabled(true)
	, _cecStandbyActivated(false)
	, _noSignalDetected(false)
	, _noSignalCounter(0)
	, _x_frac_min(0.25)
	, _y_frac_min(0.25)
	, _x_frac_max(0.75)
	, _y_frac_max(0.75)
	, _streamNotifier(nullptr)
	, _initialized(false)
	, _deviceAutoDiscoverEnabled(false)
	,_hdrToneMappingEnabled(0)	
	, _fpsSoftwareDecimation(1)
	, lutBuffer(NULL)
	, _lutBufferInit(false)
	, _currentFrame(0)
	, _configurationPath(configurationPath)
	
{
	setPixelDecimation(pixelDecimation);
	getV4Ldevices();

	// init
	setInput(input);
	setWidthHeight(width, height);
	setFramerate(fps);
	setDeviceVideoStandard(device, videoStandard);	
}

void V4L2Grabber::loadLutFile(const QString & color)
{	
	// load lut
	QString fileName3d = QString("%1%2%3%4").arg(_configurationPath,"/lut_",color,".3d");
	
	_lutBufferInit = false;
	
	if (_hdrToneMappingEnabled)
	{
		if (FILE *file = fopen(QSTRING_CSTR(fileName3d), "r")) {
			size_t length;
			Debug(_log,"LUT file found: %s", QSTRING_CSTR(fileName3d));
			
			fseek(file, 0, SEEK_END);
			length = ftell(file);
			fseek(file, 0, SEEK_SET);
			if (length ==  LUT_FILE_SIZE) {
				if (lutBuffer==NULL)
					lutBuffer = (unsigned char *)malloc(length + 1);
				if(fread(lutBuffer, 1, length, file)!=length)
				{					
					Error(_log,"Error reading LUT file %s", QSTRING_CSTR(fileName3d));
				}
				else
				{
					_lutBufferInit = true;
					Debug(_log,"LUT file has been loaded");					
				}
			}
			else
				Error(_log,"LUT file has invalid length: %i %s", length, QSTRING_CSTR(fileName3d));
			fclose(file);
		}
		else
			Error(_log,"LUT file NOT found: %s", QSTRING_CSTR(fileName3d));
	}					
}

void V4L2Grabber::setFpsSoftwareDecimation(int decimation)
{
	_fpsSoftwareDecimation = decimation;
	Debug(_log,"setFpsSoftwareDecimation to: %i", decimation);
}

void V4L2Grabber::setHdrToneMappingEnabled(int mode)
{
	_hdrToneMappingEnabled = mode;
	if (lutBuffer!=NULL || !mode)
		Debug(_log,"setHdrToneMappingMode to: %s", (mode == 0) ? "Disabled" : ((mode == 1)? "Fullscreen": "Border mode") );
	else
		Warning(_log,"setHdrToneMappingMode to: enable, but the LUT file is currently unloaded");	
		
	if (_V4L2WorkerManager.isActive())	
	{
		Debug(_log,"setHdrToneMappingMode replacing LUT");
		_V4L2WorkerManager.Stop();
		if ((_pixelFormat == PixelFormat::UYVY) || (_pixelFormat == PixelFormat::YUYV))
			loadLutFile("yuv");
		else
			loadLutFile("rgb");				
		_V4L2WorkerManager.Start();
	}
}

V4L2Grabber::~V4L2Grabber()
{
	if (lutBuffer!=NULL)
		free(lutBuffer);
	lutBuffer = NULL;

	uninit();
}

void V4L2Grabber::uninit()
{
	// stop if the grabber was not stopped
	if (_initialized)
	{
		Debug(_log,"uninit grabber: %s", QSTRING_CSTR(_deviceName));
		stop();
	}
}

bool V4L2Grabber::init()
{
	if (!_initialized)
	{
		getV4Ldevices();
		QString v4lDevices_str;

		// show list only once
		if (!_deviceName.startsWith("/dev/"))
		{
			for (auto& dev: _v4lDevices)
			{
				v4lDevices_str += "\t"+ dev.first + "\t" + dev.second + "\n";
			}
			if (!v4lDevices_str.isEmpty())
				Info(_log, "available V4L2 devices:\n%s", QSTRING_CSTR(v4lDevices_str));
		}

		if (_deviceName == "auto")
		{
			_deviceAutoDiscoverEnabled = true;
			_deviceName = "unknown";
			Info( _log, "search for usable video devices" );
			for (auto& dev: _v4lDevices)
			{
				_deviceName = dev.first;
				if (init())
				{
					Info(_log, "found usable v4l2 device: %s (%s)",QSTRING_CSTR(dev.first), QSTRING_CSTR(dev.second));
					_deviceAutoDiscoverEnabled = false;
					return _initialized;
				}
			}
			Info(_log, "no usable device found");
		}
		else if (!_deviceName.startsWith("/dev/"))
		{
			for (auto& dev: _v4lDevices)
			{
				if (_deviceName.toLower() == dev.second.toLower())
				{
					_deviceName = dev.first;
					Info(_log, "found v4l2 device with configured name: %s (%s)", QSTRING_CSTR(dev.second), QSTRING_CSTR(dev.first) );
					break;
				}
			}
		}
		else
		{
			Info(_log, "%s v4l device: %s", (_deviceAutoDiscoverEnabled? "test" : "configured"), QSTRING_CSTR(_deviceName));
		}

		bool opened = false;
		try
		{
			// do not init with unknown device
			if (_deviceName != "unknown")
			{
				if (open_device())
				{
					opened = true;
					init_device(_videoStandard);
					_initialized = true;
				}
			}
		}
		catch(std::exception& e)
		{
			if (opened)
			{
				uninit_device();
				close_device();
			}
			ErrorIf( !_deviceAutoDiscoverEnabled, _log, "V4l2 init failed (%s)", e.what());
		}
	}

	return _initialized;
}

void V4L2Grabber::getV4Ldevices()
{
	QDirIterator it("/sys/class/video4linux/", QDirIterator::NoIteratorFlags);
	_deviceProperties.clear();
	while(it.hasNext())
	{
		//_v4lDevices
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

			if (cap.device_caps & V4L2_CAP_META_CAPTURE) // this device has bit 23 set (and bit 1 reset), so it doesn't have capture.
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

			V4L2Grabber::DeviceProperties properties;

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
					input.index++;
				}
			}

			// collect available device resolutions & frame rates
			struct v4l2_frmsizeenum frmsizeenum;
			CLEAR(frmsizeenum);

			frmsizeenum.index = 0;
			frmsizeenum.pixel_format = fmt.fmt.pix.pixelformat;
			while (xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsizeenum) >= 0)
			{
				switch (frmsizeenum.type)
				{
					case V4L2_FRMSIZE_TYPE_DISCRETE:
					{
						properties.resolutions << QString::number(frmsizeenum.discrete.width) + "x" + QString::number(frmsizeenum.discrete.height);
						enumFrameIntervals(properties.framerates, fd, fmt.fmt.pix.pixelformat, frmsizeenum.discrete.width, frmsizeenum.discrete.height);
					}
					break;
					case V4L2_FRMSIZE_TYPE_CONTINUOUS:
					case V4L2_FRMSIZE_TYPE_STEPWISE:
						// We do not take care of V4L2_FRMSIZE_TYPE_CONTINUOUS or V4L2_FRMSIZE_TYPE_STEPWISE
						break;
				}
				frmsizeenum.index++;
			}

			if (close(fd) < 0) continue;

			QFile devNameFile(dev+"/name");
			if (devNameFile.exists())
			{
				devNameFile.open(QFile::ReadOnly);
				devName = devNameFile.readLine();
				devName = devName.trimmed();
				properties.name = devName;
				devNameFile.close();
			}
			_v4lDevices.emplace("/dev/"+it.fileName(), devName);
			_deviceProperties.insert("/dev/"+it.fileName(), properties);
		}
    }
}

void V4L2Grabber::setSignalThreshold(double redSignalThreshold, double greenSignalThreshold, double blueSignalThreshold, int noSignalCounterThreshold)
{
	_noSignalThresholdColor.red   = uint8_t(255*redSignalThreshold);
	_noSignalThresholdColor.green = uint8_t(255*greenSignalThreshold);
	_noSignalThresholdColor.blue  = uint8_t(255*blueSignalThreshold);
	_noSignalCounterThreshold     = qMax(1, noSignalCounterThreshold);

	Info(_log, "Signal threshold set to: {%d, %d, %d}", _noSignalThresholdColor.red, _noSignalThresholdColor.green, _noSignalThresholdColor.blue );
}

void V4L2Grabber::setSignalDetectionOffset(double horizontalMin, double verticalMin, double horizontalMax, double verticalMax)
{
	// rainbow 16 stripes 0.47 0.2 0.49 0.8
	// unicolor: 0.25 0.25 0.75 0.75

	_x_frac_min = horizontalMin;
	_y_frac_min = verticalMin;
	_x_frac_max = horizontalMax;
	_y_frac_max = verticalMax;

	Info(_log, "Signal detection area set to: %f,%f x %f,%f", _x_frac_min, _y_frac_min, _x_frac_max, _y_frac_max );
}

void	V4L2Grabber::ResetCounter(uint64_t from)
{
	frameStat.frameBegin = from;
	frameStat.averageFrame = 0;
	frameStat.badFrame = 0;
	frameStat.goodFrame = 0;
	frameStat.segment = 0;
}

bool V4L2Grabber::start()
{
	try
	{
		ResetCounter(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
		_V4L2WorkerManager.Start();
		
		#ifdef HAVE_JPEG_DECODER
		if (_V4L2WorkerManager.workersCount<=1)
			Info(_log, "Multithreading for V4L2 is disabled. Available thread's count %d",_V4L2WorkerManager.workersCount );
		else
			Info(_log, "Multithreading for V4L2 is enabled. Available thread's count %d",_V4L2WorkerManager.workersCount );
		#endif
		
		if (init() && _streamNotifier != nullptr && !_streamNotifier->isEnabled())
		{
			_streamNotifier->setEnabled(true);
			start_capturing();
			Info(_log, "Started");
			return true;
		}
	}
	catch(std::exception& e)
	{
		Error(_log, "start failed (%s)", e.what());
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
		_deviceProperties.clear();
		Info(_log, "Stopped");
	}
}

bool V4L2Grabber::open_device()
{
	struct stat st;

	if (-1 == stat(QSTRING_CSTR(_deviceName), &st))
	{
		throw_errno_exception("Cannot identify '" + _deviceName + "'");
		return false;
	}

	if (!S_ISCHR(st.st_mode))
	{
		throw_exception("'" + _deviceName + "' is no device");
		return false;
	}

	_fileDescriptor = open(QSTRING_CSTR(_deviceName), O_RDWR | O_NONBLOCK, 0);

	if (-1 == _fileDescriptor)
	{
		throw_errno_exception("Cannot open '" + _deviceName + "'");
		return false;
	}

	// create the notifier for when a new frame is available
	_streamNotifier = new QSocketNotifier(_fileDescriptor, QSocketNotifier::Read);
	_streamNotifier->setEnabled(false);
	connect(_streamNotifier, &QSocketNotifier::activated, this, &V4L2Grabber::read_frame);
	return true;
}

void V4L2Grabber::close_device()
{
	if (-1 == close(_fileDescriptor))
	{
		throw_errno_exception("close");
		return;
	}

	_fileDescriptor = -1;

	delete _streamNotifier;
	_streamNotifier = nullptr;
}

void V4L2Grabber::init_mmap()
{
	struct v4l2_requestbuffers req;

	CLEAR(req);

	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(VIDIOC_REQBUFS, &req))
	{
		if (EINVAL == errno)
		{
			throw_exception("'" + _deviceName + "' does not support memory mapping");
			return;
		}
		else
		{
			throw_errno_exception("VIDIOC_REQBUFS");
			return;
		}
	}

	if (req.count < 2)
	{
		throw_exception("Insufficient buffer memory on " + _deviceName);
		return;
	}

	_buffers.resize(req.count);

	for (size_t n_buffers = 0; n_buffers < req.count; ++n_buffers)
	{
		struct v4l2_buffer buf;

		CLEAR(buf);

		buf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory	= V4L2_MEMORY_MMAP;
		buf.index	= n_buffers;

		if (-1 == xioctl(VIDIOC_QUERYBUF, &buf))
		{
			throw_errno_exception("VIDIOC_QUERYBUF");
			return;
		}

		_buffers[n_buffers].length = buf.length;
		_buffers[n_buffers].start = mmap(NULL /* start anywhere */,
						buf.length,
						PROT_READ | PROT_WRITE /* required */,
						MAP_SHARED /* recommended */,
						_fileDescriptor, buf.m.offset
					);

		if (MAP_FAILED == _buffers[n_buffers].start)
		{
			throw_errno_exception("mmap");
			return;
		}
	}
}

void V4L2Grabber::init_device(VideoStandard videoStandard)
{
	struct v4l2_capability cap;
	CLEAR(cap);

	if (-1 == xioctl(VIDIOC_QUERYCAP, &cap))
	{
		if (EINVAL == errno)
		{
			throw_exception("'" + _deviceName + "' is no V4L2 device");
			return;
		}
		else
		{
			throw_errno_exception("VIDIOC_QUERYCAP");
			return;
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		throw_exception("'" + _deviceName + "' is no video capture device");
		return;
	}

	switch (_ioMethod)
	{		
		case IO_METHOD_MMAP:		
		{
			if (!(cap.capabilities & V4L2_CAP_STREAMING))
			{
				throw_exception("'" + _deviceName + "' does not support streaming i/o");
				return;
			}
		}
		break;
	}


	/* Select video input, video standard and tune here. */

	struct v4l2_cropcap cropcap;
	CLEAR(cropcap);

	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (0 == xioctl(VIDIOC_CROPCAP, &cropcap))
	{
		struct v4l2_crop crop;
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; /* reset to default */

		if (-1 == xioctl(VIDIOC_S_CROP, &crop))
		{
			switch (errno)
			{
				case EINVAL: /* Cropping not supported. */
				default: /* Errors ignored. */
					break;
			}
		}
	}
	else
	{
		/* Errors ignored. */
	}

	// set input if needed and supported
	struct v4l2_input v4l2Input;
	CLEAR(v4l2Input);
	v4l2Input.index = _input;

	if (_input >= 0 && 0 == xioctl(VIDIOC_ENUMINPUT, &v4l2Input))
	{
		(-1 == xioctl(VIDIOC_S_INPUT, &_input))
		?	Debug(_log, "Input settings not supported.")
		:	Debug(_log, "Set device input to: %s", v4l2Input.name);
	}

	// set the video standard if needed and supported
	struct v4l2_standard standard;
	CLEAR(standard);

	if (-1 != xioctl(VIDIOC_ENUMSTD, &standard))
	{
		switch (videoStandard)
		{
			case VideoStandard::PAL:
			{
				standard.id = V4L2_STD_PAL;
				if (-1 == xioctl(VIDIOC_S_STD, &standard.id))
				{
					throw_errno_exception("VIDIOC_S_STD");
					break;
				}
				Debug(_log, "Video standard=PAL");
			}
			break;

			case VideoStandard::NTSC:
			{
				standard.id = V4L2_STD_NTSC;
				if (-1 == xioctl(VIDIOC_S_STD, &standard.id))
				{
					throw_errno_exception("VIDIOC_S_STD");
					break;
				}
				Debug(_log, "Video standard=NTSC");
			}
			break;

			case VideoStandard::SECAM:
			{
				standard.id = V4L2_STD_SECAM;
				if (-1 == xioctl(VIDIOC_S_STD, &standard.id))
				{
					throw_errno_exception("VIDIOC_S_STD");
					break;
				}
				Debug(_log, "Video standard=SECAM");
			}
			break;

			case VideoStandard::NO_CHANGE:
			default:
				// No change to device settings
				break;
		}
	}

	// get the current settings
	struct v4l2_format fmt;
	CLEAR(fmt);

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl(VIDIOC_G_FMT, &fmt))
	{
		throw_errno_exception("VIDIOC_G_FMT");
		return;
	}

	// set the requested pixel format
	switch (_pixelFormat)
	{
		case PixelFormat::UYVY:
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
		break;

		case PixelFormat::YUYV:
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
		break;

		case PixelFormat::RGB32:
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB32;
		break;

#ifdef HAVE_JPEG_DECODER
		case PixelFormat::MJPEG:
		{
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
			fmt.fmt.pix.field       = V4L2_FIELD_ANY;
		}
		break;
#endif

		case PixelFormat::NO_CHANGE:
		default:
			// No change to device settings
			break;
	}

	// set custom resolution for width and height if they are not zero
	if(_width && _height)
	{
		fmt.fmt.pix.width = _width;
		fmt.fmt.pix.height = _height;
	}

	// set the settings
	if (-1 == xioctl(VIDIOC_S_FMT, &fmt))
	{
		throw_errno_exception("VIDIOC_S_FMT");
		return;
	}

	// initialize current width and height
	_width = fmt.fmt.pix.width;
	_height = fmt.fmt.pix.height;

	// display the used width and height
	Debug(_log, "Set resolution to width=%d height=%d", _width, _height );

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
			streamparms.parm.capture.timeperframe.denominator = _fps;
			(-1 == xioctl(VIDIOC_S_PARM, &streamparms))
			?	Debug(_log, "Frame rate settings not supported.")
			:	Debug(_log, "Set framerate to %d fps", streamparms.parm.capture.timeperframe.denominator);
		}
	}

	// set the line length
	_lineLength = fmt.fmt.pix.bytesperline;

	// check pixel format and frame size
	switch (fmt.fmt.pix.pixelformat)
	{
		case V4L2_PIX_FMT_UYVY:
		{
			loadLutFile("yuv");
			_pixelFormat = PixelFormat::UYVY;
			_frameByteSize = _width * _height * 2;
			Debug(_log, "Pixel format=UYVY");
		}
		break;

		case V4L2_PIX_FMT_YUYV:
		{
			loadLutFile("yuv");		
			_pixelFormat = PixelFormat::YUYV;
			_frameByteSize = _width * _height * 2;
			Debug(_log, "Pixel format=YUYV");
		}
		break;

		case V4L2_PIX_FMT_RGB32:
		{
			loadLutFile("rgb");				
			_pixelFormat = PixelFormat::RGB32;
			_frameByteSize = _width * _height * 4;
			Debug(_log, "Pixel format=RGB32");
		}
		break;

#ifdef HAVE_JPEG_DECODER
		case V4L2_PIX_FMT_MJPEG:
		{
			loadLutFile("rgb");						
			_pixelFormat = PixelFormat::MJPEG;
			Debug(_log, "Pixel format=MJPEG");
		}
		break;
#endif

		default:
#ifdef HAVE_JPEG_DECODER
			throw_exception("Only pixel formats UYVY, YUYV, RGB32 and MJPEG are supported");
#else
			throw_exception("Only pixel formats UYVY, YUYV, and RGB32 are supported");
#endif
		return;
	}

	switch (_ioMethod)
	{		
		case IO_METHOD_MMAP:
			init_mmap();
		break;
	}
}

void V4L2Grabber::uninit_device()
{
	switch (_ioMethod)
	{		
		case IO_METHOD_MMAP:
		{
			for (size_t i = 0; i < _buffers.size(); ++i)
				if (-1 == munmap(_buffers[i].start, _buffers[i].length))
				{
					throw_errno_exception("munmap");
					return;
				}
		}
		break;		
	}

	_buffers.resize(0);
}

void V4L2Grabber::start_capturing()
{
	switch (_ioMethod)
	{		
		case IO_METHOD_MMAP:
		{
			for (size_t i = 0; i < _buffers.size(); ++i)
			{
				struct v4l2_buffer buf;

				CLEAR(buf);
				buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory = V4L2_MEMORY_MMAP;
				buf.index = i;

				if (-1 == xioctl(VIDIOC_QBUF, &buf))
				{
					throw_errno_exception("VIDIOC_QBUF");
					return;
				}
			}
			v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			if (-1 == xioctl(VIDIOC_STREAMON, &type))
			{
				throw_errno_exception("VIDIOC_STREAMON");
				return;
			}
			break;
		}		
	}
}

void V4L2Grabber::stop_capturing()
{
	enum v4l2_buf_type type;

	switch (_ioMethod)
	{		
		case IO_METHOD_MMAP:		
		{
			type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			ErrorIf((xioctl(VIDIOC_STREAMOFF, &type) == -1), _log, "VIDIOC_STREAMOFF  error code  %d, %s", errno, strerror(errno));
		}
		break;
	}
}

int V4L2Grabber::read_frame()
{
	bool rc = false;

	try
	{
		struct v4l2_buffer buf;

		switch (_ioMethod)
		{			
			case IO_METHOD_MMAP:
			{
				CLEAR(buf);

				buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory = V4L2_MEMORY_MMAP;

				if (-1 == xioctl(VIDIOC_DQBUF, &buf))
				{
					switch (errno)
					{
						case EAGAIN:
							return 0;

						case EIO: /* Could ignore EIO, see spec. */
						default:
						{
							throw_errno_exception("VIDIOC_DQBUF");
							stop();
							getV4Ldevices();
						}
						return 0;
					}
				}

				assert(buf.index < _buffers.size());

				rc = process_image(&buf, _buffers[buf.index].start, buf.bytesused);
				// if error get next frame, other wise leave it to signals
				if (!rc && -1 == xioctl(VIDIOC_QBUF, &buf))
				{
					throw_errno_exception("VIDIOC_QBUF");
					return 0;
				}
			}
			break;			
		}
	}
	catch (std::exception& e)
	{
		emit readError(e.what());
		rc = false;
	}

	return rc ? 1 : 0;
}

bool V4L2Grabber::process_image(v4l2_buffer* buf, const void *frameImageBuffer, int size)
{	
	bool frameSend = false;
	unsigned int processFrameIndex = _currentFrame++;
	
	// frame skipping
	if ( (processFrameIndex % _fpsSoftwareDecimation != 0) && (_fpsSoftwareDecimation > 1))
		return frameSend;
		
	// CEC detection
	if (_cecDetectionEnabled && _cecStandbyActivated)
		return frameSend;		

	// We do want a new frame...
#ifdef HAVE_JPEG_DECODER
	if (size < _frameByteSize && _pixelFormat != PixelFormat::MJPEG)
#else
	if (size < _frameByteSize)
#endif
	{
		Error(_log, "Frame too small: %d != %d", size, _frameByteSize);
	}
	else
	{
		if (_V4L2WorkerManager.isActive())
		{		
			// benchmark
			uint64_t currentTime=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			long	 diff = currentTime - frameStat.frameBegin;
			if ( diff >=1000*60)
			{
				int total = (frameStat.badFrame+frameStat.goodFrame);
				int av = (frameStat.goodFrame>0)?frameStat.averageFrame/frameStat.goodFrame:0;
				Debug(_log, "Video FPS: %.2f, av. delay: %dms, good: %d, bad: %d (%.2f,%d)", 
					total/60.0,
					av,
					frameStat.goodFrame,
					frameStat.badFrame,
					diff/1000.0,
					frameStat.segment);

				ResetCounter(currentTime);				
			}
			
			if (_V4L2WorkerManager.workers == nullptr)
			{	
				_V4L2WorkerManager.InitWorkers();
				Debug(_log, "Worker's thread count  = %d", _V4L2WorkerManager.workersCount);	
				
				for (unsigned int i=0; i < _V4L2WorkerManager.workersCount && _V4L2WorkerManager.workers != nullptr; i++)
				{
					V4L2Worker* _workerThread=_V4L2WorkerManager.workers[i];					
					connect(_workerThread, SIGNAL(newFrameError(unsigned int, QString,unsigned int)), this , SLOT(newWorkerFrameError(unsigned int, QString,unsigned int)));
					connect(_workerThread, SIGNAL(newFrame(unsigned int, Image<ColorRgb>,unsigned int,quint64)), this , SLOT(newWorkerFrame(unsigned int, Image<ColorRgb>, unsigned int,quint64)));
				}
		    	}	 
		    	
		    	frameStat.segment|=(1<<buf->index);
		    	
			for (unsigned int i=0;_V4L2WorkerManager.isActive() && 
						i < _V4L2WorkerManager.workersCount && 
						_V4L2WorkerManager.workers != nullptr; i++)
			{													
				if ((_V4L2WorkerManager.workers[i]->isFinished() || !_V4L2WorkerManager.workers[i]->isRunning()))
					// aquire lock
					if ( _V4L2WorkerManager.workers[i]->isBusy() == false)
					{		
						V4L2Worker* _workerThread = _V4L2WorkerManager.workers[i];	
						
						if ((_pixelFormat==PixelFormat::UYVY || _pixelFormat==PixelFormat::YUYV)  && !_lutBufferInit)
						{														
							if (lutBuffer == NULL)
								lutBuffer = (unsigned char *)malloc(LUT_FILE_SIZE + 1);
								
							for (int y = 0; y<256; y++)
								for (int u= 0; u<256; u++)
									for (int v = 0; v<256; v++)
									{
										uint32_t ind_lutd = (LUTD_R_STRIDE(y) + LUTD_G_STRIDE(u) + LUTD_B_STRIDE(v));
										ColorSys::yuv2rgb(y, u, v, 
											lutBuffer[ind_lutd + LUTD_C_STRIDE(0)], 
											lutBuffer[ind_lutd + LUTD_C_STRIDE(1)], 
											lutBuffer[ind_lutd + LUTD_C_STRIDE(2)]);
									}				
									
							_lutBufferInit = true;
										
							Debug(_log,"Internal LUT table for YUV conversion created");
						}			
						
						_workerThread->setup(
							i, 
							buf,
							_videoMode,
							_pixelFormat,
							(uint8_t *)frameImageBuffer, size, _width, _height, _lineLength,
				#ifdef HAVE_TURBO_JPEG
							_subsamp, 
				#else
							0,
				#endif			 
							_pixelDecimation,  _cropLeft,  _cropTop, _cropBottom, _cropRight, 		
							processFrameIndex,currentTime,_hdrToneMappingEnabled,
							(_lutBufferInit)? lutBuffer: NULL);							
									
						if (_V4L2WorkerManager.workersCount>1)					
							_V4L2WorkerManager.workers[i]->start();
						else
							_V4L2WorkerManager.workers[i]->startOnThisThread();
						//Debug(_log, "Frame index = %d => send to decode to the thread = %i", processFrameIndex,i);			
						frameSend = true;
						break;		
					}
				}						
		}		
	}

	return frameSend;
}

void V4L2Grabber::newWorkerFrameError(unsigned int workerIndex, QString error, unsigned int sourceCount)
{
	frameStat.badFrame++;
	//Debug(_log, "Error occured while decoding mjpeg frame %d = %s", sourceCount, QSTRING_CSTR(error));	
	
	// get next frame
	if (workerIndex >_V4L2WorkerManager.workersCount)
		Error(_log, "Frame index = %d, index out of range", sourceCount);	
		
	else if (_V4L2WorkerManager.workers == nullptr ||
		 _V4L2WorkerManager.isActive() == false ||
		 xioctl(VIDIOC_QBUF, _V4L2WorkerManager.workers[workerIndex]->GetV4L2Buffer()))
	{
		Error(_log, "Frame index = %d, inactive or critical VIDIOC_QBUF error in v4l2 driver. Buf index = %d, worker = %d, is_active = %d.", 
				sourceCount, _V4L2WorkerManager.workers[workerIndex]->GetV4L2Buffer()->index, workerIndex, _V4L2WorkerManager.isActive());	
	}
	
	if (workerIndex <=_V4L2WorkerManager.workersCount)
		_V4L2WorkerManager.workers[workerIndex]->noBusy();	
}


void V4L2Grabber::newWorkerFrame(unsigned int workerIndex, Image<ColorRgb> image, unsigned int sourceCount, quint64 _frameBegin)
{
	frameStat.goodFrame++;
	frameStat.averageFrame += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()-_frameBegin;
	
	//Debug(_log, "Frame index = %d <= received from the thread and it's ready", sourceCount);

	checkSignalDetectionEnabled(image);
	
	// get next frame
	if (workerIndex >_V4L2WorkerManager.workersCount)
		Error(_log, "Frame index = %d, index out of range", sourceCount);	
		
	else if (_V4L2WorkerManager.workers == nullptr ||
		 _V4L2WorkerManager.isActive() == false ||
		 xioctl(VIDIOC_QBUF, _V4L2WorkerManager.workers[workerIndex]->GetV4L2Buffer()))
	{
		Error(_log, "Frame index = %d, inactive or critical VIDIOC_QBUF error in v4l2 driver. Buf index = %d, worker = %d, is_active = %d.", 
				sourceCount, _V4L2WorkerManager.workers[workerIndex]->GetV4L2Buffer()->index, workerIndex, _V4L2WorkerManager.isActive());	
	}
	
	if (workerIndex <=_V4L2WorkerManager.workersCount)
		_V4L2WorkerManager.workers[workerIndex]->noBusy();
}

void V4L2Grabber::checkSignalDetectionEnabled(Image<ColorRgb> image)
{
	if (_signalDetectionEnabled)
	{
		// check signal (only in center of the resulting image, because some grabbers have noise values along the borders)
		bool noSignal = true;

		// top left
		unsigned xOffset  = image.width()  * _x_frac_min;
		unsigned yOffset  = image.height() * _y_frac_min;

		// bottom right
		unsigned xMax     = image.width()  * _x_frac_max;
		unsigned yMax     = image.height() * _y_frac_max;


		for (unsigned x = xOffset; noSignal && x < xMax; ++x)
		{
			for (unsigned y = yOffset; noSignal && y < yMax; ++y)
			{
				noSignal &= (ColorRgb&)image(x, y) <= _noSignalThresholdColor;
			}
		}

		if (noSignal)
		{
			++_noSignalCounter;
		}
		else
		{
			if (_noSignalCounter >= _noSignalCounterThreshold)
			{
				_noSignalDetected = true;
				Info(_log, "Signal detected");
			}

			_noSignalCounter = 0;
		}

		if ( _noSignalCounter < _noSignalCounterThreshold)
		{
			emit newFrame(image);
		}
		else if (_noSignalCounter == _noSignalCounterThreshold)
		{
			_noSignalDetected = false;
			Info(_log, "Signal lost");
		}
	}
	else
	{
		emit newFrame(image);
	}
}

int V4L2Grabber::xioctl(int request, void *arg)
{
	int r;

	do
	{
		r = ioctl(_fileDescriptor, request, arg);
	}
	while (-1 == r && EINTR == errno);

	return r;
}

int V4L2Grabber::xioctl(int fileDescriptor, int request, void *arg)
{
	int r;

	do
	{
		r = ioctl(fileDescriptor, request, arg);
	}
	while (r < 0 && errno == EINTR );

	return r;
}

void V4L2Grabber::enumFrameIntervals(QStringList &framerates, int fileDescriptor, int pixelformat, int width, int height)
{
	// collect available frame rates
	struct v4l2_frmivalenum frmivalenum;
	CLEAR(frmivalenum);

	frmivalenum.index = 0;
	frmivalenum.pixel_format = pixelformat;
	frmivalenum.width = width;
	frmivalenum.height = height;

	while (xioctl(fileDescriptor, VIDIOC_ENUM_FRAMEINTERVALS, &frmivalenum) >= 0)
	{
		int rate;
		switch (frmivalenum.type)
		{
			case V4L2_FRMSIZE_TYPE_DISCRETE:
			{
				if (frmivalenum.discrete.numerator != 0)
				{
					rate = frmivalenum.discrete.denominator / frmivalenum.discrete.numerator;
					if (!framerates.contains(QString::number(rate)))
						framerates.append(QString::number(rate));
				}
			}
			break;
			case V4L2_FRMSIZE_TYPE_CONTINUOUS:
			case V4L2_FRMSIZE_TYPE_STEPWISE:
			{
				if (frmivalenum.stepwise.min.denominator != 0)
				{
					rate = frmivalenum.stepwise.min.denominator / frmivalenum.stepwise.min.numerator;
					if (!framerates.contains(QString::number(rate)))
						framerates.append(QString::number(rate));
				}
			}
		}
		frmivalenum.index++;
	}
}

void V4L2Grabber::setSignalDetectionEnable(bool enable)
{
	if (_signalDetectionEnabled != enable)
	{
		_signalDetectionEnabled = enable;
		Info(_log, "Signal detection is now %s", enable ? "enabled" : "disabled");
	}
}

void V4L2Grabber::setCecDetectionEnable(bool enable)
{
	if (_cecDetectionEnabled != enable)
	{
		_cecDetectionEnabled = enable;
		Info(_log, QString("CEC detection is now %1").arg(enable ? "enabled" : "disabled").toLocal8Bit());
	}
}

void V4L2Grabber::setPixelDecimation(int pixelDecimation)
{
	if (_pixelDecimation != pixelDecimation)
	{
		_pixelDecimation = pixelDecimation;
		_imageResampler.setHorizontalPixelDecimation(pixelDecimation);
		_imageResampler.setVerticalPixelDecimation(pixelDecimation);
	}
}

void V4L2Grabber::setDeviceVideoStandard(QString device, VideoStandard videoStandard)
{
	if (_deviceName != device || _videoStandard != videoStandard)
	{
		// extract input of device
		QChar input = device.at(device.size() - 1);
		_input = input.isNumber() ? input.digitValue() : -1;

		bool started = _initialized;
		uninit();
		_deviceName = device;
		_videoStandard = videoStandard;

		if(started) start();
	}
}

bool V4L2Grabber::setInput(int input)
{
	if(Grabber::setInput(input))
	{
		bool started = _initialized;
		uninit();
		if(started) start();
		return true;
	}
	return false;
}

bool V4L2Grabber::setWidthHeight(int width, int height)
{
	if(Grabber::setWidthHeight(width,height))
	{
		bool started = _initialized;
		uninit();
		if(started) start();
		return true;
	}
	return false;
}

bool V4L2Grabber::setFramerate(int fps)
{
	if(Grabber::setFramerate(fps))
	{
		bool started = _initialized;
		uninit();
		if(started) start();
		return true;
	}
	return false;
}

QStringList V4L2Grabber::getV4L2devices() const
{
	QStringList result = QStringList();
	for (auto it = _deviceProperties.begin(); it != _deviceProperties.end(); ++it)
	{
		result << it.key();
	}
	return result;
}

QString V4L2Grabber::getV4L2deviceName(const QString& devicePath) const
{
	return _deviceProperties.value(devicePath).name;
}

QMultiMap<QString, int> V4L2Grabber::getV4L2deviceInputs(const QString& devicePath) const
{
	return _deviceProperties.value(devicePath).inputs;
}

QStringList V4L2Grabber::getResolutions(const QString& devicePath) const
{
	return _deviceProperties.value(devicePath).resolutions;
}

QStringList V4L2Grabber::getFramerates(const QString& devicePath) const
{
	return _deviceProperties.value(devicePath).framerates;
}

void V4L2Grabber::handleCecEvent(CECEvent event)
{
	switch (event)
	{
		case CECEvent::On  :
			Debug(_log,"CEC on event received");
			_cecStandbyActivated = false;
			return;
		case CECEvent::Off :
			Debug(_log,"CEC off event received");
			_cecStandbyActivated = true;
			return;
		default: break;
	}
}
