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
#include <hyperion/Grabber.h>
#include <grabber/V4L2Worker.h>
#include <grabber/VideoStandard.h>
#include <utils/Components.h>
#include <cec/CECEvent.h>

// general JPEG decoder includes
#ifdef HAVE_JPEG_DECODER
	#include <QImage>
	#include <QColor>
#endif

// System JPEG decoder
#ifdef HAVE_JPEG
	#include <jpeglib.h>
	#include <csetjmp>
#endif

// TurboJPEG decoder
#ifdef HAVE_TURBO_JPEG
	#include <turbojpeg.h>
#endif

/// Capture class for V4L2 devices
///
/// @see http://linuxtv.org/downloads/v4l-dvb-apis/capture-example.html
class V4L2Grabber : public Grabber
{
	Q_OBJECT

public:
	struct DeviceProperties
	{
		QString					name		= QString();
		QMultiMap<QString, int>	inputs		= QMultiMap<QString, int>();
		QStringList				resolutions	= QStringList();
		QStringList				framerates	= QStringList();
	};

	V4L2Grabber(const QString & device,
			const unsigned width,
			const unsigned height,
			const unsigned fps,
			const unsigned input,
			VideoStandard videoStandard,
			PixelFormat pixelFormat,
			const QString & configurationPath
	);
	~V4L2Grabber() override;

	QRectF getSignalDetectionOffset() const
	{
		return QRectF(_x_frac_min, _y_frac_min, _x_frac_max, _y_frac_max);
	}

	bool getSignalDetectionEnabled() const { return _signalDetectionEnabled; }
	bool getCecDetectionEnabled() const { return _cecDetectionEnabled; }

	int grabFrame(Image<ColorRgb> &);

	///
	/// @brief  overwrite Grabber.h implementation
	///
	void setSignalThreshold(
					double redSignalThreshold,
					double greenSignalThreshold,
					double blueSignalThreshold,
					int noSignalCounterThreshold = 50) override;

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
	void setDeviceVideoStandard(QString device, VideoStandard videoStandard) override;

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
	void setHdrToneMappingEnabled(int mode);
	
	void setEncoding(QString enc);

public slots:

	bool start();

	void stop();

	void handleCecEvent(CECEvent event);
	
	void newWorkerFrame(unsigned int _workerIndex, Image<ColorRgb> image,unsigned int sourceCount, quint64 _frameBegin);	
	void newWorkerFrameError(unsigned int _workerIndex, QString error,unsigned int sourceCount);
signals:
	
	void newFrame(const Image<ColorRgb> & image);
	void readError(const char* err);

private slots:
	int read_frame();

private:
	QString GetSharedLut();
	
	void loadLutFile(const QString & color);
	
	void getV4Ldevices();

	bool init();

	void uninit();

	bool open_device();

	void close_device();
	
	void init_mmap();	

	void init_device(VideoStandard videoStandard);

	void uninit_device();

	void start_capturing();

	void stop_capturing();

	bool process_image(v4l2_buffer* buf, const void *frameImageBuffer, int size);

	int xioctl(int request, void *arg);

	int xioctl(int fileDescriptor, int request, void *arg);

	void throw_exception(const QString & error)
	{
		Error(_log, "Throws error: %s", QSTRING_CSTR(error));
	}

	void throw_errno_exception(const QString & error)
	{
		Error(_log, "Throws error nr: %s", QSTRING_CSTR(QString(error + " error code " + QString::number(errno) + ", " + strerror(errno))));
	}
	
	void checkSignalDetectionEnabled(Image<ColorRgb> image);
	
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

#ifdef HAVE_JPEG
	struct errorManager
	{
		jpeg_error_mgr pub;
		jmp_buf setjmp_buffer;
	};

	static void errorHandler(j_common_ptr cInfo)
	{
		errorManager* mgr = reinterpret_cast<errorManager*>(cInfo->err);
		longjmp(mgr->setjmp_buffer, 1);
	}

	static void outputHandler(j_common_ptr cInfo)
	{
		// Suppress fprintf warnings.
	}

	jpeg_decompress_struct* _decompress;
	errorManager* _error;
#endif

#ifdef HAVE_TURBO_JPEG
	tjhandle _decompress = nullptr;
	int _subsamp;
#endif

private:
	QString _deviceName;
	std::map<QString, QString> _v4lDevices;
	QMap<QString, V4L2Grabber::DeviceProperties> _deviceProperties;

	VideoStandard       _videoStandard;
	io_method           _ioMethod;
	int                 _fileDescriptor;
	std::vector<buffer> _buffers;
	
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

	QSocketNotifier *_streamNotifier;

	bool _initialized;
	bool _deviceAutoDiscoverEnabled;
	
	// enable/disable HDR tone mapping
	uint8_t       _hdrToneMappingEnabled;
	
	// accept only frame: n'th mod fpsSoftwareDecimation == 0 
	int            _fpsSoftwareDecimation;
	
	// memory buffer for 3DLUT HDR tone mapping
	unsigned char  *lutBuffer;		
	bool 		_lutBufferInit;
	
	// frame counter
	volatile unsigned int _currentFrame;
		
	// hyperion configuration folder
	QString        _configurationPath;		
	
		
	V4L2WorkerManager _V4L2WorkerManager;
	
	void	ResetCounter(uint64_t from);	
	
	QString        _enc;
protected:
	void enumFrameIntervals(QStringList &framerates, int fileDescriptor, int pixelformat, int width, int height);
};
