#pragma once

#ifndef PCH_ENABLED
	#include <QObject>
	#include <QList>
	#include <QRectF>
	#include <cstdint>
	#include <QJsonObject>
	#include <QMultiMap>
	#include <QSemaphore>
	#include <atomic>
#endif

#include <image/ColorRgb.h>
#include <image/Image.h>
#include <utils/Logger.h>
#include <utils/Components.h>
#include <image/MemoryBuffer.h>
#include <utils/FrameDecoder.h>
#include <utils/LutLoader.h>
#include <base/DetectionManual.h>
#include <base/DetectionAutomatic.h>
#include <performance-counters/PerformanceCounters.h>

#if  defined(_WIN32) || defined(WIN32)
	// Windows
	#include <guiddef.h>
	#include <cguid.h>
#elif defined(__unix__) || defined(__linux__)
	// Linux
	#include <linux/videodev2.h>
#elif defined(__APPLE__)
	#include <CoreGraphics/CoreGraphics.h>
#endif

#define UNSUPPORTED_DECODER "UNSUPPORTED YUV DECODER"

class Grabber : public DetectionAutomatic, public DetectionManual, protected LutLoader
{
	Q_OBJECT

public:
	Grabber(const QString& configurationPath, const QString& grabberName);

	virtual ~Grabber();

	static const QString AUTO_SETTING;
	static const int	 AUTO_FPS;
	static const int	 AUTO_INPUT;

	struct DevicePropertiesItem;

	virtual void setHdrToneMappingEnabled(int mode) override = 0;

	virtual void setCropping(unsigned cropLeft, unsigned cropRight, unsigned cropTop, unsigned cropBottom);

	bool trySetWidthHeight(int width, int height);

	bool trySetInput(int input);

	int getImageWidth();

	int getImageHeight();

	bool isInitialized();

	void setEnabled(bool enable);

	QString getVideoDeviceName(const QString& devicePath) const;

	enum class currentVideoModeInfo { resolution = 0, device };
	QMap<currentVideoModeInfo, QString> getVideoCurrentMode() const;

	QMultiMap<QString, int> getVideoDeviceInputs(const QString& devicePath) const;

	enum class VideoControls { BrightnessDef = 0, ContrastDef, SaturationDef, HueDef };

	QMap<VideoControls, int> getVideoDeviceControls(const QString& devicePath);

	virtual bool init() = 0;

	virtual void uninit() = 0;

	void enableHardwareAcceleration(bool hardware);

	void setMonitorNits(int nits);

	void setFpsSoftwareDecimation(int decimation) override;

	int  getFpsSoftwareDecimation() override;

	QString getSignature() override;

	int  getActualFps() override;

	void setEncoding(QString enc);

	void setBrightnessContrastSaturationHue(int brightness, int contrast, int saturation, int hue);

	void setQFrameDecimation(int setQframe);

	void unblockAndRestart(bool running);

	void setBlocked();

	bool setFramerate(int fps);

	bool setWidthHeight(int width, int height);

	bool setInput(int input);

	void setDeviceVideoStandard(QString device);

	void resetCounter(int64_t from);

	int  getHdrToneMappingEnabled() override;

	void setSignalDetectionEnable(bool enable);

	void setAutoSignalDetectionEnable(bool enable);

	void benchmarkCapture(int status, QString message);

	QList<Grabber::DevicePropertiesItem> getVideoDeviceModesFullInfo(const QString& devicePath);

	QString	getConfigurationPath();

	struct DevicePropertiesItem
	{
		int		x, y, fps, fps_a, fps_b, input;

		PixelFormat				pf;

#if  defined(_WIN32) || defined(WIN32)
		// Windows
		GUID        guid = GUID_NULL;
#elif defined(__unix__) || defined(__linux__)
		// Linux
		__u32       v4l2PixelFormat = 0;
#elif defined(__APPLE__)
		// Apple
		CGDirectDisplayID	display = 0;
		uint32_t			fourcc = 0;
#endif

