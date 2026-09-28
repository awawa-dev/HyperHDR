#include <led-drivers/hid/ProviderHid.h>
#include <linalg.h>

#ifndef PCH_ENABLED
	#include <algorithm>
#endif


const QString ProviderHid::all_devices_serial = QStringLiteral ("<all>");


ProviderHid::ProviderHid (const QJsonObject& deviceConfig)
: LedDevice (deviceConfig)
{
}


ProviderHid::~ProviderHid ()
{
	close();
}


QJsonObject ProviderHid::discover (const QJsonObject& /*params*/)
{
	QJsonArray json;
	const auto devices = enumerate ();

	/* Insert the "<all>" 'device' first. */
	if (devices && !devices->empty ())
	{
		json.push_back (QJsonObject {
				{ "value", all_devices_serial },
				{
					"name",
					QString ("All detected devices (%1 devices)")
						.arg (devices->size ())
				},
		});
	}

	if (devices)
	{
		for (const auto& device : *devices)
		{
			QString serial = device.serial;
			QString path = QString::fromStdString (device.path);

			QString value = serial;
			if (value.isEmpty ()) {
				value = path;
			}

			QString vid = QString ("0x%1")
					.arg(device.vendorId, 4, 16, QLatin1Char('0'));

			QString pid = QString ("0x%1")
					.arg(device.productId, 4, 16, QLatin1Char('0'));

			QString name = QString ("%1 %2 (%3:%4)")
				.arg (QString::fromStdString (getDeviceName ()))
				.arg (value)
				.arg (vid)
				.arg (pid);

			json.push_back (QJsonObject {
				{ "value", value },
				{ "name", name },
				{ "serial", serial },
				{ "vid", vid },
				{ "pid", pid },
				{ "path", path },
			});
		}
	}

	QJsonObject result {
		{ "ledDeviceType", _activeDeviceType },
		{ "devices", json }
	};

	Debug (_log, "HID devices discovered: [{:s}]", QString (QJsonDocument (result).toJson (QJsonDocument::Compact)));

	return result;
}


bool ProviderHid::init (QJsonObject deviceConfig)
{
	if (!LedDevice::init (deviceConfig)) {
		return false;
	}

	serial = deviceConfig["serial"].toString ().trimmed ();
	if (serial.isEmpty ()) {
		serial = all_devices_serial;
	}

	if (hid_init() == -1) {
		Error (_log, "Could not initialize hidapi: {:s}", getHidError ());
		return false;
	}

	return true;
}


int ProviderHid::open ()
{
	bool success = true;
	size_t ledCapacity = 0;

	/* Close all already-open devices first. */
	close ();

	const auto devices = enumerate ();
	if (!devices)
	{
		setInError (QStringLiteral ("Could not enumerate supported HID devices"));
		success = false;
		goto open_done;
	}

	open_devices.reserve (devices->size ());

	for (const auto& device_info : *devices)
	{
		/* Only process configured devices. */
		if (serial != all_devices_serial
				&& serial != device_info.serial
				&& serial != QString::fromStdString (device_info.path))
		{
			continue;
		}

		auto device = open_device (device_info);

		if (!device) {
			success = false;
			goto open_done;
		}

		open_devices.push_back (std::move (*device));

		success = init_device (open_devices.back ());
		if (!success) {
			goto open_done;
		}
	}

	if (open_devices.empty ())
	{
		if (serial == all_devices_serial) {
			setInError (QStringLiteral ("No supported HID devices found"));
		} else {
			setInError (QString ("HID device with serial '%1' not found")
					.arg (serial));
		}

		success = false;
		goto open_done;
	}

	for (const auto& device : open_devices)
	{
		ledCapacity += getLedCount (device);
	}

	if (_ledCount > ledCapacity)
	{
		setInError (QString ("Configured for %1 LEDs, but %2 HID device(s) provide only %3")
				.arg (_ledCount)
				.arg (open_devices.size ())
				.arg (ledCapacity));
		success = false;
	}
	else
	{
		_customInfo = QString (" (%1 device(s))").arg (open_devices.size ());
		Info (_log, "Opened {:d} {:s}(s) for {:d} LEDs",
				open_devices.size (), getDeviceName (), _ledCount);

		for (size_t i = 0; i < open_devices.size (); ++i)
		{
			const auto& device = open_devices[i];
			QString identifier;
			if (device.serial.isEmpty ()) {
				identifier = QString::fromStdString (device.path);
			} else {
				identifier = device.serial;
			}
			Info (_log, "{:s} {:d}: {:s}", getDeviceName (), i + 1, identifier);
		}
	}

open_done:

	if (!success) {
		close ();
		setupRetry (retry_interval);
		return -1;
	}

	_isDeviceReady = true;
	return 0;
}


