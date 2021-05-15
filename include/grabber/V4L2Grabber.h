#pragma once

// stl includes
#include <vector>
#include <map>
#include <chrono>

// Qt includes
#include <QObject>
#include <QSocketNotifier>
#include <QRectF>
#include <QMap>
#include <QMultiMap>

// util includes
#include <utils/PixelFormat.h>
#include <hyperhdrbase/Grabber.h>
#include <grabber/V4L2Worker.h>
#include <utils/Components.h>

// general JPEG decoder includes
#include <QImage>
#include <QColor>
#include <turbojpeg.h>

class V4L2Grabber : public Grabber
{
	Q_OBJECT

public:
	struct HyperHdrFormat
	{
		__u32		v4l2Format;
		PixelFormat innerFormat;
	};

	struct DevicePropertiesItem
	{
		int 		x, y, fps, input;
		PixelFormat pf;
		__u32       pixel_format;
	};

	struct DeviceProperties
	{
		QString						name = QString();
		QMultiMap<QString, int>		inputs = QMultiMap<QString, int>();
		QStringList					resolutions = QStringList();
		QStringList					displayResolutions = QStringList();
		QStringList					framerates = QStringList();
		QList<DevicePropertiesItem> valid = QList<DevicePropertiesItem>();
	};

	V4L2Grabber(const QString& device,
			const unsigned width,
			const unsigned height,
			const unsigned fps,
			const unsigned input,			
			PixelFormat pixelFormat,
			const QString& configurationPath
	);
	~V4L2Grabber() override;

	QRectF getSignalDetectionOffset()  const;
	bool   getSignalDetectionEnabled() const;

	int  getHdrToneMappingEnabled();
	int  grabFrame(Image<ColorRgb> &);
	
	void setSignalThreshold(
					double redSignalThreshold,
					double greenSignalThreshold,
					double blueSignalThreshold,
					int noSignalCounterThreshold) override;
	
	void setSignalDetectionOffset(
					double verticalMin,
					double horizontalMin,
					double verticalMax,
					double horizontalMax) override;

	void setSignalDetectionEnable(bool enable) override;
	
	void setDeviceVideoStandard(QString device) override;

	bool setInput(int input) override;

	bool setWidthHeight(int width, int height) override;

	bool setFramerate(int fps) override;

	QStringList getV4L2devices() const override;

	QString getV4L2deviceName(const QString& devicePath) const override;

	QMultiMap<QString, int> getV4L2deviceInputs(const QString& devicePath) const override;

	QStringList getResolutions(const QString& devicePath) const override;

	QStringList getFramerates(const QString& devicePath) const override;		
	
	void setFpsSoftwareDecimation(int decimation);
	
	void setHdrToneMappingEnabled(int mode) override;
	
	void setEncoding(QString enc);
	
	void setBrightnessContrastSaturationHue(int brightness, int contrast, int saturation, int hue);

	void setQFrameDecimation(int setQframe);

public slots:

	bool start();

	void stop();
	
	void newWorkerFrame(unsigned int workerIndex, Image<ColorRgb> image, quint64 sourceCount, qint64 _frameBegin);
	void newWorkerFrameError(unsigned int workerIndex, QString error, quint64 sourceCount);
signals:
	
	void newFrame(const Image<ColorRgb>& image);
	void readError(const char* err);

private slots:
	int read_frame();

private:
	QString GetSharedLut();

	void enumerateV4L2devices(bool silent);

	void loadLutFile(const QString& color);
	
	void getV4Ldevices();

	bool init();

	void uninit();

	void close_device();
	
	bool init_mmap();	

	bool init_device(QString selectedDeviceName, DevicePropertiesItem props);

	void uninit_device();

	void start_capturing();

	void stop_capturing();

	bool process_image(v4l2_buffer* buf, const void* frameImageBuffer, int size);

	int xioctl(int request, void* arg);

	int xioctl(int fileDescriptor, int request, void* arg);

	void throw_exception(const QString& error);

	void throw_errno_exception(const QString& error);
	
	void checkSignalDetectionEnabled(Image<ColorRgb> image);

	void resetCounter(int64_t from);

	QString formatRes(int w, int h, QString format);

	QString formatFrame(int fr);

	PixelFormat identifyFormat(__u32 format);
	
private:
	
	struct buffer
	{
		void*	start;
		size_t  length;
	};
	
	QString _deviceName;
	QMap<QString, V4L2Grabber::DeviceProperties> _deviceProperties;

	int                 _fileDescriptor;
	std::vector<buffer> _buffers;
	
	// statistics
	struct {
		int64_t			frameBegin;
		int   			averageFrame;
		unsigned int	badFrame, goodFrame, segment;
	} frameStat;

	PixelFormat _pixelFormat;
	int         _lineLength;
	int         _frameByteSize;

	// signal detection
	int         _noSignalCounterThreshold;
	ColorRgb	_noSignalThresholdColor;
	bool		_signalDetectionEnabled;
	bool		_noSignalDetected;
	int			_noSignalCounter;
	double		_x_frac_min;
	double		_y_frac_min;
	double		_x_frac_max;
	double		_y_frac_max;

	QSocketNotifier *_streamNotifier;

	bool		_initialized;
	bool		_deviceAutoDiscoverEnabled;
	int			_fpsSoftwareDecimation;

	// memory buffer for 3DLUT HDR tone mapping
	uint8_t*	_lutBuffer;		
	bool		_lutBufferInit;
	
	// frame counter
	volatile uint64_t   _currentFrame;			
	QString				_configurationPath;				
	V4L2WorkerManager   _V4L2WorkerManager;	
	
	PixelFormat	_enc;
	int			_brightness,_contrast, _saturation, _hue;
	bool		_qframe;
};
