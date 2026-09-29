#include <led-drivers/hid/DriverHidSyncLight.h>

#ifndef PCH_ENABLED
	#include <QJsonObject>
	#include <QStringList>
	#include <QThread>
	#include <algorithm>
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

	_brightness = static_cast<quint8> (
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
	if (!sendRb (device, action_keepalive, QByteArray ()))
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


quint8 DriverHidSyncLight::checksum (const QByteArray& frame)
{
	quint8 sum = 0;
	for (char byte : frame) {
		sum = static_cast<quint8> (sum + static_cast<quint8> (byte));
	}
	return sum;
}


QByteArray DriverHidSyncLight::buildRbFrame (
		quint8 action,
		const QByteArray& payload,
		quint8 id)
{
	const int totalLength = rb_overhead + payload.size ();
	if (totalLength > report_size) {
		return {};
	}

	QByteArray frame (totalLength, 0);
	frame[0] = 'R';
	frame[1] = 'B';
	frame[2] = static_cast<char> (totalLength);
	frame[3] = static_cast<char> (id);
	frame[4] = static_cast<char> (action);
	if (!payload.isEmpty ()) {
		std::ranges::copy (payload, frame.begin () + 5);
	}
	frame[totalLength - 1] = static_cast<char> (checksum (frame.left (totalLength - 1)));
	return frame;
}


QByteArray DriverHidSyncLight::buildScFrame (
		std::span<const ColorRgb> ledValues,
		int totalLedCount,
		int controllerLedCount,
		quint8 id)
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

	QByteArray frame (frameLength, 0);
	frame[0] = 'S';
	frame[1] = 'C';
	frame[2] = static_cast<char> ((frameLength >> 8) & 0xff);
	frame[3] = static_cast<char> (frameLength & 0xff);
	frame[4] = static_cast<char> (id);

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

		const int offset = sc_header_size + segment * sc_record_size;
		quint8 start = static_cast<quint8> (deviceStart);
		if (segment == 0) {
			start = static_cast<quint8> (start | 0x80);
		}

		frame[offset] = static_cast<char> (start);
		frame[offset + 1] = static_cast<char> (deviceEnd);
		frame[offset + 2] = static_cast<char> (color.red);
		frame[offset + 3] = static_cast<char> (color.green);
		frame[offset + 4] = static_cast<char> (color.blue);
	}

	frame[frameLength - 2] = static_cast<char> (maxAddress);
	frame[frameLength - 1] = static_cast<char> (checksum (frame.left (frameLength - 1)));
	return frame;
}


QByteArray DriverHidSyncLight::buildReport (const QByteArray& frame)
{
	if (frame.size () > report_size) {
		return {};
	}

	QByteArray report (report_size + 1, 0);
	std::ranges::copy (frame, report.begin () + 1);
	return report;
}


QByteArray DriverHidSyncLight::buildSectionPayload (
		quint8 section,
		quint8 red,
		quint8 green,
		quint8 blue)
{
	QByteArray payload;
	payload.reserve (10);
	payload.push_back (static_cast<char> (section));
	payload.push_back (static_cast<char> (red));
	payload.push_back (static_cast<char> (green));
	payload.push_back (static_cast<char> (blue));
	payload.push_back (static_cast<char> (0x47));
	payload.push_back (static_cast<char> (0x48));
	payload.push_back (static_cast<char> (0x00));
	payload.push_back (static_cast<char> (0x00));
	payload.push_back (static_cast<char> (0x00));
	payload.push_back (static_cast<char> (0xfe));
	return payload;
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


quint8 DriverHidSyncLight::nextId ()
{
	_idCounter = static_cast<quint8> (_idCounter + 1);
	if (_idCounter == 0) {
		_idCounter = 1;
	}
	return _idCounter;
}


bool DriverHidSyncLight::sendRb (
		const Device& device,
		quint8 action,
		const QByteArray& payload)
{
	const QByteArray frame = buildRbFrame (action, payload, nextId ());
	if (frame.isEmpty ())
	{
		Error (_log, "SyncLight RB frame too large for action 0x{:02x}",
				static_cast<int> (action));
		return false;
	}

	return writeReport (device, buildReport (frame));
}


bool DriverHidSyncLight::sendAveragedSectionColor (
		const Device& device,
		std::span<const ColorRgb> ledValues,
		int totalLedCount)
{
	const ColorRgb color = averageColor (ledValues, totalLedCount);

	/* This mirrors the confirmed working sequence from the original Rust UI. */
	if (!sendRb (device, action_keepalive, QByteArray ())) {
		return false;
	}

	QThread::msleep (20);
	return sendRb (
			device,
			action_color,
			buildSectionPayload (section_global, color.red, color.green, color.blue));
}


bool DriverHidSyncLight::sendScColors (
		const Device& device,
		std::span<const ColorRgb> ledValues,
		int totalLedCount)
{
	const QByteArray frame = buildScFrame (
			ledValues,
			totalLedCount,
			_controllerLedCount,
			nextId ());

	for (int offset = 0; offset < frame.size (); offset += report_size)
	{
		if (!writeReport (device, buildReport (frame.mid (offset, report_size)))) {
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


bool DriverHidSyncLight::sendBrightness (const Device& device, quint8 value)
{
	QByteArray payload;
	payload.push_back (static_cast<char> (value));
	return sendRb (device, action_brightness, payload);
}


bool DriverHidSyncLight::writeReport (
		const Device& device,
		const QByteArray& report)
{
	if (report.size () != report_size + 1)
	{
		Error (_log, "Invalid SyncLight HID report size: {:d}", report.size ());
		return false;
	}

	const auto* data = reinterpret_cast<const uint8_t*> (report.constData ());
	const std::span<const uint8_t> bytes (data, static_cast<size_t> (report.size ()));
	QString error;
	if (!ProviderHid::write (device, bytes, error))
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
