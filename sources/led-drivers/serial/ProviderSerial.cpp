#include <HyperhdrConfig.h>

#ifdef ENABLE_BONJOUR
	#include <bonjour/DiscoveryWrapper.h>	
#endif

// LedDevice includes
#include <led-drivers/LedDevice.h>
#include <led-drivers/serial/ProviderSerial.h>

// qt includes
#include <QSerialPortInfo>
#include <QEventLoop>
#include <QThread>
#include <QSerialPort>

#include <utils/InternalClock.h>
#include <utils/GlobalSignals.h>

#include <led-drivers/serial/EspTools.h>

// Constants
constexpr std::chrono::milliseconds WRITE_TIMEOUT{ 1000 };	// device write timeout in ms
constexpr std::chrono::milliseconds OPEN_TIMEOUT{ 5000 };		// device open timeout in ms
const int MAX_WRITE_TIMEOUTS = 5;	// Maximum number of allowed timeouts
const int NUM_POWEROFF_WRITE_BLACK = 3;	// Number of write "BLACK" during powering off

ProviderSerial::ProviderSerial(const QJsonObject& deviceConfig)
	: LedDevice(deviceConfig)
	, _serialPort(nullptr)
	, _baudRate_Hz(1000000)
	, _isAutoDeviceName(false)
	, _delayAfterConnect_ms(0)
	, _frameDropCounter(0)
	, _espHandshake(true)
	, _forceSerialDetection(false)
{
}

bool ProviderSerial::init(const QJsonObject& deviceConfig)
{
	bool isInitOK = false;

	if (_serialPort == nullptr)
		_serialPort = new QSerialPort(this);

	if (LedDevice::init(deviceConfig))
	{

		Debug(_log, "DeviceType   : %s", QSTRING_CSTR(this->getActiveDeviceType()));
		Debug(_log, "LedCount     : %d", this->getLedCount());
		Debug(_log, "RefreshTime  : %d", this->getRefreshTime());

		_deviceName = deviceConfig["output"].toString("auto");

		// If device name was given as unix /dev/ system-location, get port name
		if (_deviceName.startsWith(QLatin1String("/dev/")))
			_deviceName = _deviceName.mid(5);

		_isAutoDeviceName = _deviceName.toLower() == "auto";
		_baudRate_Hz = deviceConfig["rate"].toInt();
		_delayAfterConnect_ms = deviceConfig["delayAfterConnect"].toInt(0);
		_espHandshake = deviceConfig["espHandshake"].toBool(false);
		_maxRetry = _devConfig["maxRetry"].toInt(60);
		_forceSerialDetection = deviceConfig["forceSerialDetection"].toBool(false);

		Debug(_log, "Device name   : %s", QSTRING_CSTR(_deviceName));
		Debug(_log, "Auto selection: %d", _isAutoDeviceName);
		Debug(_log, "Baud rate     : %d", _baudRate_Hz);
		Debug(_log, "ESP handshake : %s", (_espHandshake) ? "ON" : "OFF");
		Debug(_log, "Force ESP/Pico Detection : %s", (_forceSerialDetection) ? "ON" : "OFF");
		Debug(_log, "Delayed open  : %d", _delayAfterConnect_ms);
		Debug(_log, "Retry limit   : %d", _maxRetry);

		if (_defaultInterval > 0)
			Warning(_log, "The refresh timer is enabled ('Refresh time' > 0) and may limit the performance of the LED driver. Ignore this error if you set it on purpose for some reason (but you almost never need it).");

		isInitOK = true;
	}
	return isInitOK;
}

ProviderSerial::~ProviderSerial()
{
	if (_serialPort != nullptr && _serialPort->isOpen())
		_serialPort->close();

	delete _serialPort;

	_serialPort = nullptr;
}

int ProviderSerial::open()
{
	int retval = -1;

	if (_serialPort == nullptr)
		return retval;

	_isDeviceReady = false;

	// open device physically
	if (tryOpen(_delayAfterConnect_ms))
	{
		// Everything is OK, device is ready
		_isDeviceReady = true;
		retval = 0;
	}
	else
	{
		setupRetry(2500);
	}
	return retval;
}

bool ProviderSerial::waitForExitStats()
{
	if (_serialPort->isOpen())
	{
		if (_serialPort->bytesAvailable() > 32)
		{
			auto check = _serialPort->peek(256);
			if (check.lastIndexOf('\n') > 1)
			{
				auto incoming = QString(_serialPort->readAll());

				Info(_log, "Received goodbye: '%s' (%i)", QSTRING_CSTR(incoming), incoming.length());
				return true;
			}
		}		
	}

	return false;
}

