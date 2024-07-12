#include <led-drivers/net/DriverNetTpm2net.h>
#include <vector>
#include <algorithm>

const ushort TPM2_DEFAULT_PORT = 65506;

DriverNetTpm2net::DriverNetTpm2net(const QJsonObject& deviceConfig)
	: ProviderUdp(deviceConfig),
	_tpm2Max(0),
	_tpm2ByteCount(0),
	_tpm2TotalPackets(0)
{
}

LedDevice* DriverNetTpm2net::construct(const QJsonObject& deviceConfig)
{
	return new DriverNetTpm2net(deviceConfig);
}

bool DriverNetTpm2net::init(const QJsonObject& deviceConfig)
{
	bool isInitOK = false;

	_port = TPM2_DEFAULT_PORT;

	// Initialise sub-class
	if (ProviderUdp::init(deviceConfig))
	{
		_tpm2Max = std::max(deviceConfig["max-packet"].toInt(170), 1);
		_tpm2ByteCount = 3 * _ledCount;
		_tpm2TotalPackets = 1 + (_tpm2ByteCount / _tpm2Max);

		isInitOK = true;
	}
	return isInitOK;
}

int DriverNetTpm2net::write(const std::vector<ColorRgb>& ledValues)
{
	std::vector<uint8_t> tpm2buffer((size_t)_tpm2Max + 7);
	const uint8_t* rawdata = reinterpret_cast<const uint8_t*>(ledValues.data());

	int retVal = 0;
	int thisPacketBytes = 0;
	int tpm2ThisPacket = 1;

	for (int rawIdx = 0; rawIdx < _tpm2ByteCount; rawIdx++)
	{
		if (rawIdx % _tpm2Max == 0) // start of new packet
		{
			// is this the last packet?         ?    ^^ last packet          : ^^ earlier packets
			thisPacketBytes = (_tpm2ByteCount - rawIdx < _tpm2Max) ? (_tpm2ByteCount % _tpm2Max) : _tpm2Max;

			tpm2buffer[0] = 0x9c;	// Packet start byte
			tpm2buffer[1] = 0xda;   // Packet type Data frame
			tpm2buffer[2] = (thisPacketBytes >> 8) & 0xff; // Frame size high
			tpm2buffer[3] = thisPacketBytes & 0xff;        // Frame size low
			tpm2buffer[4] = tpm2ThisPacket++;        // Packet Number
			tpm2buffer[5] = _tpm2TotalPackets;       // Number of packets
		}

		tpm2buffer[(size_t)6 + (rawIdx % _tpm2Max)] = rawdata[rawIdx];

		//     is this the      last byte of last packet    ||   last byte of other packets
		if ((rawIdx == (_tpm2ByteCount - 1)) || ((rawIdx % _tpm2Max) == (_tpm2Max - 1)))
		{
			tpm2buffer[(size_t)6 + (rawIdx % _tpm2Max) + 1] = 0x36;		// Packet end byte
			retVal &= writeBytes(thisPacketBytes + 7, reinterpret_cast<const uint8_t*>(tpm2buffer.data()));
		}
	}

	return retVal;
}

bool DriverNetTpm2net::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("tpm2net", "leds_group_2_network", DriverNetTpm2net::construct);
