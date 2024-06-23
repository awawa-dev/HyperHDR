#include <led-drivers/spi/DriverSpiLpd8806.h>

///
/// Implementation of the LedDevice interface for writing to LPD8806 led device.
///
/// The following description is copied from 'adafruit' (github.com/adafruit/LPD8806)
///
/// Clearing up some misconceptions about how the LPD8806 drivers work:
///
/// The LPD8806 is not a FIFO shift register.  The first data out controls the
/// LED *closest* to the processor (unlike a typical shift register, where the
/// first data out winds up at the *furthest* LED).  Each LED driver 'fills up'
/// with data and then passes through all subsequent bytes until a latch
/// condition takes place.  This is actually pretty common among LED drivers.
///
/// All color data bytes have the high bit (128) set, with the remaining
/// seven bits containing a brightness value (0-127).  A byte with the high
/// bit clear has special meaning (explained later).
///
/// The rest gets bizarre...
///
/// The LPD8806 does not perform an in-unison latch (which would display the
/// newly-transmitted data all at once).  Rather, each individual byte (even
/// the separate G, R, B components of each LED) is latched AS IT ARRIVES...
/// or more accurately, as the first bit of the subsequent byte arrives and
/// is passed through.  So the strip actually refreshes at the speed the data
/// is issued, not instantaneously (this can be observed by greatly reducing
/// the data rate).  This has implications for POV displays and light painting
/// applications.  The 'subsequent' rule also means that at least one extra
/// byte must follow the last pixel, in order for the final blue LED to latch.
///
/// To reset the pass-through behaviour and begin sending new data to the start
/// of the strip, a number of zero bytes must be issued (remember, all color
/// data bytes have the high bit set, thus are in the range 128 to 255, so the
/// zero is 'special').  This should be done before each full payload of color
/// values to the strip.  Curiously, zero bytes can only travel one meter (32
/// LEDs) down the line before needing backup; the next meter requires an
/// extra zero byte, and so forth.  Longer strips will require progressively
/// more zeros.  *(see note below)
///
/// In the interest of efficiency, it's possible to combine the former EOD
/// extra latch byte and the latter zero reset...the same data can do double
/// duty, latching the last blue LED while also resetting the strip for the
/// next payload.
///
/// So: reset byte(s) of suitable length are issued once at startup to 'prime'
/// the strip to a known ready state.  After each subsequent LED color payload,
/// these reset byte(s) are then issued at the END of each payload, both to
/// latch the last LED and to prep the strip for the start of the next payload
/// (even if that data does not arrive immediately).  This avoids a tiny bit
/// of latency as the new color payload can begin issuing immediately on some
/// signal, such as a timer or GPIO trigger.
///
/// Technically these zero byte(s) are not a latch, as the color data (save
/// for the last byte) is already latched.  It's a start-of-data marker, or
/// an indicator to clear the thing-that's-not-a-shift-register.  But for
/// conversational consistency with other LED drivers, we'll refer to it as
/// a 'latch' anyway.
///
/// This has been validated independently with multiple customers'
/// hardware.  Please do not report as a bug or issue pull requests for
/// this.  Fewer zeros sometimes gives the *illusion* of working, the first
/// payload will correctly load and latch, but subsequent frames will drop
/// data at the end.  The data shortfall won't always be visually apparent
/// depending on the color data loaded on the prior and subsequent frames.
/// Tested.  Confirmed.  Fact.
///
///
/// The summary of the story is that the following needs to be written on the spi-device:
/// 1RRRRRRR 1GGGGGGG 1BBBBBBB 1RRRRRRR 1GGGGGGG ... ... 1GGGGGGG 1BBBBBBB 00000000 00000000 ...
/// |---------led_1----------| |---------led_2--         -led_n----------| |----clear data--
///
/// The number of zeroes in the 'clear data' is (#led/32 + 1)bytes (or *8 for bits)
///

DriverSpiLpd8806::DriverSpiLpd8806(const QJsonObject& deviceConfig)
	: ProviderSpi(deviceConfig)
{
}

LedDevice* DriverSpiLpd8806::construct(const QJsonObject& deviceConfig)
{
	return new DriverSpiLpd8806(deviceConfig);
}

bool DriverSpiLpd8806::init(const QJsonObject& deviceConfig)
{
	bool isInitOK = false;

	// Initialise sub-class
	if (ProviderSpi::init(deviceConfig))
	{
		const unsigned clearSize = _ledCount / 32 + 1;
		unsigned messageLength = _ledRGBCount + clearSize;
		// Initialise the buffer
		_ledBuffer.resize(messageLength, 0x00);

		isInitOK = true;
	}
	return isInitOK;
}

int DriverSpiLpd8806::write(const std::vector<ColorRgb>& ledValues)
{
	if (_ledCount != ledValues.size())
	{
		Warning(_log, "Lpd8806 led's number has changed (old: %d, new: %d). Rebuilding buffer.", _ledCount, ledValues.size());
		_ledCount = ledValues.size();

		_ledBuffer.resize(0, 0x00);

		const unsigned clearSize = _ledCount / 32 + 1;
		unsigned messageLength = _ledRGBCount + clearSize;
		// Initialise the buffer
		_ledBuffer.resize(messageLength, 0x00);
	}

	// Copy the colors from the ColorRgb vector to the Ldp8806 data vector
	for (unsigned iLed = 0; iLed < (unsigned)_ledCount; ++iLed)
	{
		const ColorRgb& color = ledValues[iLed];

		_ledBuffer[iLed * 3] = 0x80 | (color.red >> 1);
		_ledBuffer[iLed * 3 + 1] = 0x80 | (color.green >> 1);
		_ledBuffer[iLed * 3 + 2] = 0x80 | (color.blue >> 1);
	}

	// Write the data
	return writeBytes(_ledBuffer.size(), _ledBuffer.data());
}

bool DriverSpiLpd8806::isRegistered = hyperhdr::leds::REGISTER_LED_DEVICE("lpd8806", "leds_group_0_SPI", DriverSpiLpd8806::construct);
