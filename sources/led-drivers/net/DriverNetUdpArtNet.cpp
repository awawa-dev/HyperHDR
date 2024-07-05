#include <led-drivers/net/DriverNetUdpArtNet.h>

#ifdef _WIN32
	#include <winsock.h>
#else
	#include <arpa/inet.h>
#endif

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
	};
#pragma pack(pop)

	uint8_t raw[18 + DMX_MAX];
};

DriverNetUdpArtNet::DriverNetUdpArtNet(const QJsonObject& deviceConfig)
	: ProviderUdp(deviceConfig)
{
	artnet_packet = std::unique_ptr<artnet_packet_t>(new artnet_packet_t);
}

LedDevice* DriverNetUdpArtNet::construct(const QJsonObject& deviceConfig)
{
	return new DriverNetUdpArtNet(deviceConfig);
}

bool DriverNetUdpArtNet::init(const QJsonObject& deviceConfig)
{
	bool isInitOK = false;

	_port = ARTNET_DEFAULT_PORT;

	// Initialise sub-class
	if (ProviderUdp::init(deviceConfig))
	{
		_artnet_universe = deviceConfig["universe"].toInt(1);
		_artnet_channelsPerFixture = deviceConfig["channelsPerFixture"].toInt(3);
		_disableSplitting = deviceConfig["disableSplitting"].toBool(false);

		isInitOK = true;
	}
	return isInitOK;
}

// populates the headers
void DriverNetUdpArtNet::prepare(unsigned this_universe, unsigned this_sequence, unsigned this_dmxChannelCount)
{
	// WTF? why do the specs say:
	// "This value should be an even number in the range 2 – 512. "
	if (this_dmxChannelCount & 0x1)
	{
		this_dmxChannelCount++;
	}

	memcpy(artnet_packet->ID, "Art-Net\0", 8);

	artnet_packet->OpCode = htons(0x0050);	// OpOutput / OpDmx
	artnet_packet->ProtVer = htons(0x000e);
	artnet_packet->Sequence = this_sequence;
	artnet_packet->Physical = 0;
	artnet_packet->SubUni = this_universe & 0xff;
	artnet_packet->Net = (this_universe >> 8) & 0x7f;
	artnet_packet->Length = htons(this_dmxChannelCount);
}

int DriverNetUdpArtNet::write(const std::vector<ColorRgb>& ledValues)
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

		artnet_packet->Data[dmxIdx++] = rawdata[ledIdx];
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

bool DriverNetUdpArtNet::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("udpartnet", "leds_group_2_network", DriverNetUdpArtNet::construct);
