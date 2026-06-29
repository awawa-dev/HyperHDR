#include <led-drivers/other/DriverOtherSyncLight.h>

#ifndef PCH_ENABLED
	#include <QMutexLocker>
	#include <QStringList>
	#include <QThread>
	#include <algorithm>
#endif

#if defined(_WIN32)
	#include <hidsdi.h>
	#include <setupapi.h>
#endif

namespace
{
	const QList<DriverOtherSyncLight::SupportedDevice> DEFAULT_DEVICES = {
		{ 0x1a86, 0xfe07 },
		{ 0x1a86, 0xfe0c }
	};
}

DriverOtherSyncLight::DriverOtherSyncLight(const QJsonObject& deviceConfig)
	: LedDevice(deviceConfig)
	, _idCounter(0)
	, _brightness(0xff)
	, _totalLedCount(0)
	, _controllerLedCount(DEFAULT_CONTROLLER_LED_COUNT)
	, _outputMode(OutputMode::Global)
#if defined(_WIN32)
	, _deviceHandle(INVALID_HANDLE_VALUE)
#endif
{
	_lastKeepalive.invalidate();
}

bool DriverOtherSyncLight::init(QJsonObject deviceConfig)
{
	bool initOK = LedDevice::init(deviceConfig);

	_devices = configuredDevices();
	_brightness = static_cast<quint8>(qBound(0, deviceConfig["brightness"].toInt(255), 255));
	_totalLedCount = qBound(1, static_cast<int>(_ledCount), 254);
	_controllerLedCount = qBound(1, deviceConfig["controllerLedCount"].toInt(DEFAULT_CONTROLLER_LED_COUNT), 254);
	const QString outputMode = deviceConfig["outputMode"].toString("global");
	_outputMode = OutputMode::Global;
	if (outputMode.compare("segments", Qt::CaseInsensitive) == 0)
	{
		_outputMode = OutputMode::Segments;
	}

	QStringList ids;
	for (const auto& device : _devices)
	{
		ids << QString("0x%1:0x%2")
			.arg(device.vendorId, 4, 16, QLatin1Char('0'))
			.arg(device.productId, 4, 16, QLatin1Char('0'));
	}

	const QString modeName = _outputMode == OutputMode::Segments ? "sc-segments" : "global";
	const int scAddressPairs = (_controllerLedCount + 3) / 2;
	Info(_log, "SyncLight HID devices: {:s}, brightness: {:d}, layoutLeds: {:d}, controllerLeds: {:d}, outputMode: {:s}, scAddressMax: {:d}, scAddressPairs: {:d}",
		ids.join(", "), _brightness, _totalLedCount, _controllerLedCount, modeName, _controllerLedCount, scAddressPairs);

	return initOK;
}

int DriverOtherSyncLight::open()
{
	QMutexLocker locker(&_transaction);

	_isDeviceReady = false;

#if defined(_WIN32)
	if (_deviceHandle != INVALID_HANDLE_VALUE)
	{
		_isDeviceReady = true;
		return 0;
	}

	QString error = openDeviceHandle();
	if (!error.isEmpty())
	{
		setInError(error);
		return -1;
	}

	if (!sendRb(ACTION_KEEPALIVE, QByteArray()))
	{
		closeDeviceHandle();
		setInError("SyncLight keepalive failed after opening HID device");
		return -1;
	}

	if (!sendBrightness(_brightness))
	{
		closeDeviceHandle();
		setInError("SyncLight brightness setup failed after opening HID device");
		return -1;
	}

	_lastKeepalive.restart();
	_isDeviceReady = true;
	return 0;
#else
	setInError("SyncLight HID driver is currently implemented for Windows only");
	return -1;
#endif
}

int DriverOtherSyncLight::close()
{
	QMutexLocker locker(&_transaction);
	_isDeviceReady = false;
	closeDeviceHandle();

	return 0;
}

void DriverOtherSyncLight::closeDeviceHandle()
{
	_isDeviceReady = false;

#if defined(_WIN32)
	if (_deviceHandle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(_deviceHandle);
		_deviceHandle = INVALID_HANDLE_VALUE;
	}
#endif
}

bool DriverOtherSyncLight::powerOn()
{
	QMutexLocker locker(&_transaction);
	return sendBrightness(_brightness);
}

bool DriverOtherSyncLight::powerOff()
{
	std::vector<ColorRgb> black(static_cast<size_t>(_totalLedCount), ColorRgb::BLACK);
	return writeFiniteColors(black) >= 0;
}

