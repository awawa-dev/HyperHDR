#include <led-drivers/hid/ProviderHid.h>

#ifndef PCH_ENABLED
	#include <QDir>	
	#include <QFile>
	#include <QFileInfo>
	#include <QJsonArray>
	#include <QJsonDocument>
	#include <QJsonObject>
#endif

#include <algorithm>
#include <cerrno>
#include <system_error>
#include <QDirIterator>

#if defined(_WIN32)
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <windows.h>
	#include <hidsdi.h>
	#include <setupapi.h>
#else
	#include <fcntl.h>
	#include <linux/hidraw.h>
	#include <poll.h>
	#include <sys/ioctl.h>
	#include <unistd.h>
#endif

#if defined(_WIN32)
namespace
{
	// The device handle is opened for overlapped I/O, the only way to get a timeout on a HID handle.
	// Returns the number of bytes transferred, 0 on timeout, -1 on error ('error' then holds the Win32 error code)
	int transfer(HANDLE handle, bool write, void* data, DWORD size, DWORD timeoutMs, int& error)
	{
		OVERLAPPED overlapped{};
		overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		if (overlapped.hEvent == nullptr)
		{
			error = static_cast<int>(GetLastError());
			return -1;
		}

		bool ok = false, timedOut = false;
		DWORD done = 0;

		const BOOL started = write ? WriteFile(handle, data, size, nullptr, &overlapped) : ReadFile(handle, data, size, nullptr, &overlapped);
		const bool pending = !started && GetLastError() == ERROR_IO_PENDING;

		if (started || pending)
		{
			if (pending && WaitForSingleObject(overlapped.hEvent, timeoutMs) == WAIT_TIMEOUT)
			{
				timedOut = true;
				CancelIoEx(handle, &overlapped);
			}
			ok = GetOverlappedResult(handle, &overlapped, &done, TRUE) != FALSE; // also waits for a cancelled request to finish
		}

		error = ok ? 0 : static_cast<int>(timedOut ? ERROR_TIMEOUT : GetLastError());
		CloseHandle(overlapped.hEvent);

		return ok ? static_cast<int>(done) : (timedOut ? 0 : -1);
	}
}
#else
namespace
{
	QString readText(const QString& path)
	{
		QFile file(path);
		return file.open(QIODevice::ReadOnly | QIODevice::Text) ? QString::fromUtf8(file.readAll()).trimmed() : QString();
	}

	bool parseHex(const QString& text, uint16_t& value)
	{
		bool ok = false;
		const unsigned int parsed = text.trimmed().toUInt(&ok, 16);
		if (ok && parsed <= 0xffff)
			value = static_cast<uint16_t>(parsed);
		return ok && parsed <= 0xffff;
	}

	// Usage Page and Usage of the top-level collection: the first such items of the report descriptor
	void firstUsage(const uint8_t* data, size_t size, uint16_t& usagePage, uint16_t& usage)
	{
		bool hasPage = false, hasUsage = false;
		for (size_t i = 0; i < size && !(hasPage && hasUsage);)
		{
			const uint8_t prefix = data[i];
			if (prefix == 0xfe) // long item, not used for usages
			{
				i += (i + 1 < size) ? 3 + data[i + 1] : size;
				continue;
			}

			const size_t length = ((prefix & 3) == 3) ? 4 : (prefix & 3);
			uint32_t value = 0;
			for (size_t b = 0; b < length && i + 1 + b < size; ++b)
				value |= static_cast<uint32_t>(data[i + 1 + b]) << (8 * b);

			if ((prefix & 0xfc) == 0x04 && !hasPage) // Usage Page (global)
			{
				usagePage = static_cast<uint16_t>(value);
				hasPage = true;
			}
			else if ((prefix & 0xfc) == 0x08 && !hasUsage) // Usage (local)
			{
				usage = static_cast<uint16_t>(value);
				hasUsage = true;
			}
			i += 1 + length;
		}
	}
}
#endif