std::optional<ProviderHid::Device> ProviderHid::open_device (const Device& deviceInfo)
{
	Device device = deviceInfo;
	device.handle = hid_open_path (device.path.c_str());
	if (!device.handle)
	{
		setInError (QString ("Could not open HID device %1: %2")
				.arg (QString::fromStdString (device.path), getHidError ()));
		return std::nullopt;
	}

	return device;
}


int ProviderHid::close ()
{
	_isDeviceReady = false;

	for (const auto& device : open_devices) {
		if (device.handle) {
			hid_close (device.handle);
		}
	}

	open_devices.clear ();

	return 0;
}


void ProviderHid::setInError (const QString& errorMsg)
{
	close ();
	LedDevice::setInError (errorMsg);
}


bool ProviderHid::powerOn ()
{
	bool success = true;

	for (const auto& device : open_devices)
	{
		if (!powerOn (device))
		{
			success = false;
		}
	}
	return success;
}


bool ProviderHid::powerOff ()
{
	bool success = true;

	for (const auto& device : open_devices)
	{
		if (!powerOff (device))
		{
			success = false;
		}
	}

	return success;
}


std::pair<bool, int> ProviderHid::writeInfiniteColors (SharedOutputColors nonlinearRgbColors)
{
	size_t offset = 0;
	size_t total_leds = 0;

	for (const auto& device : open_devices)
	{
		total_leds += getLedCount (device);
	}

	if (nonlinearRgbColors->size () > total_leds)
	{
		setInError (QString ("Received %1 LED colors, but the connected HID devices support only %2")
				.arg (nonlinearRgbColors->size ())
				.arg (total_leds));
		return {true, -1};
	}

	for (const auto& device : open_devices)
	{
		const size_t leds = getLedCount (device);
		size_t color_count = 0;
		std::span<const linalg::aliases::float3> colors;

		if (offset < nonlinearRgbColors->size ())
		{
			color_count = std::min (leds, nonlinearRgbColors->size () - offset);
			colors = std::span<const linalg::aliases::float3> (
					nonlinearRgbColors->data () + offset,
					color_count);
		}

		const auto [handled, status] = writeInfiniteColors (device, colors);

		if (!handled)
		{
			return {handled, status};
		}

		if (status != 0)
		{
			setupRetry (retry_interval);
			return {handled, status};
		}

		offset += leds;
	}

	return {true, 0};
}


int ProviderHid::writeFiniteColors (const std::vector<ColorRgb>& ledValues)
{
	size_t offset = 0;
	size_t total_leds = 0;

	for (const auto& device : open_devices)
	{
		total_leds += getLedCount (device);
	}

	if (ledValues.size () > total_leds)
	{
		setInError (QString ("Received %1 LED colors, but the connected HID devices support only %2")
				.arg (ledValues.size ())
				.arg (total_leds));
		return -1;
	}

	for (const auto& device : open_devices)
	{
		const size_t leds = getLedCount (device);
		size_t color_count = 0;
		std::span<const ColorRgb> colors;

		if (offset < ledValues.size ())
		{
			color_count = std::min (leds, ledValues.size () - offset);
			colors = std::span<const ColorRgb> (
					ledValues.data () + offset,
					color_count);
		}

		const int status = writeFiniteColors (device, colors);
		if (status != 0)
		{
			setupRetry (retry_interval);
			return status;
		}

		offset += leds;
	}

	return 0;
}