int DriverOtherSyncLight::writeFiniteColors(const std::vector<ColorRgb>& ledValues)
{
	QMutexLocker locker(&_transaction);

	if (ledValues.empty())
	{
		return 0;
	}

	if (_totalLedCount != static_cast<int>(ledValues.size()))
	{
		_totalLedCount = qBound(1, static_cast<int>(ledValues.size()), 254);
		Debug(_log, "SyncLight led count changed to {:d}", _totalLedCount);
	}

	bool ok = false;
	switch (_outputMode)
	{
	case OutputMode::Segments:
		ok = sendScColors(ledValues);
		break;
	case OutputMode::Global:
	default:
		ok = sendAveragedSectionColor(ledValues);
		break;
	}
	return ok ? static_cast<int>(ledValues.size()) : -1;
}

quint8 DriverOtherSyncLight::checksum(const QByteArray& frame)
{
	quint8 sum = 0;
	for (char byte : frame)
	{
		sum = static_cast<quint8>(sum + static_cast<quint8>(byte));
	}
	return sum;
}

bool DriverOtherSyncLight::parseDeviceId(const QString& text, quint16& value)
{
	bool ok = false;
	uint parsed = text.trimmed().toUInt(&ok, 0);
	if (!ok || parsed > 0xffff)
	{
		return false;
	}
	value = static_cast<quint16>(parsed);
	return true;
}

QList<DriverOtherSyncLight::SupportedDevice> DriverOtherSyncLight::configuredDevices() const
{
	QList<SupportedDevice> devices;

	quint16 vendorId = 0;
	quint16 productId = 0;
	const QString vendorText = _devConfig["VID"].toString("0x1a86");
	const QString productText = _devConfig["PID"].toString("auto");

	if (parseDeviceId(vendorText, vendorId))
	{
		if (productText.compare("auto", Qt::CaseInsensitive) == 0 || productText.trimmed().isEmpty())
		{
			for (const auto& device : DEFAULT_DEVICES)
			{
				if (device.vendorId == vendorId)
				{
					devices.push_back(device);
				}
			}
		}
		else if (parseDeviceId(productText, productId))
		{
			devices.push_back({ vendorId, productId });
		}
	}

	return devices.isEmpty() ? DEFAULT_DEVICES : devices;
}

QByteArray DriverOtherSyncLight::buildRbFrame(quint8 action, const QByteArray& payload, quint8 id)
{
	const int totalLength = RB_OVERHEAD + payload.size();
	if (totalLength > REPORT_SIZE)
	{
		return QByteArray();
	}

	QByteArray frame(totalLength, 0);
	frame[0] = 'R';
	frame[1] = 'B';
	frame[2] = static_cast<char>(totalLength);
	frame[3] = static_cast<char>(id);
	frame[4] = static_cast<char>(action);
	if (!payload.isEmpty())
	{
		std::copy(payload.cbegin(), payload.cend(), frame.begin() + 5);
	}
	frame[totalLength - 1] = static_cast<char>(checksum(frame.left(totalLength - 1)));
	return frame;
}

QByteArray DriverOtherSyncLight::buildScFrame(const std::vector<ColorRgb>& ledValues, int totalLedCount, int controllerLedCount, quint8 id)
{
	const int inputLedCount = qMin(qBound(1, totalLedCount, 254), static_cast<int>(ledValues.size()));
	const int maxAddress = qBound(1, controllerLedCount, 254);
	const int devicePositions = maxAddress + 1;
	const int maxPairs = (devicePositions + 2) / 2;
	const int addressPairs = (maxAddress + 3) / 2;
	const int segments = qMin(qBound(1, addressPairs, maxPairs), inputLedCount);
	const int frameLength = SC_HEADER_SIZE + (segments * SC_RECORD_SIZE) + SC_FOOTER_SIZE + SC_CHECKSUM_SIZE;

	QByteArray frame(frameLength, 0);
	frame[0] = 'S';
	frame[1] = 'C';
	frame[2] = static_cast<char>((frameLength >> 8) & 0xff);
	frame[3] = static_cast<char>(frameLength & 0xff);
	frame[4] = static_cast<char>(id);

	for (int segment = 0; segment < segments; ++segment)
	{
		// The SC record stores two physical positions, not a continuous range.
		const int deviceStart = qMin(segment * 2, maxAddress);
		const int deviceEnd = qMin(deviceStart + 1, maxAddress);
		const int inputStart = (deviceStart * inputLedCount) / devicePositions;
		const int inputEnd = qMax(inputStart + 1, ((deviceEnd + 1) * inputLedCount) / devicePositions);
		const ColorRgb color = averageColorRange(ledValues, inputStart, inputEnd - inputStart);

		const int offset = SC_HEADER_SIZE + (segment * SC_RECORD_SIZE);
		quint8 start = static_cast<quint8>(deviceStart);
		if (segment == 0)
		{
			start = static_cast<quint8>(start | 0x80);
		}

		frame[offset] = static_cast<char>(start);
		frame[offset + 1] = static_cast<char>(deviceEnd);
		frame[offset + 2] = static_cast<char>(color.red);
		frame[offset + 3] = static_cast<char>(color.green);
		frame[offset + 4] = static_cast<char>(color.blue);
	}

	frame[frameLength - 2] = static_cast<char>(maxAddress);
	frame[frameLength - 1] = static_cast<char>(checksum(frame.left(frameLength - 1)));
	return frame;
}

