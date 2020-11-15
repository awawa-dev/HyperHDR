#pragma once
#include <Guiddef.h>
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
#include <hyperion/Grabber.h>
#include <grabber/QTCWorker.h>
#include <utils/Components.h>

// TurboJPEG decoder
#ifdef HAVE_TURBO_JPEG
	#include <QImage>
	#include <QColor>
	#include <turbojpeg.h>
#endif

/// Capture class for V4L2 devices
class SourceReaderCB;
struct IMFSourceReader;

class QTCGrabber : public Grabber
{
	Q_OBJECT

public:
	struct DevicePropertiesItem
	{
		int 		x,y,fps,fps_a,fps_b;
		PixelFormat pf;
		GUID        guid;
	};
	struct DeviceProperties
	{
		QString					name		= QString();
		QMultiMap<QString, int>	inputs		= QMultiMap<QString, int>();
		QStringList				displayResolutions = QStringList();		
		QStringList				framerates	= QStringList();	
		QList<DevicePropertiesItem>    valid = QList<DevicePropertiesItem>();
	};
	
	typedef struct
	{
		const GUID		format_id;
		const QString	format_name;
		PixelFormat		pixel;
	} format_t;

	QTCGrabber(const QString & device,
			const unsigned width,
			const unsigned height,
			const unsigned fps,
			const unsigned input,			
			PixelFormat pixelFormat,
			const QString & configurationPath
	);
	~QTCGrabber() override;

	QRectF getSignalDetectionOffset() const
	{
		return QRectF(_x_frac_min, _y_frac_min, _x_frac_max, _y_frac_max);
	}

	bool getSignalDetectionEnabled() const { return _signalDetectionEnabled; }
	bool getCecDetectionEnabled() const { return _cecDetectionEnabled; }
	void receive_image(const void *frameImageBuffer, int size, QString message);
	int grabFrame(Image<ColorRgb> &);

	///
	/// @brief  overwrite Grabber.h implementation
	///
	void setSignalThreshold(
					double redSignalThreshold,
					double greenSignalThreshold,
					double blueSignalThreshold,
					int noSignalCounterThreshold) override;

	///
	/// @brief  overwrite Grabber.h implementation
	///
	void setSignalDetectionOffset(
					double verticalMin,
					double horizontalMin,
					double verticalMax,
					double horizontalMax) override;
	///
	/// @brief  overwrite Grabber.h implementation
	///
	void setSignalDetectionEnable(bool enable) override;

	///
	/// @brief  overwrite Grabber.h implementation
	///
	void setCecDetectionEnable(bool enable) override;

	///
	/// @brief overwrite Grabber.h implementation
	///
	void setDeviceVideoStandard(QString device) override;

	///
	/// @brief overwrite Grabber.h implementation
	///
	bool setInput(int input) override;

	///
	/// @brief overwrite Grabber.h implementation
	///
	bool setWidthHeight(int width, int height) override;

	///
	/// @brief overwrite Grabber.h implementation
	///
	bool setFramerate(int fps) override;

	///
	/// @brief overwrite Grabber.h implementation
	///
	QStringList getV4L2devices() const override;

	///
	/// @brief overwrite Grabber.h implementation
	///
	QString getV4L2deviceName(const QString& devicePath) const override;

	///
	/// @brief overwrite Grabber.h implementation
	///
	QMultiMap<QString, int> getV4L2deviceInputs(const QString& devicePath) const override;

	///
	/// @brief overwrite Grabber.h implementation
	///
	QStringList getResolutions(const QString& devicePath) const override;

	///
	/// @brief overwrite Grabber.h implementation
	///
	QStringList getFramerates(const QString& devicePath) const override;		
	
	///
	/// @brief set software decimation (v4l2)
	///
	void setFpsSoftwareDecimation(int decimation);
	
	///
	/// @brief enable HDR to SDR tone mapping (v4l2)
	///
	void setHdrToneMappingEnabled(int mode) override;
	
	void setEncoding(QString enc);
	
	void setBrightnessContrastSaturationHue(int brightness, int contrast, int saturation, int hue);

public slots:

	bool start();

	void stop();
	
	void newWorkerFrame(unsigned int _workerIndex, const Image<ColorRgb>& image,unsigned int sourceCount, quint64 _frameBegin);	
	void newWorkerFrameError(unsigned int _workerIndex, QString error,unsigned int sourceCount);
signals:
	
	void newFrame(const Image<ColorRgb> & image);
	void readError(const char* err);



private:
	void getV4Ldevices(bool silent);
	QString GetSharedLut();
	
	void loadLutFile(const QString & color);
	
	void getV4Ldevices();

	bool init();

	void uninit();		

	bool init_device(QString device, DevicePropertiesItem props);

	void uninit_device();

	void start_capturing();

	bool process_image(const void *frameImageBuffer, int size);

	
	void checkSignalDetectionEnabled(Image<ColorRgb> image);
	
	QString FormatRes(int w,int h, QString format);

	QString FormatFrame(int fr);
	
	QString identify_format(const GUID format, PixelFormat& pixelformat);

	
private:
	enum io_method
	{
			
			IO_METHOD_MMAP
			
	};

	struct buffer
	{
			void   *start;
			size_t  length;
	};



#ifdef HAVE_TURBO_JPEG
	tjhandle _decompress = nullptr;
	int _subsamp;
#endif

private:
	QString _deviceName;
	QMap<QString, QTCGrabber::DeviceProperties> _deviceProperties;
	
	io_method           _ioMethod;
	int                 _fileDescriptor;
	std::vector<buffer> _buffers;
	bool				_isMF;
	SourceReaderCB*     _sourceReaderCB;
	
	
	// statistics
	struct {
		uint64_t	frameBegin;
		int   		averageFrame;
		unsigned int	badFrame,goodFrame,segment;
	} frameStat;

	PixelFormat _pixelFormat;
	int         _lineLength;
	int         _frameByteSize;

	// signal detection
	int      _noSignalCounterThreshold;
	ColorRgb _noSignalThresholdColor;
	bool     _signalDetectionEnabled;
	bool     _cecDetectionEnabled;
	bool     _cecStandbyActivated;
	bool     _noSignalDetected;
	int      _noSignalCounter;
	double   _x_frac_min;
	double   _y_frac_min;
	double   _x_frac_max;
	double   _y_frac_max;

	bool _initialized;
	bool _deviceAutoDiscoverEnabled;	
	
	// accept only frame: n'th mod fpsSoftwareDecimation == 0 
	int            _fpsSoftwareDecimation;
	
	// memory buffer for 3DLUT HDR tone mapping
	unsigned char  *lutBuffer;		
	bool 		_lutBufferInit;
	
	// frame counter
	volatile unsigned int _currentFrame;
		
	// hyperion configuration folder
	QString        _configurationPath;		
	
		
	QTCWorkerManager _QTCWorkerManager;
	
	void	ResetCounter(uint64_t from);	
	
	PixelFormat        _enc;

	IMFSourceReader* READER;
	
	int _brightness,_contrast, _saturation, _hue;
	//static format_t fmt_array[];
};
