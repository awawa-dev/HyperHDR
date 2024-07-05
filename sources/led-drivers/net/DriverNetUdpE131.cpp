#ifdef _WIN32
	#include <winsock.h>
#else
	#include <arpa/inet.h>
#endif

#include <QHostInfo>

// hyperhdr local includes
#include <led-drivers/net/DriverNetUdpE131.h>

union e131_packet_t
{
	#pragma pack(push, 1)
	struct
	{
		uint16_t preamble_size;
		uint16_t postamble_size;
		uint8_t  acn_id[12];
		uint16_t root_flength;
		uint32_t root_vector;
		char     cid[16];

		uint16_t frame_flength;
		uint32_t frame_vector;
		char     source_name[64];
		uint8_t  priority;
		uint16_t reserved;
		uint8_t  sequence_number;
		uint8_t  options;
		uint16_t universe;

		uint16_t dmp_flength;
		uint8_t  dmp_vector;
		uint8_t  type;
		uint16_t first_address;
		uint16_t address_increment;
		uint16_t property_value_count;
		uint8_t  property_values[513];
	};
	#pragma pack(pop)

	uint8_t raw[638];
};

namespace
{
	const unsigned int E131_DMP_DATA = 125;
	const ushort E131_DEFAULT_PORT = 5568;
	const uint32_t VECTOR_ROOT_E131_DATA = 0x00000004;
	const uint8_t VECTOR_DMP_SET_PROPERTY = 0x02;
	const uint32_t VECTOR_E131_DATA_PACKET = 0x00000002;
	const int DMX_MAX = 512;
}

DriverNetUdpE131::DriverNetUdpE131(const QJsonObject& deviceConfig)
	: ProviderUdp(deviceConfig)
{
	e131_packet = std::unique_ptr<e131_packet_t>(new e131_packet_t);
}

LedDevice* DriverNetUdpE131::construct(const QJsonObject& deviceConfig)
{
	return new DriverNetUdpE131(deviceConfig);
}

bool DriverNetUdpE131::init(const QJsonObject& deviceConfig)
{
	bool isInitOK = false;

	_port = E131_DEFAULT_PORT;

	// Initialise sub-class
	if (ProviderUdp::init(deviceConfig))
	{
		_e131_universe = deviceConfig["universe"].toInt(1);
		_e131_source_name = deviceConfig["source-name"].toString("hyperhdr on " + QHostInfo::localHostName());
		QString _json_cid = deviceConfig["cid"].toString("");

		if (_json_cid.isEmpty())
		{
			_e131_cid = QUuid::createUuid();
			Debug(_log, "e131 no CID found, generated %s", QSTRING_CSTR(_e131_cid.toString()));
			isInitOK = true;
		}
		else
		{
			_e131_cid = QUuid(_json_cid);
			if (!_e131_cid.isNull())
			{
				Debug(_log, "e131  CID found, using %s", QSTRING_CSTR(_e131_cid.toString()));
				isInitOK = true;
			}
			else
			{
				this->setInError("CID configured is not a valid UUID. Format expected is \"xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\"");
			}
		}
	}
	return isInitOK;
}

// populates the headers
void DriverNetUdpE131::prepare(unsigned this_universe, unsigned this_dmxChannelCount)
{
	memset(e131_packet->raw, 0, sizeof(e131_packet->raw));

	/* Root Layer */
	e131_packet->preamble_size = htons(16);
	e131_packet->postamble_size = 0;
	memcpy(e131_packet->acn_id, _acn_id, 12);
	e131_packet->root_flength = htons(0x7000 | (110 + this_dmxChannelCount));
	e131_packet->root_vector = htonl(VECTOR_ROOT_E131_DATA);
	memcpy(e131_packet->cid, _e131_cid.toRfc4122().constData(), sizeof(e131_packet->cid));

	/* Frame Layer */
	e131_packet->frame_flength = htons(0x7000 | (88 + this_dmxChannelCount));
	e131_packet->frame_vector = htonl(VECTOR_E131_DATA_PACKET);
	snprintf(e131_packet->source_name, sizeof(e131_packet->source_name), "%s", QSTRING_CSTR(_e131_source_name));
	e131_packet->priority = 100;
	e131_packet->reserved = htons(0);
	e131_packet->options = 0;	// Bit 7 =  Preview_Data
					// Bit 6 =  Stream_Terminated
					// Bit 5 = Force_Synchronization
	e131_packet->universe = htons(this_universe);

	/* DMX Layer */
	e131_packet->dmp_flength = htons(0x7000 | (11 + this_dmxChannelCount));
	e131_packet->dmp_vector = VECTOR_DMP_SET_PROPERTY;
	e131_packet->type = 0xa1;
	e131_packet->first_address = htons(0);
	e131_packet->address_increment = htons(1);
	e131_packet->property_value_count = htons(1 + this_dmxChannelCount);

	e131_packet->property_values[0] = 0;	// start code
}

int DriverNetUdpE131::write(const std::vector<ColorRgb>& ledValues)
{
	int retVal = 0;
	int thisChannelCount = 0;
	int dmxChannelCount = _ledRGBCount;
	const uint8_t* rawdata = reinterpret_cast<const uint8_t*>(ledValues.data());

	_e131_seq++;

	for (int rawIdx = 0; rawIdx < dmxChannelCount; rawIdx++)
	{
		if (rawIdx % DMX_MAX == 0) // start of new packet
		{
			thisChannelCount = (dmxChannelCount - rawIdx < DMX_MAX) ? dmxChannelCount % DMX_MAX : DMX_MAX;
			//			                       is this the last packet?         ?       ^^ last packet      : ^^ earlier packets

			prepare(_e131_universe + rawIdx / DMX_MAX, thisChannelCount);
			e131_packet->sequence_number = _e131_seq;
		}

		e131_packet->property_values[1 + rawIdx % DMX_MAX] = rawdata[rawIdx];

		//     is this the      last byte of last packet    ||   last byte of other packets
		if ((rawIdx == dmxChannelCount - 1) || (rawIdx % DMX_MAX == DMX_MAX - 1))
		{
#undef e131debug
#if e131debug
			Debug(_log, "send packet: rawidx %d dmxchannelcount %d universe: %d, packetsz %d"
				, rawIdx
				, dmxChannelCount
				, _e131_universe + rawIdx / DMX_MAX
				, E131_DMP_DATA + 1 + thisChannelCount
			);
#endif
			retVal &= writeBytes(E131_DMP_DATA + 1 + thisChannelCount, e131_packet->raw);
		}
	}

	return retVal;
}

bool DriverNetUdpE131::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("udpe131", "leds_group_2_network", DriverNetUdpE131::construct);