		DevicePropertiesItem() :
			x(0), y(0), fps(0), fps_a(0), fps_b(0), input(0),
			pf(PixelFormat::NO_CHANGE) {};
	};

public slots:
	virtual bool start() = 0;

	virtual void stop() = 0;

	virtual void newWorkerFrameHandler(unsigned int workerIndex, Image<ColorRgb> image, quint64 sourceCount, qint64 _frameBegin) = 0;

	virtual void newWorkerFrameErrorHandler(unsigned int workerIndex, QString error, quint64 sourceCount) = 0;

	void revive();

	QJsonObject getJsonInfo();

	QStringList getVideoDevices() const;

signals:
	void SignalNewCapturedFrame(const Image<ColorRgb>& image);

	void SignalCapturingException(const char* err);

	void SignalSetNewComponentStateToAllInstances(hyperhdr::Components component, bool enable);

	void SignalBenchmarkUpdate(int status, QString message);

protected:

	int getTargetSystemFrameDimension(int& targetSizeX, int& targetSizeY);

	void processSystemFrameBGRA(uint8_t* source, int lineSize = 0, bool useLut = true);

	void processSystemFrameBGR(uint8_t* source, int lineSize = 0);

	void processSystemFrameBGR16(uint8_t* source, int lineSize = 0);

	void processSystemFrameRGBA(uint8_t* source, int lineSize = 0);

	void processSystemFramePQ10(uint8_t* source, int lineSize = 0);

	void handleNewFrame(unsigned int workerIndex, Image<ColorRgb> image, quint64 sourceCount, qint64 _frameBegin);

	struct DeviceControlCapability
	{
		bool enabled;
		long minVal, maxVal, defVal, current;
		DeviceControlCapability() :
			enabled(false), minVal(0), maxVal(0), defVal(0), current(0) {};
	};

	struct DeviceProperties
	{
		QString						name = QString();
		QMultiMap<QString, int>		inputs = QMultiMap<QString, int>();
		QStringList					resolutions = QStringList();
		QStringList					displayResolutions = QStringList();
		QStringList					framerates = QStringList();
		DeviceControlCapability		brightness, contrast, saturation, hue;
		QList<DevicePropertiesItem> valid = QList<DevicePropertiesItem>();
	};

	QMap<QString, DeviceProperties> _deviceProperties;


	/// the selected HDR mode
	int _hdr;

	QString	_configurationPath;

	/// With of the captured snapshot [pixels]
	int _width;

	/// Height of the captured snapshot [pixels]
	int _height;

	/// frame per second
	int _fps;

	/// device input
	int _input;

	/// number of pixels to crop after capturing
	int _cropLeft, _cropRight, _cropTop, _cropBottom;

	bool _enabled;

	/// logger instance
	Logger* _log;

	// statistics
	struct {
		int64_t			token = 0;
		int64_t			frameBegin;
		int   			averageFrame;
		unsigned int	badFrame, goodFrame, segment;
		bool			directAccess;
	} frameStat;

	std::atomic<unsigned long>	 _currentFrame;

	QString		_deviceName;

	PixelFormat	_enc;
	int			_brightness, _contrast, _saturation, _hue;
	bool		_qframe;
	bool		_blocked;
	bool		_restartNeeded;
	bool		_initialized;
	int			_fpsSoftwareDecimation;
	bool		_hardware;

	PixelFormat _actualVideoFormat;
	int			_actualWidth, _actualHeight, _actualFPS;
	QString		_actualDeviceName;
	uint		_targetMonitorNits;

	int			_lineLength;
	int			_frameByteSize;

	bool		_signalDetectionEnabled;
	bool		_signalAutoDetectionEnabled;
	QSemaphore  _synchro;

	int			_benchmarkStatus;
	QString		_benchmarkMessage;
};

bool sortDevicePropertiesItem(const Grabber::DevicePropertiesItem& v1, const Grabber::DevicePropertiesItem& v2);
