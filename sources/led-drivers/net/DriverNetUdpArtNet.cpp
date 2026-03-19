#include <led-drivers/net/DriverNetUdpArtNet.h>
#include <infinite-color-engine/ColorSpace.h>

#include <bit>
#include <cstring>
#include <QHostInfo>

/**
 *
 *  This program is provided free for you to use in any way that you wish,
 *  subject to the laws and regulations where you are using it.  Due diligence
 *  is strongly suggested before using this code.  Please give credit where due.
 *
 **/

namespace {
	const int DMX_MAX = 512;
	const ushort ARTNET_DEFAULT_PORT = 6454;

	constexpr uint16_t cpp_htons(uint16_t value) {
		if constexpr (std::endian::native == std::endian::little) {
			return ((value & 0xFF) << 8) | ((value >> 8) & 0xFF);
		}
		else {
			return value;
		}
	}
}

// http://stackoverflow.com/questions/16396013/artnet-packet-structure
union artnet_packet_t
{	
#pragma pack(push, 1)
	struct {
		char		ID[8];		// "Art-Net"
		uint16_t	OpCode;		// See Doc. Table 1 - OpCodes e.g. 0x5000 OpOutput / OpDmx
		uint16_t	ProtVer;	// 0x0e00 (aka 14)
		uint8_t		Sequence;	// monotonic counter
		uint8_t		Physical;	// 0x00
		uint8_t		SubUni;		// low universe (0-255)
		uint8_t		Net;		// high universe (not used)
		uint16_t	Length;		// data length (2 - 512)
		uint8_t		Data[DMX_MAX];	// universe data
	} fields;
#pragma pack(pop)

	uint8_t raw[18 + DMX_MAX];
};

DriverNetUdpArtNet::DriverNetUdpArtNet(const QJsonObject& deviceConfig)
	: ProviderUdp(deviceConfig)
	, _enable_ice_rgbw(false)
	, _ice_white_temperatur{ 0.8f, 0.8f, 0.8f }
	, _ice_white_mixer_threshold(0.02f)
	, _ice_white_led_intensity(1.8f)
{
	artnet_packet = std::make_unique<artnet_packet_t>();
}

bool DriverNetUdpArtNet::init(QJsonObject deviceConfig)
{
	bool isInitOK = false;

	_port = ARTNET_DEFAULT_PORT;

	// Initialise sub-class
	if (ProviderUdp::init(deviceConfig))
	{
		_artnet_universe = deviceConfig["universe"].toInt(1);
		_artnet_channelsPerFixture = deviceConfig["channelsPerFixture"].toInt(3);
		_disableSplitting = deviceConfig["disableSplitting"].toBool(false);

		_enable_ice_rgbw = deviceConfig["enable_ice_rgbw"].toBool(false);
		_ice_white_mixer_threshold = deviceConfig["ice_white_mixer_threshold"].toDouble(0.02);
		_ice_white_led_intensity = deviceConfig["ice_white_led_intensity"].toDouble(1.8);
		_ice_white_temperatur.x = deviceConfig["ice_white_temperatur_r"].toDouble(0.8);
		_ice_white_temperatur.y = deviceConfig["ice_white_temperatur_g"].toDouble(0.8);
		_ice_white_temperatur.z = deviceConfig["ice_white_temperatur_b"].toDouble(0.8);
		Debug(_log, "Infinite Color Engine RGBW is: {:s}, white channel temp for the white LED: {:s}, white mixer threshold: {:f}, white LED intensity: {:f}",
			((_enable_ice_rgbw) ? "enabled" : "disabled"), ColorSpaceMath::vecToString(_ice_white_temperatur), _ice_white_mixer_threshold, _ice_white_led_intensity);

		isInitOK = true;
	}
	return isInitOK;
}

// populates the headers
void DriverNetUdpArtNet::prepare(unsigned this_universe, unsigned this_sequence, unsigned this_dmxChannelCount)
{
	if (this_dmxChannelCount & 1) this_dmxChannelCount++;
	if (this_dmxChannelCount > DMX_MAX) this_dmxChannelCount = DMX_MAX;

	memcpy(artnet_packet->fields.ID, "Art-Net\0", 8);

	artnet_packet->fields.OpCode = cpp_htons(0x0050);	// OpOutput / OpDmx
	artnet_packet->fields.ProtVer = cpp_htons(0x000e);
	artnet_packet->fields.Sequence = this_sequence;
	artnet_packet->fields.Physical = 0;
	artnet_packet->fields.SubUni = this_universe & 0xff;
	artnet_packet->fields.Net = (this_universe >> 8) & 0x7f;
	artnet_packet->fields.Length = cpp_htons(this_dmxChannelCount);
}

