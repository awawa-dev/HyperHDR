#include <led-drivers/hid/DriverHidSyncLight.h>

#ifndef PCH_ENABLED
	#include <QJsonObject>
	#include <QStringList>
	#include <QThread>
	#include <algorithm>
	#include <numeric>
#endif


DriverHidSyncLight::~DriverHidSyncLight ()
{
	/* LedDevice shutdown normally powers the device off before closing it.
	 * Keep the destructor fallback used by the original SyncLight driver. */
	//TODO: shutdown with taskbar icon should use normal shutdown process
	ProviderHid::powerOff ();
}


bool DriverHidSyncLight::init (QJsonObject deviceConfig)
{
	if (!ProviderHid::init (deviceConfig)) {
		return false;
	}

	_brightness = static_cast<uint8_t> (
			qBound (0, deviceConfig["brightness"].toInt (255), 255));
	_totalLedCount = qBound (1, static_cast<int> (_ledCount), static_cast<int> (maximum_led_count));
	_controllerLedCount = qBound (
			1,
			deviceConfig["controllerLedCount"].toInt (default_controller_led_count),
			static_cast<int> (maximum_led_count));

	_outputMode = OutputMode::Global;
	QString modeName = QStringLiteral ("global");
	const QString outputMode = deviceConfig["outputMode"].toString ("global");
	if (outputMode.compare ("segments", Qt::CaseInsensitive) == 0)
	{
		_outputMode = OutputMode::PerLed;
		modeName = QStringLiteral ("per-led");
	}

	QStringList ids;
	for (const auto& device : getSupportedDeviceIds ())
	{
		ids << QString ("0x%1:0x%2")
			.arg (device.vendorId, 4, 16, QLatin1Char ('0'))
			.arg (device.productId, 4, 16, QLatin1Char ('0'));
	}

	const int scAddressPairs = (_controllerLedCount + 3) / 2;
	Info (_log, "SyncLight HID devices: {:s}, brightness: {:d}, layoutLeds: {:d}, controllerLeds: {:d}, outputMode: {:s}, scAddressMax: {:d}, scAddressPairs: {:d}",
			ids.join (", "),
			_brightness,
			_totalLedCount,
			_controllerLedCount,
			modeName,
			_controllerLedCount,
			scAddressPairs);

	return true;
}


bool DriverHidSyncLight::init_device (const Device& device)
{
	if (!sendKeepalive (device))
	{
		setInError (QString ("SyncLight keepalive failed after opening HID device %1")
				.arg (QString::fromStdString (device.path)));
		return false;
	}

	if (!sendBrightness (device, _brightness))
	{
		setInError (QString ("SyncLight brightness setup failed after opening HID device %1")
				.arg (QString::fromStdString (device.path)));
		return false;
	}

	return true;
}


bool DriverHidSyncLight::powerOn (const Device& device)
{
	return sendBrightness (device, _brightness);
}


bool DriverHidSyncLight::powerOff (const Device& device)
{
	return sendBlackFrame (device);
}


int DriverHidSyncLight::writeFiniteColors (
		const Device& device,
		std::span<const ColorRgb> ledValues)
{
	if (ledValues.empty ()) {
		return 0;
	}

	const int totalLedCount = qBound (
			1,
			static_cast<int> (ledValues.size ()),
			static_cast<int> (maximum_led_count));

	bool success = false;
	switch (_outputMode)
	{
		case OutputMode::PerLed:
			success = sendScColors (device, ledValues, totalLedCount);
			break;
		case OutputMode::Global:
		default:
			success = sendAveragedSectionColor (device, ledValues, totalLedCount);
			break;
	}

	if (!success)
	{
		setInError (QString ("Could not write colors to SyncLight %1")
				.arg (QString::fromStdString (device.path)));
		return -1;
	}

	return 0;
}


uint8_t DriverHidSyncLight::checksum (std::span<const uint8_t> data)
{
	return static_cast<uint8_t> (
			std::reduce (data.begin (), data.end (), uint32_t {0}));
}


