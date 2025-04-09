// LedDevice includes
#include <leddevice/LedDevice.h>
#include "ProviderFtdi.h"

#include <ftdi.h>
#include <libusb.h>

#include <QEventLoop>

#define ANY_FTDI_VENDOR 0x0
#define ANY_FTDI_PRODUCT 0x0

namespace Pin
{
	// enumerate the AD bus for conveniance.
	enum bus_t
	{
		SK = 0x01, // ADBUS0, SPI data clock
		DO = 0x02, // ADBUS1, SPI data out
		CS = 0x08, // ADBUS3, SPI chip select, active low
	};
}

static const QString AUTO_SETTING = QString("auto");
static const uint8_t pinInitialState = Pin::CS;
static const uint8_t pinDirection = Pin::SK | Pin::DO | Pin::CS;

ProviderFtdi::ProviderFtdi(const QJsonObject& deviceConfig)
	: LedDevice(deviceConfig),
	_ftdic(nullptr),
	_baudRate_Hz(1000000)
{
}

bool ProviderFtdi::init(const QJsonObject& deviceConfig)
{
	bool isInitOK = false;

	if (LedDevice::init(deviceConfig))
	{
		_baudRate_Hz = deviceConfig["rate"].toInt(_baudRate_Hz);
		_deviceName = deviceConfig["output"].toString(AUTO_SETTING);

		Debug(_log, "_baudRate_Hz [%d]", _baudRate_Hz);
		Debug(_log, "_deviceName [%s]", QSTRING_CSTR(_deviceName));

		isInitOK = true;
	}
	return isInitOK;
}

bool ProviderFtdi::openDevice()
{
	_ftdic = ftdi_new();

	if (ftdi_init(_ftdic) < 0)
	{
		_ftdic = nullptr;
		setInError("Could not initialize the ftdi library");
		return false;
	}

	bool autoDiscovery = (QString::compare(_deviceName, AUTO_SETTING, Qt::CaseInsensitive) == 0);
	Debug(_log, "Opening FTDI device=%s autoDiscovery=%s", QSTRING_CSTR(_deviceName), autoDiscovery ? "true" : "false");

	if (autoDiscovery)
	{
		struct ftdi_device_list* devlist = nullptr;

		if (ftdi_usb_find_all(_ftdic, &devlist, ANY_FTDI_VENDOR, ANY_FTDI_PRODUCT) < 0 ||
			ftdi_usb_open_dev(_ftdic, devlist[0].dev) != 0)
		{
			if (devlist != nullptr)
				ftdi_list_free(&devlist);			
			setInError(QString("%1").fromUtf8(ftdi_get_error_string(_ftdic)));
		}
		else
			ftdi_list_free(&devlist);
	}
	else
	{
		if (ftdi_usb_open_string(_ftdic, QSTRING_CSTR(_deviceName)) < 0)
		{
			setInError(QString("%1").fromUtf8(ftdi_get_error_string(_ftdic)));
		}
	}
	return (_ftdic != nullptr);
}
int ProviderFtdi::open()
{
	if (_retryMode)
		return -1;

	_isDeviceReady = false;

	if (!openDevice())
	{
		return -1;
	}

	double reference_clock = 60e6;
	int divisor = (reference_clock / 2 / _baudRate_Hz) - 1;
	std::vector<uint8_t> buf = {
			DIS_DIV_5,
			TCK_DIVISOR,
			static_cast<unsigned char>(divisor),
			static_cast<unsigned char>(divisor >> 8),
			SET_BITS_LOW,		  // opcode: set low bits (ADBUS[0-7]
			pinInitialState,	// argument: inital pin state
			pinDirection
	};

	if (ftdi_disable_bitbang(_ftdic) < 0 ||
		ftdi_setflowctrl(_ftdic, SIO_DISABLE_FLOW_CTRL) < 0 ||
		ftdi_set_bitmode(_ftdic, 0x00, BITMODE_RESET) < 0 ||
		ftdi_set_bitmode(_ftdic, 0xff, BITMODE_MPSSE) < 0 ||
		ftdi_write_data(_ftdic, buf.data(), buf.size()) != buf.size())
	{
		setInError(QString("%1").fromUtf8(ftdi_get_error_string(_ftdic)));
		return -1;
	}

	_isDeviceReady = true;
	_currentRetry = 0;
	_retryMode = false;

	return 0;
}