std::optional<std::vector<ProviderHid::Device>> ProviderHid::enumerate () const
{
	std::vector<Device> devices;

	if (hid_init () == -1)
	{
		Error (_log, "Could not initialize hidapi: {:s}", getHidError ());
		return std::nullopt;
	}

	for (const auto& id : getSupportedDeviceIds ())
	{
		if (!enumerateForDeviceId (devices, id))
		{
			/* We assume that if enumeration fails with
			 * one device id, it will fail for all. */
			return std::nullopt;
		}
	}

	std::ranges::sort (devices,
			[] (const auto& left, const auto& right)
			{
				/* If one serial is empty, the one with serial goes first */
				if (left.serial.isEmpty() != right.serial.isEmpty()) {
					return !left.serial.isEmpty();
				}

				/* If the serials are different */
				if (left.serial != right.serial) {
					return left.serial < right.serial;
				}

				/* Fall back to path if neither device has a serial
				 * or they are identical */
				return left.path < right.path;
			}
	);

	return devices;
}


bool ProviderHid::enumerateForDeviceId (
		std::vector<Device>& info,
		const DeviceId& id) const
{
	bool success = true;
	struct hid_device_info* devices = hid_enumerate (id.vendorId, id.productId);

	if (!devices)
	{
		const QString error = getHidError ();
		if (error == QStringLiteral ("unknown HID error")
				|| error == QStringLiteral ("hid_error is not implemented yet"))
		{
			/* No devices found */
		}
		else
		{
			success = false;
			Error (_log, "Could not enumerate HID devices VID=0x{:04x} PID=0x{:04x}: {:s}",
					static_cast<int> (id.vendorId),
					static_cast<int> (id.productId),
					error);
			goto enumerateForDeviceId_done;
		}
	}

	for (auto const * device = devices; device != nullptr; device = device->next)
	{
		if (device->path == nullptr) {
			Warning (_log, "Ignoring HID device VID=0x{:04x} PID=0x{:04x} without a path",
					static_cast<int> (device->vendor_id),
					static_cast<int> (device->product_id));
			continue;
		}

		QString serial;
		if (device->serial_number) {
			serial = QString::fromWCharArray (device->serial_number);
		}

		Device infoEntry {
			.path=std::string (device->path),
			.vendorId=device->vendor_id,
			.productId=device->product_id,
			.serial=std::move (serial),
		};

		info.push_back (std::move (infoEntry));
	}

enumerateForDeviceId_done:

	if (devices) {
		hid_free_enumeration (devices);
	}

	return success;
}


QString ProviderHid::getHidError ()
{
	const wchar_t* error = hid_error (nullptr);
	if (!error) {
		return QStringLiteral ("unknown HID error");
	}

	return QString::fromWCharArray (error);
}


QString ProviderHid::getHidError (const Device& device)
{
	const wchar_t* error = hid_error (device.handle);
	if (!error) {
		return QStringLiteral ("unknown HID error");
	}

	return QString::fromWCharArray (error);
}


bool ProviderHid::writeReport (
		const Device& device,
		std::span<const uint8_t> report,
		QString& error,
		bool send)
{
	if (!device.handle) {
		error = QStringLiteral ("Invalid HID device handle");
		return false;
	}

	if (report.empty()) {
		error = QStringLiteral ("Empty HID report");
		return false;
	}

	int written = 0;

	if (send) {
		written = hid_send_output_report (device.handle, report.data (), report.size ());
	} else {
		written = hid_write (device.handle, report.data (), report.size ());
	}

	if (written != static_cast<int> (report.size ())) {
		error = QStringLiteral ("Incomplete write: %1").arg (getHidError (device));
		return false;
	}

	return true;
}