bool DriverHidSyncLight::buildRbFrame (
		HidReport& report,
		uint8_t action,
		std::span<const uint8_t> payload,
		uint8_t id)
{
	const size_t totalLength = rb_overhead + payload.size ();
	if (totalLength > report_size) {
		return false;
	}

	report = {};
	report[1] = 'R';
	report[2] = 'B';
	report[3] = static_cast<uint8_t> (totalLength);
	report[4] = id;
	report[5] = action;
	if (!payload.empty ()) {
		std::ranges::copy (payload, report.begin () + 6);
	}
	const std::span<const uint8_t> checksumData (
			report.data () + 1,
			totalLength - 1);
	report[totalLength] = checksum (checksumData);
	return true;
}


std::vector<uint8_t> DriverHidSyncLight::buildScFrame (
		std::span<const ColorRgb> ledValues,
		int totalLedCount,
		int controllerLedCount,
		uint8_t id)
{
	const int inputLedCount = qMin (
			qBound (1, totalLedCount, static_cast<int> (maximum_led_count)),
			static_cast<int> (ledValues.size ()));
	const int maxAddress = qBound (
			1,
			controllerLedCount,
			static_cast<int> (maximum_led_count));
	const int devicePositions = maxAddress + 1;
	const int maxPairs = (devicePositions + 2) / 2;
	const int addressPairs = (maxAddress + 3) / 2;
	const int segments = qMin (qBound (1, addressPairs, maxPairs), inputLedCount);
	const int frameLength = sc_header_size
			+ segments * sc_record_size
			+ sc_footer_size
			+ sc_checksum_size;
	const size_t frameSize = static_cast<size_t> (frameLength);

	std::vector<uint8_t> frame (frameSize, 0);
	frame[0] = 'S';
	frame[1] = 'C';
	frame[2] = static_cast<uint8_t> ((frameLength >> 8) & 0xff);
	frame[3] = static_cast<uint8_t> (frameLength & 0xff);
	frame[4] = id;

	for (int segment = 0; segment < segments; ++segment)
	{
		/* The SC record stores two physical positions, not a range. */
		const int deviceStart = qMin (segment * 2, maxAddress);
		const int deviceEnd = qMin (deviceStart + 1, maxAddress);
		const int inputStart = deviceStart * inputLedCount / devicePositions;
		const int inputEnd = qMax (
				inputStart + 1,
				(deviceEnd + 1) * inputLedCount / devicePositions);
		const ColorRgb color = averageColorRange (
				ledValues,
				inputStart,
				inputEnd - inputStart);

		const size_t offset = static_cast<size_t> (
				sc_header_size + segment * sc_record_size);
		uint8_t start = static_cast<uint8_t> (deviceStart);
		if (segment == 0) {
			start = static_cast<uint8_t> (start | 0x80);
		}

		frame[offset + 0] = start;
		frame[offset + 1] = static_cast<uint8_t> (deviceEnd);
		frame[offset + 2] = color.red;
		frame[offset + 3] = color.green;
		frame[offset + 4] = color.blue;
	}

	frame[frameSize - 2] = static_cast<uint8_t> (maxAddress);
	const std::span<const uint8_t> checksumData (
			frame.data (),
			frameSize - 1);
	frame[frameSize - 1] = checksum (checksumData);
	return frame;
}


std::array<uint8_t, 10> DriverHidSyncLight::buildSectionPayload (
		uint8_t section,
		uint8_t red,
		uint8_t green,
		uint8_t blue)
{
	return {
		section,
		red,
		green,
		blue,
		0x47,
		0x48,
		0x00,
		0x00,
		0x00,
		0xfe,
	};
}


ColorRgb DriverHidSyncLight::averageColor (
		std::span<const ColorRgb> ledValues,
		int ledCount)
{
	const int count = qMin (
			qBound (1, ledCount, static_cast<int> (maximum_led_count)),
			static_cast<int> (ledValues.size ()));
	return averageColorRange (ledValues, 0, count);
}


