#include <led-drivers/hid/DriverHidSyncLight.h>

#ifndef PCH_ENABLED
	#include <QJsonObject>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <limits>
#include <numeric>
#include <thread>

namespace
{
	constexpr size_t REPORT_SIZE = 64; // HID report payload; writeReport() gets it prefixed with the report ID (0)
	constexpr size_t MAX_LEDS = 254;	
	constexpr uint8_t ACTION_COLOR = 0x86;
	constexpr uint8_t ACTION_READ_INFO = 0x82;
	constexpr uint8_t ACTION_BRIGHTNESS = 0x87;
	constexpr uint8_t ACTION_STOP_EFFECT = 0x97; // disables the device-side animations

	// the last byte of every frame is the sum of all the previous ones
	void appendCrc(std::vector<uint8_t>& frame)
	{
		frame.back() = std::accumulate(frame.begin(), frame.end() - 1, uint8_t{ 0 });
	}

	std::vector<uint8_t> buildRbFrame(uint8_t action, std::initializer_list<uint8_t> payload, uint8_t id)
	{
		std::vector<uint8_t> frame{ 'R', 'B', static_cast<uint8_t>(6 + payload.size()), id, action };
		frame.insert(frame.end(), payload.begin(), payload.end());
		frame.push_back(0);
		appendCrc(frame);
		return frame;
	}

	ColorRgb averageColor(const ColorRgb* first, const ColorRgb* last)
	{
		if (first == last)
			return ColorRgb::BLACK;

		size_t red = 0, green = 0, blue = 0;
		for (const ColorRgb* color = first; color != last; ++color)
		{
			red += color->red;
			green += color->green;
			blue += color->blue;
		}
		const size_t count = static_cast<size_t>(last - first);
		return ColorRgb(static_cast<uint8_t>(red / count), static_cast<uint8_t>(green / count), static_cast<uint8_t>(blue / count));
	}

	// A run of adjacent LEDs that is sent as one color
	struct Range
	{
		int first, last; // LEDs are numbered from 1
		uint64_t count;
		std::array<uint64_t, 3> sum;
		double energy; // |sum|^2 / count: what a single average color can still represent of the run
	};

	Range makeRange(int first, int last, uint64_t count, const std::array<uint64_t, 3>& sum)
	{
		return { first, last, count, sum, static_cast<double>(sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]) / static_cast<double>(count) };
	}

	Range merge(const Range& a, const Range& b)
	{
		return makeRange(a.first, b.last, a.count + b.count, { a.sum[0] + b.sum[0], a.sum[1] + b.sum[1], a.sum[2] + b.sum[2] });
	}

	// SC frame: "SC", length (big endian), id, 0x80, ranges of {first LED, R, G, B, last LED}, checksum.
	// One color per controller LED goes in, at most MAX_RANGES ranges come out: the adjacent pair whose merge
	// adds the least squared color error is merged first (leftmost on ties).
	std::vector<uint8_t> buildScFrame(const std::vector<ColorRgb>& colors, uint8_t id, int controllerSegmentLimit)
	{
		std::vector<Range> ranges;
		for (size_t i = 0; i < colors.size(); ++i)
			ranges.push_back(makeRange(static_cast<int>(i) + 1, static_cast<int>(i) + 1, 1, { colors[i].red, colors[i].green, colors[i].blue }));

		while (ranges.size() > controllerSegmentLimit)
		{
			size_t best = 0;
			double bestCost = std::numeric_limits<double>::infinity();
			for (size_t i = 0; i + 1 < ranges.size(); ++i)
			{
				const double cost = ranges[i].energy + ranges[i + 1].energy - merge(ranges[i], ranges[i + 1]).energy;
				if (cost < bestCost)
				{
					bestCost = cost;
					best = i;
				}
			}
			ranges[best] = merge(ranges[best], ranges[best + 1]);
			ranges.erase(ranges.begin() + static_cast<std::ptrdiff_t>(best) + 1);
		}

		const size_t length = 7 + 5 * ranges.size();
		std::vector<uint8_t> frame{ 'S', 'C', static_cast<uint8_t>(length >> 8), static_cast<uint8_t>(length & 0xff), id, 0x80 };
		for (const Range& r : ranges)
		{
			const auto average = [&r](size_t channel) { return static_cast<uint8_t>((2 * r.sum[channel] + r.count) / (2 * r.count)); }; // rounded
			frame.insert(frame.end(), { static_cast<uint8_t>(r.first), average(0), average(1), average(2), static_cast<uint8_t>(r.last) });
		}
		frame.push_back(0);
		appendCrc(frame);
		return frame;
	}

	struct SupportedDevice
	{
		uint16_t vendorId;
		uint16_t productId;
	};

	const std::vector<SupportedDevice> DEFAULT_DEVICES{ { 0x1a86, 0xfe07 }, { 0x1a86, 0xfe0c } };

	bool parseId(const QString& text, uint16_t& value)
	{
		bool ok = false;
		const unsigned int parsed = text.trimmed().toUInt(&ok, 0);
		if (ok && parsed <= 0xffff)
			value = static_cast<uint16_t>(parsed);
		return ok && parsed <= 0xffff;
	}

	// "VID" + "PID" from the config ("auto" = every default device of that vendor), defaults if nothing valid is configured
	std::vector<SupportedDevice> configuredDevices(const QJsonObject& config)
	{
		std::vector<SupportedDevice> devices;
		uint16_t vid = 0, pid = 0;
		const QString pidText = config["PID"].toString("auto").trimmed();

		if (parseId(config["VID"].toString("0x1a86"), vid))
		{
			if (pidText.isEmpty() || pidText.compare("auto", Qt::CaseInsensitive) == 0)
				std::copy_if(DEFAULT_DEVICES.begin(), DEFAULT_DEVICES.end(), std::back_inserter(devices), [vid](const SupportedDevice& d) { return d.vendorId == vid; });
			else if (parseId(pidText, pid))
				devices.push_back({ vid, pid });
		}

		return devices.empty() ? DEFAULT_DEVICES : devices;
	}
}