int ProviderFtdi::close()
{
	_isDeviceReady = false;

	Debug(_log, "Closing FTDI device");
	if (_ftdic != nullptr)
	{
		ftdi_usb_close(_ftdic);
		ftdi_deinit(_ftdic);
		_ftdic = nullptr;
	}

	return 0;
}

int ProviderFtdi::writeBytes(const qint64 size, const uint8_t* data)
{
	int count_arg = size - 1;
	std::vector<uint8_t> buf =
	{
		SET_BITS_LOW,
		pinInitialState & ~Pin::CS,
		pinDirection,
		MPSSE_DO_WRITE | MPSSE_WRITE_NEG,
		static_cast<unsigned char>(count_arg),
		static_cast<unsigned char>(count_arg >> 8),
		SET_BITS_LOW,
		pinInitialState | Pin::CS,
		pinDirection
	};
	// insert before last SET_BITS_LOW command
	// SET_BITS_LOW takes 2 arguments, so we're inserting data in -3 position from the end
	buf.insert(buf.end() - 3, &data[0], &data[size]);

	if (_ftdic != nullptr && ftdi_write_data(_ftdic, buf.data(), buf.size()) != buf.size())
	{
		setInError(ftdi_get_error_string(_ftdic));
		return -1;
	}
	return 0;
}

QJsonObject ProviderFtdi::discover(const QJsonObject& /*params*/)
{
	QJsonObject devicesDiscovered;
	QJsonArray deviceList;
	struct ftdi_device_list* devlist;
	struct ftdi_context* ftdic;

	QJsonObject autoDevice = QJsonObject{ {"value", AUTO_SETTING}, {"name", "Auto"} };
	deviceList.push_back(autoDevice);

	ftdic = ftdi_new();

	if (ftdi_usb_find_all(ftdic, &devlist, ANY_FTDI_VENDOR, ANY_FTDI_PRODUCT) > 0)
	{
		QMap<QString, uint8_t> deviceIndexes;
		struct ftdi_device_list* curdev = devlist;
		while (curdev)
		{
			char manufacturer[128] = { 0 }, serial_string[128] = { 0 };
			ftdi_usb_get_strings(ftdic, curdev->dev, manufacturer, 128, NULL, 0, serial_string, 128);

			libusb_device_descriptor desc;
			libusb_get_device_descriptor(curdev->dev, &desc);

			QString vendorAndProduct = QString("0x%1:0x%2")
				.arg(desc.idVendor, 4, 16, QChar{ '0' })
				.arg(desc.idProduct, 4, 16, QChar{ '0' });

			QString serialNumber{ serial_string };
			QString ftdiOpenString;
			if (!serialNumber.isEmpty())
			{
				ftdiOpenString = QString("s:%1:%2").arg(vendorAndProduct).arg(serialNumber);
			}
			else
			{
				uint8_t deviceIndex = deviceIndexes.value(vendorAndProduct, 0);
				ftdiOpenString = QString("i:%1:%2").arg(vendorAndProduct).arg(deviceIndex);
				deviceIndexes.insert(vendorAndProduct, deviceIndex + 1);
			}

			QString displayLabel = QString("%1 (%2)")
				.arg(ftdiOpenString)
				.arg(manufacturer);

			deviceList.push_back(QJsonObject{
				{"value", ftdiOpenString},
				{"name", displayLabel}
			});

			curdev = curdev->next;
		}
	}

	ftdi_list_free(&devlist);
	ftdi_free(ftdic);

	devicesDiscovered.insert("ledDeviceType", _activeDeviceType);
	devicesDiscovered.insert("devices", deviceList);

	Debug(_log, "FTDI devices discovered: [%s]", QString(QJsonDocument(devicesDiscovered).toJson(QJsonDocument::Compact)).toUtf8().constData());

	return devicesDiscovered;
}

void ProviderFtdi::setInError(const QString& errorMsg)
{
	this->close();

	LedDevice::setInError(errorMsg);
}