ProviderHid::ProviderHid(const QJsonObject& deviceConfig)
	: LedDevice(deviceConfig)
{
}

ProviderHid::~ProviderHid()
{
	closeHandles();
}

std::vector<ProviderHid::HidDeviceInfo> ProviderHid::enumerateDevices()
{
	std::vector<HidDeviceInfo> devices;

#if defined(_WIN32)
	GUID hidGuid;
	HidD_GetHidGuid(&hidGuid);

	const HDEVINFO set = SetupDiGetClassDevsW(&hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (set == INVALID_HANDLE_VALUE)
		return devices;

	SP_DEVICE_INTERFACE_DATA iface{};
	iface.cbSize = sizeof(iface);

	for (DWORD index = 0; SetupDiEnumDeviceInterfaces(set, nullptr, &hidGuid, index, &iface); ++index)
	{
		DWORD size = 0;
		SetupDiGetDeviceInterfaceDetailW(set, &iface, nullptr, 0, &size, nullptr);
		if (size == 0)
			continue;

		std::vector<uint8_t> buffer(size);
		auto* detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(buffer.data());
		detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
		if (!SetupDiGetDeviceInterfaceDetailW(set, &iface, detail, size, nullptr, nullptr))
			continue;

		// access rights 0: enough to query attributes, works even if another app has the device open
		const HANDLE handle = CreateFileW(detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
		if (handle == INVALID_HANDLE_VALUE)
			continue;

		HIDD_ATTRIBUTES attributes{};
		attributes.Size = sizeof(attributes);
		if (HidD_GetAttributes(handle, &attributes))
		{
			const auto text = [handle](auto reader) {
				wchar_t chars[128] = {};
				return reader(handle, chars, sizeof(chars)) ? QString::fromWCharArray(chars) : QString();
			};

			HidDeviceInfo info{ QString::fromWCharArray(detail->DevicePath), text(HidD_GetManufacturerString), text(HidD_GetProductString),
								text(HidD_GetSerialNumberString), attributes.VendorID, attributes.ProductID };

			PHIDP_PREPARSED_DATA preparsed = nullptr;
			if (HidD_GetPreparsedData(handle, &preparsed))
			{
				HIDP_CAPS caps{};
				if (HidP_GetCaps(preparsed, &caps) == HIDP_STATUS_SUCCESS)
				{
					info.usagePage = caps.UsagePage;
					info.usage = caps.Usage;
				}
				HidD_FreePreparsedData(preparsed);
			}
			devices.push_back(std::move(info));
		}
		CloseHandle(handle);
	}
	SetupDiDestroyDeviceInfoList(set);
#else
	QDirIterator it("/sys/class/hidraw", QDir::Dirs | QDir::NoDotAndDotDot | QDir::System);
	while (it.hasNext())
	{
		const QString node = it.next();
		const QString sysfs = QFileInfo(node + "/device").canonicalFilePath();
		if (sysfs.isEmpty())
			continue;

		HidDeviceInfo info;
		info.path = QString("/dev/%1").arg(QFileInfo(node).fileName());

		QString hidName;
		for (const QString& line : readText(sysfs + "/uevent").split('\n', Qt::SkipEmptyParts))
		{
			const int eq = line.indexOf('=');
			const QString key = line.left(eq), value = line.mid(eq + 1).trimmed();

			if (key == "HID_ID") // bus:vendor:product
			{
				const QStringList id = value.split(':');
				if (id.size() >= 3 && parseHex(id[1], info.vendorId))
					parseHex(id[2], info.productId);
			}
			else if (key == "HID_NAME")
				hidName = value;
			else if (key == "HID_UNIQ")
				info.serial = value;
		}

		QFile descriptor(sysfs + "/report_descriptor");
		if (descriptor.open(QIODevice::ReadOnly))
		{
			const QByteArray bytes = descriptor.readAll();
			firstUsage(reinterpret_cast<const uint8_t*>(bytes.constData()), static_cast<size_t>(bytes.size()), info.usagePage, info.usage);
		}

		// the USB parent (if any) knows the string descriptors
		QDir dir(sysfs);
		for (int depth = 0; depth < 8 && dir.cdUp(); ++depth)
			if (parseHex(readText(dir.filePath("idVendor")), info.vendorId) && parseHex(readText(dir.filePath("idProduct")), info.productId))
			{
				info.manufacturer = readText(dir.filePath("manufacturer"));
				info.product = readText(dir.filePath("product"));
				if (info.serial.isEmpty())
					info.serial = readText(dir.filePath("serial"));
				break;
			}

		if (info.product.isEmpty())
			info.product = hidName;

		devices.push_back(std::move(info));
	}
#endif

	return devices;
}

int ProviderHid::open()
{
	_isDeviceReady = false;

	if (!isOpen())
	{
		QString error = "No supported HID device found";
		for (const HidDeviceInfo& device : devicesToOpen(selectDevices(enumerateDevices())))
		{
			if (_handles.size() >= _maxDevices)
				break;

			const QString result = openDevice(device);
			if (result.isEmpty())
				Info(_log, "Opened HID device #{:d} {:s} VID=0x{:04x} PID=0x{:04x}", static_cast<int>(_handles.size() - 1), device.path, device.vendorId, device.productId);
			else
				error = result;
		}

		if (!isOpen())
		{
			setInError(error);
			return -1;
		}

		if (!initDevice())
		{
			setInError("HID device initialization failed");
			return -1;
		}
	}

	_isDeviceReady = true;
	return 0;
}

int ProviderHid::close()
{
	_isDeviceReady = false;
	closeHandles();
	return 0;
}

bool ProviderHid::powerOff()
{
	return writeBlack(1) >= 0;
}

void ProviderHid::setInError(const QString& errorMsg)
{
	close();
	LedDevice::setInError(errorMsg);
}

QJsonObject ProviderHid::discover(const QJsonObject& /*params*/)
{
	QJsonArray deviceList{ QJsonObject{ { "value", "auto" }, { "name", "Auto" } } };

	for (const HidDeviceInfo& d : selectDevices(enumerateDevices()))
	{
		const QString vid = QString("0x%1").arg(d.vendorId, 4, 16, QLatin1Char('0'));
		const QString pid = QString("0x%1").arg(d.productId, 4, 16, QLatin1Char('0'));

		QJsonObject device{
			{ "value", d.path },
			{ "name", QString("%1 (%2:%3)").arg(d.product.isEmpty() ? QString("HID device") : d.product, vid, pid) },
			{ "vid", vid },
			{ "pid", pid },
			{ "path", d.path } };

		if (!d.manufacturer.isEmpty())
			device["manufacturer"] = d.manufacturer;
		if (!d.product.isEmpty())
			device["product"] = d.product;
		if (!d.serial.isEmpty())
			device["serial"] = d.serial;

		deviceList.push_back(device);
	}

	QJsonObject discovered{ { "ledDeviceType", _activeDeviceType }, { "devices", deviceList } };
	Debug(_log, "HID devices discovered: [{:s}]", QString(QJsonDocument(discovered).toJson(QJsonDocument::Compact)).toUtf8().constData());
	return discovered;
}

QString ProviderHid::openDevice(const HidDeviceInfo& device)
{
#if defined(_WIN32)
	const std::intptr_t handle = reinterpret_cast<std::intptr_t>(CreateFileW(reinterpret_cast<const wchar_t*>(device.path.utf16()),
		GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr));
	const int error = static_cast<int>(GetLastError());
#else
	const std::intptr_t handle = ::open(device.path.toUtf8().constData(), O_RDWR | O_CLOEXEC);
	const int error = errno;
#endif

	if (handle == -1)
		return QString("Failed to open HID device %1: %2").arg(device.path, QString::fromStdString(std::system_category().message(error)));

	_handles.push_back(handle);
	_opened.push_back(device);
	return QString();
}

void ProviderHid::closeHandles()
{
	for (const std::intptr_t handle : _handles)
	{
#if defined(_WIN32)
		CloseHandle(reinterpret_cast<HANDLE>(handle));
#else
		::close(static_cast<int>(handle));
#endif
	}
	_handles.clear();
	_opened.clear();
}

bool ProviderHid::writeData(const uint8_t* report, size_t size, bool feature, size_t device)
{
	const std::intptr_t handle = handleOf(device);
	if (handle == -1)
	{
		Error(_log, "HID device {:d} is not open", static_cast<int>(device));
		return false;
	}

#if defined(_WIN32)
	int error = 0;
	int written = -1;
	if (feature)
	{
		const bool sent = HidD_SetFeature(reinterpret_cast<HANDLE>(handle), const_cast<uint8_t*>(report), static_cast<ULONG>(size)) != FALSE;
		written = sent ? static_cast<int>(size) : -1;
		error = sent ? 0 : static_cast<int>(GetLastError());
	}
	else
		written = transfer(reinterpret_cast<HANDLE>(handle), true, const_cast<uint8_t*>(report), static_cast<DWORD>(size), 1000, error);

	const bool ok = written == static_cast<int>(size);
#else
	ssize_t written = -1;
	do
	{
		written = feature ? ::ioctl(static_cast<int>(handle), HIDIOCSFEATURE(size), const_cast<uint8_t*>(report))
						  : ::write(static_cast<int>(handle), report, size);
	} while (written < 0 && errno == EINTR);

	const bool ok = written == static_cast<ssize_t>(size);
	const int error = written < 0 ? errno : 0;
#endif

	if (!ok)
		Error(_log, "HID {:s} report write failed. written={:d}, expected={:d}, error: {:s}", feature ? "feature" : "output",
			static_cast<int>(written), static_cast<int>(size), std::system_category().message(error).c_str());

	return ok;
}

int ProviderHid::readReport(uint8_t* report, size_t size, int timeoutMs, size_t device)
{
	const std::intptr_t handle = handleOf(device);
	if (handle == -1)
		return -1;

#if defined(_WIN32)
	// ReadFile returns the whole input report including the report ID (0 here): drop it to get the same data as on Linux
	std::vector<uint8_t> buffer(size + 1);
	int error = 0;
	const int length = transfer(reinterpret_cast<HANDLE>(handle), false, buffer.data(), static_cast<DWORD>(buffer.size()), static_cast<DWORD>(timeoutMs), error);
	if (length <= 0)
		return length;

	std::copy(buffer.begin() + 1, buffer.begin() + length, report);
	return length - 1;
#else
	pollfd descriptor{ static_cast<int>(handle), POLLIN, 0 };
	int ready;
	do
	{
		ready = ::poll(&descriptor, 1, timeoutMs);
	} while (ready < 0 && errno == EINTR);

	if (ready <= 0)
		return ready; // 0: timeout

	ssize_t length;
	do
	{
		length = ::read(static_cast<int>(handle), report, size);
	} while (length < 0 && errno == EINTR);

	return static_cast<int>(length);
#endif
}

int ProviderHid::readFeature(uint8_t* report, size_t size, size_t device)
{
	const std::intptr_t handle = handleOf(device);
	if (handle == -1)
		return -1;

#if defined(_WIN32)
	return HidD_GetFeature(reinterpret_cast<HANDLE>(handle), report, static_cast<ULONG>(size)) ? static_cast<int>(size) : -1;
#else
	int length;
	do
	{
		length = ::ioctl(static_cast<int>(handle), HIDIOCGFEATURE(size), report);
	} while (length < 0 && errno == EINTR);

	return length;
#endif
}