ColorRgb DriverHidSyncLight::averageColorRange (
		std::span<const ColorRgb> ledValues,
		int offset,
		int count)
{
	const int first = qBound (0, offset, static_cast<int> (ledValues.size ()));
	const int last = qMin (
			first + qMax (0, count),
			static_cast<int> (ledValues.size ()));
	count = last - first;
	if (count <= 0) {
		return ColorRgb::BLACK;
	}

	uint64_t red = 0;
	uint64_t green = 0;
	uint64_t blue = 0;
	for (int i = first; i < last; ++i)
	{
		const ColorRgb& color = ledValues[static_cast<size_t> (i)];
		red += color.red;
		green += color.green;
		blue += color.blue;
	}

	return ColorRgb (
			static_cast<uint8_t> (red / static_cast<uint64_t> (count)),
			static_cast<uint8_t> (green / static_cast<uint64_t> (count)),
			static_cast<uint8_t> (blue / static_cast<uint64_t> (count)));
}


uint8_t DriverHidSyncLight::nextId ()
{
	_idCounter = static_cast<uint8_t> (_idCounter + 1);
	if (_idCounter == 0) {
		_idCounter = 1;
	}
	return _idCounter;
}


bool DriverHidSyncLight::sendRb (
		const Device& device,
		const HidReport& report)
{
	return writeReport (device, report);
}


bool DriverHidSyncLight::sendKeepalive (const Device& device)
{
	HidReport report {};

	if (!buildRbFrame (report, action_keepalive, {}, nextId ())) {
		return false;
	}

	return sendRb (device, report);
}


bool DriverHidSyncLight::sendAveragedSectionColor (
		const Device& device,
		std::span<const ColorRgb> ledValues,
		int totalLedCount)
{
	const ColorRgb color = averageColor (ledValues, totalLedCount);

	/* This mirrors the confirmed working sequence from the original Rust UI. */
	if (!sendKeepalive (device)) {
		return false;
	}

	QThread::msleep (20);
	HidReport report {};
	const std::array<uint8_t, 10> payload = buildSectionPayload (
			section_global,
			color.red,
			color.green,
			color.blue);
	if (!buildRbFrame (
			report,
			action_color,
			payload,
			nextId ()))
	{
		return false;
	}

	return sendRb (device, report);
}


bool DriverHidSyncLight::sendScColors (
		const Device& device,
		std::span<const ColorRgb> ledValues,
		int totalLedCount)
{
	const std::vector<uint8_t> frame = buildScFrame (
			ledValues,
			totalLedCount,
			_controllerLedCount,
			nextId ());

	for (size_t offset = 0; offset < frame.size (); offset += report_size)
	{
		const size_t chunkSize = std::min (report_size, frame.size () - offset);
		const std::span<const uint8_t> chunk (frame.data () + offset, chunkSize);
		HidReport report {};
		std::ranges::copy (chunk, report.begin () + 1);

		if (!writeReport (device, report)) {
			return false;
		}
	}

	return true;
}


bool DriverHidSyncLight::sendBlackFrame (const Device& device)
{
	int blackLedCount = _totalLedCount;
	if (_outputMode == OutputMode::PerLed) {
		blackLedCount = qMax (_totalLedCount, (_controllerLedCount + 3) / 2);
	}

	std::vector<ColorRgb> black (
			static_cast<size_t> (qBound (
					1,
					blackLedCount,
					static_cast<int> (maximum_led_count))),
			ColorRgb::BLACK);
	const std::span<const ColorRgb> blackColors (black);

	if (_outputMode == OutputMode::PerLed) {
		return sendScColors (device, blackColors, static_cast<int> (black.size ()));
	}

	return sendAveragedSectionColor (
			device,
			blackColors,
			static_cast<int> (black.size ()));
}


bool DriverHidSyncLight::sendBrightness (const Device& device, uint8_t value)
{
	const std::array<uint8_t, 1> payload = { value };
	HidReport report {};

	if (!buildRbFrame (report, action_brightness, payload, nextId ())) {
		return false;
	}

	return sendRb (device, report);
}


bool DriverHidSyncLight::writeReport (
		const Device& device,
		const HidReport& report)
{
	QString error;
	if (!ProviderHid::write (device, report, error))
	{
		Error (_log, "SyncLight HID write failed for {:s}: {:s}",
				QString::fromStdString (device.path),
				error);
		return false;
	}

	return true;
}


LedDevice* DriverHidSyncLight::construct (const QJsonObject& deviceConfig)
{
	return new DriverHidSyncLight (deviceConfig);
}


bool DriverHidSyncLight::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE (
		"synclight",
		"leds_group_3_serial",
		DriverHidSyncLight::construct);