QByteArray DriverOtherSyncLight::buildReport(const QByteArray& frame)
{
	if (frame.size() > REPORT_SIZE)
	{
		return QByteArray();
	}

	QByteArray report(REPORT_SIZE + 1, 0);
	std::copy(frame.cbegin(), frame.cend(), report.begin() + 1);
	return report;
}

QByteArray DriverOtherSyncLight::buildSectionPayload(quint8 section, quint8 red, quint8 green, quint8 blue)
{
	QByteArray payload;
	payload.reserve(10);
	payload.push_back(static_cast<char>(section));
	payload.push_back(static_cast<char>(red));
	payload.push_back(static_cast<char>(green));
	payload.push_back(static_cast<char>(blue));
	payload.push_back(static_cast<char>(0x47));
	payload.push_back(static_cast<char>(0x48));
	payload.push_back(static_cast<char>(0x00));
	payload.push_back(static_cast<char>(0x00));
	payload.push_back(static_cast<char>(0x00));
	payload.push_back(static_cast<char>(0xfe));
	return payload;
}

ColorRgb DriverOtherSyncLight::averageColor(const std::vector<ColorRgb>& ledValues, int ledCount)
{
	const int count = qMin(qBound(1, ledCount, 254), static_cast<int>(ledValues.size()));
	return averageColorRange(ledValues, 0, count);
}

ColorRgb DriverOtherSyncLight::averageColorRange(const std::vector<ColorRgb>& ledValues, int offset, int count)
{
	const int first = qBound(0, offset, static_cast<int>(ledValues.size()));
	const int last = qMin(first + qMax(0, count), static_cast<int>(ledValues.size()));
	count = last - first;
	if (count <= 0)
	{
		return ColorRgb::BLACK;
	}

	uint64_t red = 0;
	uint64_t green = 0;
	uint64_t blue = 0;
	for (int i = first; i < last; ++i)
	{
		const ColorRgb& color = ledValues[static_cast<size_t>(i)];
		red += color.red;
		green += color.green;
		blue += color.blue;
	}

	return ColorRgb(
		static_cast<uint8_t>(red / static_cast<uint64_t>(count)),
		static_cast<uint8_t>(green / static_cast<uint64_t>(count)),
		static_cast<uint8_t>(blue / static_cast<uint64_t>(count)));
}

quint8 DriverOtherSyncLight::nextId()
{
	_idCounter = static_cast<quint8>(_idCounter + 1);
	if (_idCounter == 0)
	{
		_idCounter = 1;
	}
	return _idCounter;
}

bool DriverOtherSyncLight::sendRb(quint8 action, const QByteArray& payload)
{
	const QByteArray frame = buildRbFrame(action, payload, nextId());
	if (frame.isEmpty())
	{
		Error(_log, "SyncLight RB frame too large for action 0x{:02x}", static_cast<int>(action));
		return false;
	}

	return writeReport(buildReport(frame));
}

bool DriverOtherSyncLight::sendAveragedSectionColor(const std::vector<ColorRgb>& ledValues)
{
	const ColorRgb color = averageColor(ledValues, _totalLedCount);

	// This mirrors the confirmed working sequence from the original Rust UI.
	if (!sendRb(ACTION_KEEPALIVE, QByteArray()))
	{
		return false;
	}
	QThread::msleep(20);
	return sendRb(ACTION_COLOR, buildSectionPayload(SECTION_GLOBAL, color.red, color.green, color.blue));
}

