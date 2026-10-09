#pragma once

#include <led-drivers/LedDevice.h>

#include <cstddef>
#include <cstdint>
#include <vector>

// Generic HID access layer shared by all HID based LED drivers (Windows: SetupAPI + hid.dll, Linux: hidraw).
// It lists and opens HID devices and moves raw reports, but knows nothing about any particular device.
// A driver derived from it only has to:
//   - selectDevices():  pick the devices it supports from everything that was found (VID/PID, usage page, serial...)
//   - initDevice():     optional handshake right after the device(s) were opened
//   - writeBlack(), writeFiniteColors(): speak the device's protocol using the report functions below
// One LED device normally is one HID device. A strip built from several identical HID devices (e.g. chained Lightpacks)
// sets _maxDevices: up to that many devices are opened together and addressed by their index (the order of devicesToOpen()).
// Which devices are opened and in which order is the driver's decision: override devicesToOpen() (default: the "output" path, else the first _maxDevices).
class ProviderHid : public LedDevice
{
	Q_OBJECT

public:
	struct HidDeviceInfo
	{
		QString path;
		QString manufacturer;
		QString product;
		QString serial;
		uint16_t vendorId = 0;
		uint16_t productId = 0;
		uint16_t usagePage = 0; // top-level collection of the HID interface (a device may expose several), 0 = unknown
		uint16_t usage = 0;
	};

	explicit ProviderHid(const QJsonObject& deviceConfig);
	~ProviderHid() override;

	// every HID device present in the system, unfiltered
	static std::vector<HidDeviceInfo> enumerateDevices();

protected:
	int open() override;
	int close() override;
	bool powerOff() override;
	QJsonObject discover(const QJsonObject& params) override;

	// the driver picks (and optionally orders) the devices it supports from everything that was found
	virtual std::vector<HidDeviceInfo> selectDevices(std::vector<HidDeviceInfo> found) = 0;

	// From the supported devices (selectDevices() result) choose the ones to open and their order = the device index of the report functions.
	// Default: the device whose path is in the "output" setting, otherwise all of them (open() stops after _maxDevices opened ones, skipping those that fail).
	// A multi-device driver (e.g. several Lightpacks forming one strip) overrides it: sort by serial, use its own config list, ...
	// and may check deviceCount() / deviceInfo(i) in initDevice() to insist on having all of them.
	virtual std::vector<HidDeviceInfo> devicesToOpen(std::vector<HidDeviceInfo> supported) = 0;

	// called once, right after a device was opened; returning false fails the open
	virtual bool initDevice() { return true; }

	// The four kinds of HID reports, hidapi conventions (devices without report IDs use ID 0):
	// output report: report ID byte followed by the data
	bool writeReport(const uint8_t* report, size_t size, size_t device = 0) { return writeData(report, size, false, device); }

	// input report of a device without report IDs: data only. Waits up to timeoutMs.
	// Returns the number of bytes read, 0 on timeout, -1 on error
	int readReport(uint8_t* report, size_t size, int timeoutMs, size_t device = 0);

	// feature report (control endpoint, Set_Report): report ID byte followed by the data
	bool writeFeature(const uint8_t* report, size_t size, size_t device = 0) { return writeData(report, size, true, device); }

	// feature report (Get_Report): report[0] must hold the report ID, the data follows. Returns the number of bytes incl. the ID, -1 on error
	int readFeature(uint8_t* report, size_t size, size_t device = 0);

	const HidDeviceInfo& deviceInfo(size_t device) const { return _opened.at(device); } // what was opened as device index 'device'
	size_t deviceCount() const { return _handles.size(); } // devices opened so far (1..._maxDevices)

	size_t _maxDevices = 1; // set by the driver (constructor): how many devices are opened together

protected slots:
	void setInError(const QString& errorMsg) override;

private:
	bool writeData(const uint8_t* report, size_t size, bool feature, size_t device);
	std::intptr_t handleOf(size_t device) const { return device < _handles.size() ? _handles[device] : -1; }
	QString openDevice(const HidDeviceInfo& device);
	void closeHandles();
	bool isOpen() const { return !_handles.empty(); }

	std::vector<HidDeviceInfo> _opened;
	std::vector<std::intptr_t> _handles; // Linux: fds, Windows: HANDLEs. Both use -1 as "invalid", so windows.h isn't needed here
};