DriverHidSyncLight::DriverHidSyncLight(const QJsonObject& deviceConfig)
	: ProviderHid(deviceConfig)
{
}

bool DriverHidSyncLight::init(QJsonObject deviceConfig)
{
	if (!ProviderHid::init(deviceConfig))
		return false;

	_brightness = static_cast<uint8_t>(std::clamp(deviceConfig["brightness"].toInt(255), 1, 255)); // the device reads 0 as maximum
	_controllerLedCount = std::clamp(deviceConfig["controllerLedCount"].toInt(0), 0, 254);
	_controllerSegmentLimit = std::clamp(deviceConfig["controllerSegmentLimit"].toInt(MAX_RANGES), 1, 254);
	_outputMode = deviceConfig["outputMode"].toString("global").compare("segments", Qt::CaseInsensitive) == 0 ? OutputMode::PerLed : OutputMode::Global;

	_output = deviceConfig["output"].toString(deviceConfig["path"].toString("auto")).trimmed();

	Debug(_log, "DeviceType: {:s}, LedCount: {:d}, HID output: {:s}", this->getActiveDeviceType(), this->getLedCount(), _output);
	Debug(_log, "SyncLight brightness: {:d}, hardware leds number: {:d}, segments: {:d}, output mode: {:s}",
		_brightness, _controllerLedCount, _controllerSegmentLimit, (_outputMode == OutputMode::PerLed) ? "per-led" : "global");

	return true;
}

std::vector<ProviderHid::HidDeviceInfo> DriverHidSyncLight::selectDevices(std::vector<HidDeviceInfo> found)
{
	const std::vector<SupportedDevice> supported = configuredDevices(_devConfig);

	const auto unsupported = [&supported](const HidDeviceInfo& d) {
		// the strip exposes two HID interfaces with the same VID/PID (the other one is a keyboard): only 0xFF00/1 takes lighting commands
		return d.usagePage != 0xff00 || d.usage != 1
			|| std::none_of(supported.begin(), supported.end(), [&d](const SupportedDevice& s) { return s.vendorId == d.vendorId && s.productId == d.productId; });
	};

	found.erase(std::remove_if(found.begin(), found.end(), unsupported), found.end());
	return found;
}

std::vector<ProviderHid::HidDeviceInfo> DriverHidSyncLight::devicesToOpen(std::vector<HidDeviceInfo> supported)
{
	const auto chosen = std::find_if(supported.begin(), supported.end(), [this](const HidDeviceInfo& d) { return d.path == _output; });
	return chosen == supported.end() ? supported : std::vector<HidDeviceInfo>{ *chosen };
}

bool DriverHidSyncLight::initDevice()
{
	if (!sendRb(ACTION_STOP_EFFECT) || !sendBrightness())
		return false;

	logDeviceInfo();

	if (_controllerLedCount == 0) {
		Error(_log, "SyncLight: could not read hardware led number, you must configure it manually in the driver setup");
		return false;
	}

	Debug(_log, "Using SyncLight hardware leds number: {:d}", _controllerLedCount);
	return true;
}