bool DriverOtherSyncLight::sendScColors(const std::vector<ColorRgb>& ledValues)
{
	const QByteArray frame = buildScFrame(ledValues, _totalLedCount, _controllerLedCount, nextId());
	for (int offset = 0; offset < frame.size(); offset += REPORT_SIZE)
	{
		if (!writeReport(buildReport(frame.mid(offset, REPORT_SIZE))))
		{
			return false;
		}
	}

	return true;
}

bool DriverOtherSyncLight::sendBrightness(quint8 value)
{
	QByteArray payload;
	payload.push_back(static_cast<char>(value));
	return sendRb(ACTION_BRIGHTNESS, payload);
}

bool DriverOtherSyncLight::sendKeepaliveIfNeeded()
{
	if (!_lastKeepalive.isValid() || _lastKeepalive.elapsed() >= KEEPALIVE_INTERVAL_MS)
	{
		if (!sendRb(ACTION_KEEPALIVE, QByteArray()))
		{
			return false;
		}
		_lastKeepalive.restart();
	}
	return true;
}

bool DriverOtherSyncLight::writeReport(const QByteArray& report)
{
	if (report.size() != REPORT_SIZE + 1)
	{
		Error(_log, "Invalid SyncLight HID report size: {:d}", report.size());
		return false;
	}

#if defined(_WIN32)
	if (_deviceHandle == INVALID_HANDLE_VALUE)
	{
		Error(_log, "SyncLight HID device is not open");
		return false;
	}

	DWORD bytesWritten = 0;
	BOOL ok = WriteFile(_deviceHandle, report.constData(), static_cast<DWORD>(report.size()), &bytesWritten, nullptr);
	if (!ok || bytesWritten != static_cast<DWORD>(report.size()))
	{
		Error(_log, "SyncLight HID write failed. bytesWritten={:d}, expected={:d}", static_cast<int>(bytesWritten), report.size());
		return false;
	}
	return true;
#else
	Q_UNUSED(report)
	return false;
#endif
}

QString DriverOtherSyncLight::openDeviceHandle()
{
#if defined(_WIN32)
	GUID hidGuid;
	HidD_GetHidGuid(&hidGuid);

	HDEVINFO deviceInfo = SetupDiGetClassDevsW(&hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (deviceInfo == INVALID_HANDLE_VALUE)
	{
		return "SetupDiGetClassDevsW failed while searching for SyncLight HID device";
	}

	QString error = "SyncLight HID device not found";
	SP_DEVICE_INTERFACE_DATA interfaceData;
	interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	for (DWORD index = 0; SetupDiEnumDeviceInterfaces(deviceInfo, nullptr, &hidGuid, index, &interfaceData); ++index)
	{
		DWORD requiredSize = 0;
		SetupDiGetDeviceInterfaceDetailW(deviceInfo, &interfaceData, nullptr, 0, &requiredSize, nullptr);
		if (requiredSize == 0)
		{
			continue;
		}

		QByteArray detailBuffer(static_cast<int>(requiredSize), 0);
		auto* detailData = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(detailBuffer.data());
		detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

		if (!SetupDiGetDeviceInterfaceDetailW(deviceInfo, &interfaceData, detailData, requiredSize, nullptr, nullptr))
		{
			continue;
		}

		HANDLE handle = CreateFileW(
			detailData->DevicePath,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			nullptr);

		if (handle == INVALID_HANDLE_VALUE)
		{
			continue;
		}

		HIDD_ATTRIBUTES attributes;
		attributes.Size = sizeof(HIDD_ATTRIBUTES);
		if (HidD_GetAttributes(handle, &attributes))
		{
			const bool supported = std::any_of(_devices.cbegin(), _devices.cend(), [&attributes](const SupportedDevice& device) {
				return attributes.VendorID == device.vendorId && attributes.ProductID == device.productId;
			});

			if (supported)
			{
				_deviceHandle = handle;
				Info(_log, "Opened SyncLight HID device VID=0x{:04x} PID=0x{:04x}", static_cast<int>(attributes.VendorID), static_cast<int>(attributes.ProductID));
				SetupDiDestroyDeviceInfoList(deviceInfo);
				return QString();
			}
		}

		CloseHandle(handle);
	}

	SetupDiDestroyDeviceInfoList(deviceInfo);
	return error;
#else
	return "SyncLight HID driver is currently implemented for Windows only";
#endif
}

LedDevice* DriverOtherSyncLight::construct(const QJsonObject& deviceConfig)
{
	return new DriverOtherSyncLight(deviceConfig);
}

bool DriverOtherSyncLight::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("synclight", "leds_group_3_serial", DriverOtherSyncLight::construct);
