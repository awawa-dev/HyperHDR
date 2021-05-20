#pragma once

#include <QObject>
#include <QList>
#include <QRectF>
#include <cstdint>

#include <utils/ColorRgb.h>
#include <utils/Image.h>
#include <utils/ImageResampler.h>
#include <utils/Logger.h>
#include <utils/Components.h>
#include <hyperhdrbase/DetectionManual.h>
#include <hyperhdrbase/DetectionAutomatic.h>

#include <QMultiMap>


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

#define LUT_FILE_SIZE 50331648

class Grabber : public DetectionAutomatic, public DetectionManual
{
	Q_OBJECT

public:
	Grabber(const QString& grabberName = "", int width = 0, int height = 0,
			int cropLeft = 0, int cropRight = 0, int cropTop = 0, int cropBottom = 0);

	~Grabber();	

	static const QString AUTO_SETTING;
	static const int	 AUTO_FPS;
	static const int	 AUTO_INPUT;

	struct DevicePropertiesItem;

	virtual void setHdrToneMappingEnabled(int mode) = 0;

	virtual void setCropping(unsigned cropLeft, unsigned cropRight, unsigned cropTop, unsigned cropBottom);

	bool trySetWidthHeight(int width, int height);

	bool trySetInput(int input);

	int getImageWidth();

	int getImageHeight();

	void setEnabled(bool enable);

	QStringList getVideoDevices() const;

	QString getVideoDeviceName(const QString& devicePath) const;

	enum class currentVideoModeInfo { resolution = 0, device };
	QMap<currentVideoModeInfo, QString> getVideoCurrentMode() const;

	QMultiMap<QString, int> getVideoDeviceInputs(const QString& devicePath) const;

	enum class VideoControls { BrightnessDef = 0, ContrastDef, SaturationDef, HueDef };

	QMap<VideoControls, int> getVideoDeviceControls(const QString& devicePath);

	virtual bool init() = 0;

	virtual void uninit() = 0;

	void setFpsSoftwareDecimation(int decimation);

	int  getFpsSoftwareDecimation();

	int  getActualFps();

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

	int  getHdrToneMappingEnabled();

	void setSignalDetectionEnable(bool enable);

	void setAutoSignalDetectionEnable(bool enable);

	QList<Grabber::DevicePropertiesItem> getVideoDeviceModesFullInfo(const QString& devicePath);

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

	virtual void newWorkerFrame(unsigned int workerIndex, Image<ColorRgb> image, quint64 sourceCount, qint64 _frameBegin) = 0;

	virtual void newWorkerFrameError(unsigned int workerIndex, QString error, quint64 sourceCount) = 0;

signals:
	void newFrame(const Image<ColorRgb>& image);

	void readError(const char* err);

protected:
	void loadLutFile(PixelFormat color, const QList<QString>& files);

	void processSystemFrameBGRA(uint8_t* source, int lineSize = 0);

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

	// enable/disable HDR tone mapping
	uint8_t _hdrToneMappingEnabled;
	
	/// logger instance
	Logger* _log;

	// statistics
	struct {
		int64_t			frameBegin;
		int   			averageFrame;
		unsigned int	badFrame, goodFrame, segment;
	} frameStat;

	volatile uint64_t   _currentFrame;

	QString		_deviceName;

	PixelFormat	_enc;
	int			_brightness, _contrast, _saturation, _hue;
	bool		_qframe;
	bool		_blocked;
	bool		_restartNeeded;
	bool		_initialized;
	int			_fpsSoftwareDecimation;

	PixelFormat _actualVideoFormat;
	int			_actualWidth, _actualHeight, _actualFPS;
	QString		_actualDeviceName;

	uint8_t*	_lutBuffer;
	bool		_lutBufferInit;

	int			_lineLength;
	int			_frameByteSize;

	bool		_signalDetectionEnabled;
	bool		_signalAutoDetectionEnabled;
};

bool sortDevicePropertiesItem(const Grabber::DevicePropertiesItem& v1, const Grabber::DevicePropertiesItem& v2);