bool DriverHidSyncLight::powerOn()
{
	return sendBrightness();
}

int DriverHidSyncLight::writeFiniteColors(const std::vector<ColorRgb>& ledValues)
{
	if (ledValues.empty())
		return 0;

	const bool ok = (_outputMode == OutputMode::PerLed) ? sendScColors(ledValues) : sendAveragedColor(ledValues);
	return ok ? static_cast<int>(ledValues.size()) : -1;
}

uint8_t DriverHidSyncLight::nextId()
{
	_idCounter = static_cast<uint8_t>(_idCounter % 255 + 1); // 1..255, never 0
	return _idCounter;
}

bool DriverHidSyncLight::sendFrame(const std::vector<uint8_t>& frame)
{
	// a frame longer than one report is split into consecutive reports
	for (size_t offset = 0; offset < frame.size(); offset += REPORT_SIZE)
	{
		std::array<uint8_t, REPORT_SIZE + 1> report{}; // report[0] = report ID (0)
		std::copy_n(frame.data() + offset, std::min(REPORT_SIZE, frame.size() - offset), report.begin() + 1);

		if (!writeReport(report.data(), report.size()))
			return false;

		if (offset + REPORT_SIZE < frame.size())
			std::this_thread::sleep_for(std::chrono::milliseconds(1)); // wait before next frame
	}
	return true;
}

bool DriverHidSyncLight::sendRb(uint8_t action, std::initializer_list<uint8_t> payload)
{
	return sendFrame(buildRbFrame(action, payload, nextId()));
}

bool DriverHidSyncLight::sendBrightness()
{
	return sendRb(ACTION_BRIGHTNESS, { _brightness });
}

bool DriverHidSyncLight::sendAveragedColor(const std::vector<ColorRgb>& leds)
{
	const ColorRgb color = averageColor(leds.data(), leds.data() + std::min(leds.size(), MAX_LEDS));
	const uint8_t last = static_cast<uint8_t>(_controllerLedCount);

	return sendRb(ACTION_COLOR, { 1, color.red, color.green, color.blue, last, static_cast<uint8_t>(last + 1), 0x00, 0x00, 0x00, 0xfe });
}

bool DriverHidSyncLight::sendScColors(const std::vector<ColorRgb>& leds)
{
	// one color per controller position: our LEDs are averaged / repeated when their count differs from the controller's
	const size_t count = static_cast<size_t>(_controllerLedCount), n = leds.size();
	std::vector<ColorRgb> positions(count);
	for (size_t p = 0; p < count; ++p)
	{
		const size_t first = p * n / count, last = std::max(first + 1, (p + 1) * n / count);
		positions[p] = averageColor(leds.data() + first, leds.data() + std::min(last, n));
	}

	return sendFrame(buildScFrame(positions, nextId(), _controllerSegmentLimit));
}

// Open-time status check: one 0x82 query, one wait, info only (our LED count is what counts).
// The answer is an RB report: [3] id of the query, [8] screen size in inches, [11] LED count, [21..23] firmware version.
void DriverHidSyncLight::logDeviceInfo()
{
	const uint8_t id = nextId();
	if (!sendFrame(buildRbFrame(ACTION_READ_INFO, {}, id)))
		return;

	std::array<uint8_t, REPORT_SIZE> reply{};
	for (int length; (length = readReport(reply.data(), reply.size(), 300)) > 0;) // older answers may be queued in front of ours
		if (length >= 24 && reply[3] == id)
		{
			if (_controllerLedCount == 0) {
				_controllerLedCount = std::clamp(static_cast<int>(reply[11]), 1, 254);
			}

			Info(_log, "SyncLight: {:d}\" display, controller reports {:d} LEDs, firmware {:d}.{:d}.{:d}",
				static_cast<int>(reply[8]), static_cast<int>(reply[11]), static_cast<int>(reply[21]), static_cast<int>(reply[22]), static_cast<int>(reply[23]));
			return;
		}

	Warning(_log, "SyncLight: no answer to the device info query, continuing");
}

LedDevice* DriverHidSyncLight::construct(const QJsonObject& deviceConfig)
{
	return new DriverHidSyncLight(deviceConfig);
}

bool DriverHidSyncLight::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("synclight", "leds_group_6_HID", DriverHidSyncLight::construct);