int ProviderSerial::close()
{
	int retval = 0;

	_isDeviceReady = false;

	if (_serialPort != nullptr && _serialPort->isOpen())
	{
		if (_serialPort->flush())
		{
			Debug(_log, "Flush was successful");
		}


		if (_espHandshake)
		{
			QTimer::singleShot(200, this, [this]() { if (_serialPort->isOpen()) EspTools::goingSleep(_serialPort); });
			EspTools::goingSleep(_serialPort);

			for (int i = 0; i < 6 && _serialPort->isOpen(); i++)
			{
				if (_serialPort->waitForReadyRead(100) && waitForExitStats())
					break;
			}
		}

		_serialPort->close();

		Debug(_log, "Serial port is closed: %s", QSTRING_CSTR(_deviceName));

	}
	return retval;
}

bool ProviderSerial::powerOff()
{
	// Simulate power-off by writing a final "Black" to have a defined outcome
	bool rc = false;
	if (writeBlack(NUM_POWEROFF_WRITE_BLACK) >= 0)
	{
		rc = true;
	}
	return rc;
}

bool ProviderSerial::tryOpen(int delayAfterConnect_ms)
{
	if (_deviceName.isEmpty() || _serialPort->portName().isEmpty() || (_isAutoDeviceName && _espHandshake))
	{
		if (!_serialPort->isOpen())
		{
			if (_isAutoDeviceName)
			{
				_deviceName = discoverFirst();
				if (_deviceName.isEmpty())
				{
					this->setInError(QString("No serial device found automatically!"));
					return false;
				}
			}
		}

		_serialPort->setPortName(_deviceName);
	}

	if (!_serialPort->isOpen())
	{
		Info(_log, "Opening UART: %s", QSTRING_CSTR(_deviceName));

		_frameDropCounter = 0;

		_serialPort->setBaudRate(_baudRate_Hz);

		Debug(_log, "_serialPort.open(QIODevice::ReadWrite): %s, Baud rate [%d]bps", QSTRING_CSTR(_deviceName), _baudRate_Hz);

		QSerialPortInfo serialPortInfo(_deviceName);

		QJsonObject portInfo;
		Debug(_log, "portName:          %s", QSTRING_CSTR(serialPortInfo.portName()));
		Debug(_log, "systemLocation:    %s", QSTRING_CSTR(serialPortInfo.systemLocation()));
		Debug(_log, "description:       %s", QSTRING_CSTR(serialPortInfo.description()));
		Debug(_log, "manufacturer:      %s", QSTRING_CSTR(serialPortInfo.manufacturer()));
		Debug(_log, "productIdentifier: %s", QSTRING_CSTR(QString("0x%1").arg(serialPortInfo.productIdentifier(), 0, 16)));
		Debug(_log, "vendorIdentifier:  %s", QSTRING_CSTR(QString("0x%1").arg(serialPortInfo.vendorIdentifier(), 0, 16)));
		Debug(_log, "serialNumber:      %s", QSTRING_CSTR(serialPortInfo.serialNumber()));

		if (!serialPortInfo.isNull())
		{
			if (!_serialPort->isOpen() && !_serialPort->open(QIODevice::ReadWrite))
			{
				this->setInError(_serialPort->errorString());
				return false;
			}

			if (_espHandshake)
			{
				disconnect(_serialPort, &QSerialPort::readyRead, nullptr, nullptr);

				EspTools::initializeEsp(_serialPort, serialPortInfo, _log, _forceSerialDetection);
			}
		}
		else
		{
			QString errortext = QString("Invalid serial device name: [%1]!").arg(_deviceName);
			this->setInError(errortext);
			return false;
		}
	}

	if (delayAfterConnect_ms > 0)
	{

		Debug(_log, "delayAfterConnect for %d ms - start", delayAfterConnect_ms);

		// Wait delayAfterConnect_ms before allowing write
		QEventLoop loop;
		QTimer::singleShot(delayAfterConnect_ms, &loop, &QEventLoop::quit);
		loop.exec();

		Debug(_log, "delayAfterConnect for %d ms - finished", delayAfterConnect_ms);
	}

	return _serialPort->isOpen();
}

void ProviderSerial::setInError(const QString& errorMsg)
{
	_serialPort->clearError();
	this->close();

	LedDevice::setInError(errorMsg);
}