int DriverNetUdpArtNet::writeFiniteColors(const std::vector<ColorRgb>& ledValues)
{
	int retVal = 0;
	int thisUniverse = _artnet_universe;
	const uint8_t* rawdata = reinterpret_cast<const uint8_t*>(ledValues.data());

	/*
	This field is incremented in the range 0x01 to 0xff to allow the receiving node to resequence packets.
	The Sequence field is set to 0x00 to disable this feature.
	*/
	if (_artnet_seq++ == 0)
	{
		_artnet_seq = 1;
	}

	int dmxIdx = 0;			// offset into the current dmx packet

	memset(artnet_packet->raw, 0, sizeof(artnet_packet->raw));
	for (unsigned int ledIdx = 0; ledIdx < _ledRGBCount; ledIdx++)
	{

		artnet_packet->fields.Data[dmxIdx++] = rawdata[ledIdx];
		if ((ledIdx % 3 == 2) && (ledIdx > 0))
		{
			dmxIdx += (_artnet_channelsPerFixture - 3);
		}

		//     is this the   last byte of last packet   ||   last byte of other packets
		if ((ledIdx == _ledRGBCount - 1) || (dmxIdx >= DMX_MAX) || (_disableSplitting && dmxIdx + _artnet_channelsPerFixture > DMX_MAX))
		{
			prepare(thisUniverse, _artnet_seq, dmxIdx);
			retVal &= writeBytes(18 + qMin(dmxIdx, DMX_MAX), artnet_packet->raw);

			memset(artnet_packet->raw, 0, sizeof(artnet_packet->raw));
			thisUniverse++;
			dmxIdx = 0;
		}

	}

	return retVal;
}

std::pair<bool, int> DriverNetUdpArtNet::writeInfiniteColors(SharedOutputColors nonlinearRgbColors)
{
	if (nonlinearRgbColors->empty() || !_enable_ice_rgbw)
	{
		return { _enable_ice_rgbw, 0 };
	}

	_ledBuffer.resize(nonlinearRgbColors->size() * 4);

	// RGBW by Infinite Color Engine
	_infiniteColorEngineRgbw.renderRgbwFrame(*nonlinearRgbColors, _ice_white_mixer_threshold, _ice_white_led_intensity, _ice_white_temperatur, _ledBuffer, 0, _colorOrder);

	int channelsPerFixture = (std::max)(4, _artnet_channelsPerFixture);
	int totalBytesWritten = 0;
	int thisUniverse = _artnet_universe;
	int dmxIdx = 0;
	memset(artnet_packet->raw, 0, sizeof(artnet_packet->raw));

	auto sendCurrentUniverse = [&]() {
		if (dmxIdx == 0) return;

		int sendLength = (dmxIdx % 2 != 0) ? dmxIdx + 1 : dmxIdx;

		if (++_artnet_seq == 0)
		{
			_artnet_seq = 1;
		}
		prepare(thisUniverse, _artnet_seq, sendLength);
		totalBytesWritten += writeBytes(18 + qMin(sendLength, DMX_MAX), artnet_packet->raw);

		memset(artnet_packet->raw, 0, sizeof(artnet_packet->raw));
		thisUniverse++;
		dmxIdx = 0;
	};

	const uint8_t* bufferEnd = _ledBuffer.data() + _ledBuffer.size();
	for (const uint8_t* rgbw = _ledBuffer.data(); rgbw < bufferEnd; rgbw += 4)
	{
		if (_disableSplitting && (dmxIdx + channelsPerFixture > DMX_MAX))
		{
			sendCurrentUniverse();
		}

		for (int ch = 0; ch < channelsPerFixture; ++ch)
		{
			if (dmxIdx >= DMX_MAX)
			{
				sendCurrentUniverse();
			}

			uint8_t value = (ch < 4) ? rgbw[ch] : 0;
			artnet_packet->fields.Data[dmxIdx++] = value;
		}
	}

	if (dmxIdx > 0)
	{
		sendCurrentUniverse();
	}

	return { true, totalBytesWritten };
}

LedDevice* DriverNetUdpArtNet::construct(const QJsonObject& deviceConfig)
{
	return new DriverNetUdpArtNet(deviceConfig);
}

bool DriverNetUdpArtNet::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("udpartnet", "leds_group_2_network", DriverNetUdpArtNet::construct);
