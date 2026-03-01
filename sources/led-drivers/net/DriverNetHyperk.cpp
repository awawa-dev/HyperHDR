/* DriverNetHyperk.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2026 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/HyperHDR
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
 */

#include <led-drivers/net/DriverNetHyperk.h>
#include <HyperhdrConfig.h>
#ifdef ENABLE_BONJOUR
	#include <bonjour/DiscoveryWrapper.h>
#endif

DriverNetHyperk::DriverNetHyperk(const QJsonObject& deviceConfig)
	: DriverNetDDP(deviceConfig)
	, _overrideBrightness(true)
	, _brightnessLevel(255)
	, _restoreConfig(false)
{
}

bool DriverNetHyperk::init(QJsonObject deviceConfig)
{
	bool result = DriverNetDDP::init(deviceConfig);

	if (result) {
		QString ipString = (_address.protocol() == QAbstractSocket::IPv4Protocol) ?
			_address.toString() : QHostAddress(_address.toIPv4Address()).toString();

		_restApi = std::make_unique<ProviderRestApi>(ipString, 80);
		_restApi->setBasePath("/json");

		_overrideBrightness = deviceConfig["brightnessMax"].toBool(true);
		Debug(_log, "Override brightness : {:s}", (_overrideBrightness) ? "true" : "false");

		_brightnessLevel = deviceConfig["brightnessMaxLevel"].toInt(255);
		Debug(_log, "Set brightness level: {:d}", _brightnessLevel);

		_restoreConfig = deviceConfig["restoreOriginalState"].toBool(false);
		Debug(_log, "Restore Hyperk state: {:s}", (_restoreConfig) ? "true" : "false");

		_maxRetry = deviceConfig["maxRetry"].toInt(60);
		Debug(_log, "Max retry           : {:d}", _maxRetry);
	}

	return result;
}

int DriverNetHyperk::writeFiniteColors(const std::vector<ColorRgb>& ledValues)
{
	return DriverNetDDP::writeFiniteColors(ledValues);
}

LedDevice* DriverNetHyperk::construct(const QJsonObject& deviceConfig)
{
	return new DriverNetHyperk(deviceConfig);
}

bool DriverNetHyperk::powerOn()
{
	Debug(_log, "");

	_restApi->setPath("");
	httpResponse response = _restApi->get();

	auto hyperkConfig = response.getBody();
	if (hyperkConfig.isEmpty())
	{
		response.setError(true);
		response.setErrorReason("Empty Hyperk config");
	}

	if (!response.error())
	{
		if (hyperkConfig.isObject())
		{
			QJsonObject mainConfig = hyperkConfig.object();
			QJsonObject infoConfig = mainConfig["info"].toObject();
			QJsonObject ledsConfig = infoConfig["leds"].toObject();
			QJsonObject stateConfig = mainConfig["state"].toObject();
			QJsonObject wifiConfig = infoConfig["wifi"].toObject();

			if (_restoreConfig)
			{
				stateConfig["live"] = false;
				_configBackup.setObject(stateConfig);
			}

			uint ledsNumber = ledsConfig["count"].toInt(0);
			int powerLimiter = ledsConfig["maxpwr"].toInt(0);
			int quality = wifiConfig["signal"].toInt(0);

			QString infoMessage = QString("Hyperk info => wifi quality: %1, wifi channel: %2, leds: %3, arch: %4, ver: %5, uptime: %6s, power limit: %8mA")
				.arg(QString::number(quality) + ((quality < 80) ? +"% (LOW)" : "%")).arg(wifiConfig["channel"].toInt()).arg(ledsNumber)
				.arg(infoConfig["arch"].toString("unknown")).arg(infoConfig["cn"].toString("unknown"))
				.arg(infoConfig["uptime"].toInt(-1)).arg(powerLimiter);

			if (quality < 80 || powerLimiter > 0 || _ledCount != ledsNumber)
				Warning(_log, "{:s}", (infoMessage));
			else
				Info(_log, "{:s}", (infoMessage));

			if (powerLimiter > 0)
				Error(_log, "Serious warning: the power limiter in Hyperk is set which may lead to unexpected side effects. Use the right cabling & power supply with the appropriate power, not this half-measure.");

			if (_ledCount != ledsNumber)
				Warning(_log, "The number of LEDs defined in HyperHDR ({:d}) is different from that defined in Hyperk ({:d})", _ledCount, ledsNumber);

			_customInfo = QString("  %1%").arg(quality);
		}
		else
			Warning(_log, "Could not read Hyperk config");

		_restApi->setPath("/state");
		response = _restApi->put(getOnOffRequest(true));
	}

	if (response.error())
	{
		this->setInError(response.error() ? response.getErrorReason() : "Empty Hyperk config");
		setupRetry(1500);
		return false;
	}

	return true;
}

bool DriverNetHyperk::powerOff()
{
	Debug(_log, "");
	bool off = true;

	if (_isDeviceReady)
	{
		writeBlack();

		_restApi->setPath("/state");
		httpResponse response = _restApi->put(getOnOffRequest(false));
		if (response.error())
		{
			this->setInError(response.getErrorReason());
			off = false;
		}
	}
	return off;
}

QString DriverNetHyperk::getOnOffRequest(bool isOn) const
{
	if (!isOn && _restoreConfig && !_configBackup.isEmpty())
	{
		return QString(_configBackup.toJson(QJsonDocument::Compact));
	}
	else
	{
		QString state = isOn ? "true" : "false";
		QString bri = (_overrideBrightness && isOn) ? QString(",\"bri\":%1").arg(_brightnessLevel) : "";
		return QString("{\"on\":%1,\"live\":%1%2}").arg(state).arg(bri);
	}
}

QJsonObject DriverNetHyperk::discover(const QJsonObject& /*params*/)
{
	QJsonObject devicesDiscovered;
	QJsonArray deviceList;
	devicesDiscovered.insert("ledDeviceType", _activeDeviceType);

#ifdef ENABLE_BONJOUR
	std::shared_ptr<DiscoveryWrapper> bonInstance = _discoveryWrapper.lock();
	if (bonInstance != nullptr)
	{
		QList<DiscoveryRecord> recs;

		SAFE_CALL_0_RET(bonInstance.get(), getHyperk, QList<DiscoveryRecord>, recs);

		for (DiscoveryRecord& r : recs)
		{
			QJsonObject newIp;
			newIp["value"] = QString("%1").arg(r.address);
			newIp["name"] = QString("%1 (%2)").arg(newIp["value"].toString()).arg(r.hostName);
			deviceList.push_back(newIp);
		}
	}
#else
	Error(_log, "The Network Discovery Service was mysteriously disabled while the maintenair was compiling this version of HyperHDR");
#endif	

	devicesDiscovered.insert("devices", deviceList);
	Debug(_log, "devicesDiscovered: [{:s}]", QString(QJsonDocument(devicesDiscovered).toJson(QJsonDocument::Compact)).toUtf8().constData());

	return devicesDiscovered;
}

bool DriverNetHyperk::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("hyperk", "leds_group_2_network", DriverNetHyperk::construct);