int ProviderSerial::writeBytes(const qint64 size, const uint8_t* data)
{
	int rc = 0;
	if (!_serialPort->isOpen())
	{
		Debug(_log, "!_serialPort.isOpen()");

		if (!tryOpen(OPEN_TIMEOUT.count()))
		{
			return -1;
		}
	}
	qint64 bytesWritten = _serialPort->write(reinterpret_cast<const char*>(data), size);
	if (bytesWritten == -1 || bytesWritten != size)
	{
		this->setInError(QString("Serial port error: %1").arg(_serialPort->errorString()));
		rc = -1;
	}
	else
	{
		if (!_serialPort->waitForBytesWritten(WRITE_TIMEOUT.count()))
		{
			if (_serialPort->error() == QSerialPort::TimeoutError)
			{
				Debug(_log, "Timeout after %dms: %d frames already dropped", WRITE_TIMEOUT, _frameDropCounter);

				++_frameDropCounter;

				// Check,if number of timeouts in a given time frame is greater than defined
				// TODO: ProviderRs232::writeBytes - Add time frame to check for timeouts that devices does not close after absolute number of timeouts
				if (_frameDropCounter > MAX_WRITE_TIMEOUTS)
				{
					this->setInError(QString("Timeout writing data to %1").arg(_deviceName));
					rc = -1;
				}
				else
				{
					//give it another try
					_serialPort->clearError();
				}
			}
			else
			{
				this->setInError(QString("Rs232 SerialPortError: %1").arg(_serialPort->errorString()));
				rc = -1;
			}
		}
	}

	if (_maxRetry > 0 && rc == -1 && !_signalTerminate)
	{
		QTimer::singleShot(2000, this, [this]() { if (!_signalTerminate) enable(); });
	}

	return rc;
}

QString ProviderSerial::discoverFirst()
{
	for (int round = 0; round < 4; round++)
		for (auto const& port : QSerialPortInfo::availablePorts())
		{
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
			if (!port.isNull() && !port.isBusy())
#else
			if (!port.isNull())
#endif
			{
				QString infoMessage = QString("%1 (%2 => %3)").arg(port.description()).arg(port.systemLocation()).arg(port.portName());
				quint16 vendor = port.vendorIdentifier();
				quint16 prodId = port.productIdentifier();
				bool knownESPA = ((vendor == 0x303a) && (prodId == 0x80c2)) ||
								 ((vendor == 0x2e8a) && (prodId == 0xa));
				bool knownESPB = (vendor == 0x303a) ||
								 ((vendor == 0x10c4) && (prodId == 0xea60)) ||
								 ((vendor == 0x1A86) && (prodId == 0x7523 || prodId == 0x55d4));
				if (round == 3 ||
					(_espHandshake && round == 0 && knownESPA) ||
					(_espHandshake && round == 1 && knownESPB) ||
					(!_espHandshake && round == 2 &&
						port.description().contains("Bluetooth", Qt::CaseInsensitive) == false &&
						port.systemLocation().contains("ttyAMA0", Qt::CaseInsensitive) == false))
				{
					Info(_log, "Serial port auto-discovery. Found serial port device: %s", QSTRING_CSTR(infoMessage));
					return port.portName();
				}
				else
				{
					Warning(_log, "Serial port auto-discovery. Skipping this device for now: %s, VID: 0x%x, PID: 0x%x", QSTRING_CSTR(infoMessage), vendor, prodId);
				}
			}
		}
	return "";
}

QJsonObject ProviderSerial::discover(const QJsonObject& /*params*/)
{
	QJsonObject devicesDiscovered;
	QJsonArray deviceList;

	deviceList.push_back(QJsonObject{
			{"value", "auto"},
			{ "name", "Auto"} });

	for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts())
	{
		deviceList.push_back(QJsonObject{
			{ "value", info.portName() },
			{ "name", QString("%2 (%1)").arg(info.systemLocation()).arg(info.description()) }
		});

#ifdef ENABLE_BONJOUR
		quint16 vendor = info.vendorIdentifier();
		quint16 prodId = info.productIdentifier();
		DiscoveryRecord newRecord;

		if ((vendor == 0x303a) && (prodId == 0x80c2))
		{
			newRecord.type = DiscoveryRecord::Service::ESP32_S2;
		}
		else if ((vendor == 0x2e8a) && (prodId == 0xa))
		{
			newRecord.type = DiscoveryRecord::Service::Pico;
		}
		else if ((vendor == 0x303a) ||
			((vendor == 0x10c4) && (prodId == 0xea60)) ||
			((vendor == 0x1A86) && (prodId == 0x7523 || prodId == 0x55d4)))
		{
			newRecord.type = DiscoveryRecord::Service::ESP;
		}

		if (newRecord.type != DiscoveryRecord::Service::Unknown)
		{			
			newRecord.hostName = info.description();
			newRecord.address = info.portName();
			newRecord.isExists = true;
			emit GlobalSignals::getInstance()->SignalDiscoveryEvent(newRecord);
		}
#endif
	}


	devicesDiscovered.insert("ledDeviceType", _activeDeviceType);
	devicesDiscovered.insert("devices", deviceList);

#ifndef ENABLE_BONJOUR
	Debug(_log, "Serial devices discovered: [%s]", QString(QJsonDocument(devicesDiscovered).toJson(QJsonDocument::Compact)).toUtf8().constData());
#endif

	return devicesDiscovered;
}
