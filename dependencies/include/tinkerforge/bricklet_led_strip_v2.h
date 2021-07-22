/* ***********************************************************
 * This file was automatically generated on 2021-05-06.      *
 *                                                           *
 * C/C++ Bindings Version 2.1.32                             *
 *                                                           *
 * If you have a bugfix for this file and want to commit it, *
 * please fix the bug in the generator. You can find a link  *
 * to the generators git repository on tinkerforge.com       *
 *************************************************************/

#ifndef BRICKLET_LED_STRIP_V2_H
#define BRICKLET_LED_STRIP_V2_H

#include "ip_connection.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup BrickletLEDStripV2 LED Strip Bricklet 2.0
 */

/**
 * \ingroup BrickletLEDStripV2
 *
 * Controls up to 2048 RGB(W) LEDs
 */
typedef Device LEDStripV2;

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_FUNCTION_SET_LED_VALUES_LOW_LEVEL 1

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_FUNCTION_GET_LED_VALUES_LOW_LEVEL 2

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_FUNCTION_SET_FRAME_DURATION 3

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_FUNCTION_GET_FRAME_DURATION 4

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_FUNCTION_GET_SUPPLY_VOLTAGE 5

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_FUNCTION_SET_CLOCK_FREQUENCY 7

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_FUNCTION_GET_CLOCK_FREQUENCY 8

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_FUNCTION_SET_CHIP_TYPE 9

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_FUNCTION_GET_CHIP_TYPE 10

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_FUNCTION_SET_CHANNEL_MAPPING 11

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_FUNCTION_GET_CHANNEL_MAPPING 12

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_FUNCTION_SET_FRAME_STARTED_CALLBACK_CONFIGURATION 13

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_FUNCTION_GET_FRAME_STARTED_CALLBACK_CONFIGURATION 14

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_FUNCTION_GET_SPITFP_ERROR_COUNT 234

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_FUNCTION_SET_BOOTLOADER_MODE 235

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_FUNCTION_GET_BOOTLOADER_MODE 236

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_FUNCTION_SET_WRITE_FIRMWARE_POINTER 237

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_FUNCTION_WRITE_FIRMWARE 238

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_FUNCTION_SET_STATUS_LED_CONFIG 239

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_FUNCTION_GET_STATUS_LED_CONFIG 240

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_FUNCTION_GET_CHIP_TEMPERATURE 242

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_FUNCTION_RESET 243

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_FUNCTION_WRITE_UID 248

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_FUNCTION_READ_UID 249

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_FUNCTION_GET_IDENTITY 255

/**
 * \ingroup BrickletLEDStripV2
 *
 * Signature: \code void callback(uint16_t length, void *user_data) \endcode
 * 
 * This callback is triggered directly after a new frame render is started.
 * The parameter is the number of LEDs in that frame.
 * 
 * You should send the data for the next frame directly after this callback
 * was triggered.
 * 
 * For an explanation of the general approach see {@link led_strip_v2_set_led_values}.
 */
#define LED_STRIP_V2_CALLBACK_FRAME_STARTED 6


/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHIP_TYPE_WS2801 2801

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHIP_TYPE_WS2811 2811

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHIP_TYPE_WS2812 2812

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHIP_TYPE_LPD8806 8806

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHIP_TYPE_APA102 102

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_RGB 6

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_RBG 9

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_BRG 33

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_BGR 36

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_GRB 18

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_GBR 24

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_RGBW 27

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_RGWB 30

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_RBGW 39

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_RBWG 45

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_RWGB 54

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_RWBG 57

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_GRWB 78

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_GRBW 75

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_GBWR 108

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_GBRW 99

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_GWBR 120

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_GWRB 114

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_BRGW 135

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_BRWG 141

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_BGRW 147

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_BGWR 156

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_BWRG 177

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_BWGR 180

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_WRBG 201

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_WRGB 198

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_WGBR 216

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_WGRB 210

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_WBGR 228

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_CHANNEL_MAPPING_WBRG 225

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_BOOTLOADER_MODE_BOOTLOADER 0

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_BOOTLOADER_MODE_FIRMWARE 1

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_BOOTLOADER_MODE_BOOTLOADER_WAIT_FOR_REBOOT 2

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_BOOTLOADER_MODE_FIRMWARE_WAIT_FOR_REBOOT 3

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_BOOTLOADER_MODE_FIRMWARE_WAIT_FOR_ERASE_AND_REBOOT 4

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_BOOTLOADER_STATUS_OK 0

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_BOOTLOADER_STATUS_INVALID_MODE 1

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_BOOTLOADER_STATUS_NO_CHANGE 2

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_BOOTLOADER_STATUS_ENTRY_FUNCTION_NOT_PRESENT 3

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_BOOTLOADER_STATUS_DEVICE_IDENTIFIER_INCORRECT 4

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_BOOTLOADER_STATUS_CRC_MISMATCH 5

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_STATUS_LED_CONFIG_OFF 0

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_STATUS_LED_CONFIG_ON 1

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_STATUS_LED_CONFIG_SHOW_HEARTBEAT 2

/**
 * \ingroup BrickletLEDStripV2
 */
#define LED_STRIP_V2_STATUS_LED_CONFIG_SHOW_STATUS 3

/**
 * \ingroup BrickletLEDStripV2
 *
 * This constant is used to identify a LED Strip Bricklet 2.0.
 *
 * The {@link led_strip_v2_get_identity} function and the
 * {@link IPCON_CALLBACK_ENUMERATE} callback of the IP Connection have a
 * \c device_identifier parameter to specify the Brick's or Bricklet's type.
 */
#define LED_STRIP_V2_DEVICE_IDENTIFIER 2103

/**
 * \ingroup BrickletLEDStripV2
 *
 * This constant represents the display name of a LED Strip Bricklet 2.0.
 */
#define LED_STRIP_V2_DEVICE_DISPLAY_NAME "LED Strip Bricklet 2.0"

/**
 * \ingroup BrickletLEDStripV2
 *
 * Creates the device object \c led_strip_v2 with the unique device ID \c uid and adds
 * it to the IPConnection \c ipcon.
 */
void led_strip_v2_create(LEDStripV2 *led_strip_v2, const char *uid, IPConnection *ipcon);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Removes the device object \c led_strip_v2 from its IPConnection and destroys it.
 * The device object cannot be used anymore afterwards.
 */
void led_strip_v2_destroy(LEDStripV2 *led_strip_v2);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Returns the response expected flag for the function specified by the
 * \c function_id parameter. It is *true* if the function is expected to
 * send a response, *false* otherwise.
 *
 * For getter functions this is enabled by default and cannot be disabled,
 * because those functions will always send a response. For callback
 * configuration functions it is enabled by default too, but can be disabled
 * via the led_strip_v2_set_response_expected function. For setter functions it is
 * disabled by default and can be enabled.
 *
 * Enabling the response expected flag for a setter function allows to
 * detect timeouts and other error conditions calls of this setter as well.
 * The device will then send a response for this purpose. If this flag is
 * disabled for a setter function then no response is sent and errors are
 * silently ignored, because they cannot be detected.
 */
int led_strip_v2_get_response_expected(LEDStripV2 *led_strip_v2, uint8_t function_id, bool *ret_response_expected);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Changes the response expected flag of the function specified by the
 * \c function_id parameter. This flag can only be changed for setter
 * (default value: *false*) and callback configuration functions
 * (default value: *true*). For getter functions it is always enabled.
 *
 * Enabling the response expected flag for a setter function allows to detect
 * timeouts and other error conditions calls of this setter as well. The device
 * will then send a response for this purpose. If this flag is disabled for a
 * setter function then no response is sent and errors are silently ignored,
 * because they cannot be detected.
 */
int led_strip_v2_set_response_expected(LEDStripV2 *led_strip_v2, uint8_t function_id, bool response_expected);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Changes the response expected flag for all setter and callback configuration
 * functions of this device at once.
 */
int led_strip_v2_set_response_expected_all(LEDStripV2 *led_strip_v2, bool response_expected);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Registers the given \c function with the given \c callback_id. The
 * \c user_data will be passed as the last parameter to the \c function.
 */
void led_strip_v2_register_callback(LEDStripV2 *led_strip_v2, int16_t callback_id, void (*function)(void), void *user_data);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Returns the API version (major, minor, release) of the bindings for this
 * device.
 */
int led_strip_v2_get_api_version(LEDStripV2 *led_strip_v2, uint8_t ret_api_version[3]);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Sets the RGB(W) values for the LEDs starting from *index*.
 * You can set at most 2048 RGB values or 1536 RGBW values (6144 byte each).
 * 
 * To make the colors show correctly you need to configure the chip type
 * (see {@link led_strip_v2_set_chip_type}) and a channel mapping (see {@link led_strip_v2_set_channel_mapping})
 * according to the connected LEDs.
 * 
 * If the channel mapping has 3 colors, you need to give the data in the sequence
 * RGBRGBRGB... if the channel mapping has 4 colors you need to give data in the
 * sequence RGBWRGBWRGBW...
 * 
 * The data is double buffered and the colors will be transfered to the
 * LEDs when the next frame duration ends (see {@link led_strip_v2_set_frame_duration}).
 * 
 * Generic approach:
 * 
 * * Set the frame duration to a value that represents the number of frames per
 *   second you want to achieve.
 * * Set all of the LED colors for one frame.
 * * Wait for the {@link LED_STRIP_V2_CALLBACK_FRAME_STARTED} callback.
 * * Set all of the LED colors for next frame.
 * * Wait for the {@link LED_STRIP_V2_CALLBACK_FRAME_STARTED} callback.
 * * And so on.
 * 
 * This approach ensures that you can change the LED colors with a fixed frame rate.
 */
int led_strip_v2_set_led_values_low_level(LEDStripV2 *led_strip_v2, uint16_t index, uint16_t value_length, uint16_t value_chunk_offset, uint8_t value_chunk_data[58]);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Returns *length* RGB(W) values starting from the
 * given *index*.
 * 
 * If the channel mapping has 3 colors, you will get the data in the sequence
 * RGBRGBRGB... if the channel mapping has 4 colors you will get the data in the
 * sequence RGBWRGBWRGBW...
 * (assuming you start at an index divisible by 3 (RGB) or 4 (RGBW)).
 */
int led_strip_v2_get_led_values_low_level(LEDStripV2 *led_strip_v2, uint16_t index, uint16_t length, uint16_t *ret_value_length, uint16_t *ret_value_chunk_offset, uint8_t ret_value_chunk_data[60]);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Sets the frame duration.
 * 
 * Example: If you want to achieve 20 frames per second, you should
 * set the frame duration to 50ms (50ms * 20 = 1 second).
 * 
 * For an explanation of the general approach see {@link led_strip_v2_set_led_values}.
 * 
 * Default value: 100ms (10 frames per second).
 */
int led_strip_v2_set_frame_duration(LEDStripV2 *led_strip_v2, uint16_t duration);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Returns the frame duration as set by {@link led_strip_v2_set_frame_duration}.
 */
int led_strip_v2_get_frame_duration(LEDStripV2 *led_strip_v2, uint16_t *ret_duration);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Returns the current supply voltage of the LEDs.
 */
int led_strip_v2_get_supply_voltage(LEDStripV2 *led_strip_v2, uint16_t *ret_voltage);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Sets the frequency of the clock.
 * 
 * The Bricklet will choose the nearest achievable frequency, which may
 * be off by a few Hz. You can get the exact frequency that is used by
 * calling {@link led_strip_v2_get_clock_frequency}.
 * 
 * If you have problems with flickering LEDs, they may be bits flipping. You
 * can fix this by either making the connection between the LEDs and the
 * Bricklet shorter or by reducing the frequency.
 * 
 * With a decreasing frequency your maximum frames per second will decrease
 * too.
 */
int led_strip_v2_set_clock_frequency(LEDStripV2 *led_strip_v2, uint32_t frequency);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Returns the currently used clock frequency as set by {@link led_strip_v2_set_clock_frequency}.
 */
int led_strip_v2_get_clock_frequency(LEDStripV2 *led_strip_v2, uint32_t *ret_frequency);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Sets the type of the LED driver chip. We currently support the chips
 * 
 * * WS2801,
 * * WS2811,
 * * WS2812 / SK6812 / NeoPixel RGB,
 * * SK6812RGBW / NeoPixel RGBW (Chip Type = WS2812),
 * * WS2813 / WS2815 (Chip Type = WS2812)
 * * LPD8806 and
 * * APA102 / DotStar.
 */
int led_strip_v2_set_chip_type(LEDStripV2 *led_strip_v2, uint16_t chip);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Returns the currently used chip type as set by {@link led_strip_v2_set_chip_type}.
 */
int led_strip_v2_get_chip_type(LEDStripV2 *led_strip_v2, uint16_t *ret_chip);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Sets the channel mapping for the connected LEDs.
 * 
 * If the mapping has 4 colors, the function {@link led_strip_v2_set_led_values} expects 4
 * values per pixel and if the mapping has 3 colors it expects 3 values per pixel.
 * 
 * The function always expects the order RGB(W). The connected LED driver chips
 * might have their 3 or 4 channels in a different order. For example, the WS2801
 * chips typically use BGR order, then WS2812 chips typically use GRB order and
 * the APA102 chips typically use WBGR order.
 * 
 * The APA102 chips are special. They have three 8-bit channels for RGB
 * and an additional 5-bit channel for the overall brightness of the RGB LED
 * making them 4-channel chips. Internally the brightness channel is the first
 * channel, therefore one of the Wxyz channel mappings should be used. Then
 * the W channel controls the brightness.
 */
int led_strip_v2_set_channel_mapping(LEDStripV2 *led_strip_v2, uint8_t mapping);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Returns the currently used channel mapping as set by {@link led_strip_v2_set_channel_mapping}.
 */
int led_strip_v2_get_channel_mapping(LEDStripV2 *led_strip_v2, uint8_t *ret_mapping);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Enables/disables the {@link LED_STRIP_V2_CALLBACK_FRAME_STARTED} callback.
 */
int led_strip_v2_set_frame_started_callback_configuration(LEDStripV2 *led_strip_v2, bool enable);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Returns the configuration as set by
 * {@link led_strip_v2_set_frame_started_callback_configuration}.
 */
int led_strip_v2_get_frame_started_callback_configuration(LEDStripV2 *led_strip_v2, bool *ret_enable);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Returns the error count for the communication between Brick and Bricklet.
 * 
 * The errors are divided into
 * 
 * * ACK checksum errors,
 * * message checksum errors,
 * * framing errors and
 * * overflow errors.
 * 
 * The errors counts are for errors that occur on the Bricklet side. All
 * Bricks have a similar function that returns the errors on the Brick side.
 */
int led_strip_v2_get_spitfp_error_count(LEDStripV2 *led_strip_v2, uint32_t *ret_error_count_ack_checksum, uint32_t *ret_error_count_message_checksum, uint32_t *ret_error_count_frame, uint32_t *ret_error_count_overflow);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Sets the bootloader mode and returns the status after the requested
 * mode change was instigated.
 * 
 * You can change from bootloader mode to firmware mode and vice versa. A change
 * from bootloader mode to firmware mode will only take place if the entry function,
 * device identifier and CRC are present and correct.
 * 
 * This function is used by Brick Viewer during flashing. It should not be
 * necessary to call it in a normal user program.
 */
int led_strip_v2_set_bootloader_mode(LEDStripV2 *led_strip_v2, uint8_t mode, uint8_t *ret_status);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Returns the current bootloader mode, see {@link led_strip_v2_set_bootloader_mode}.
 */
int led_strip_v2_get_bootloader_mode(LEDStripV2 *led_strip_v2, uint8_t *ret_mode);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Sets the firmware pointer for {@link led_strip_v2_write_firmware}. The pointer has
 * to be increased by chunks of size 64. The data is written to flash
 * every 4 chunks (which equals to one page of size 256).
 * 
 * This function is used by Brick Viewer during flashing. It should not be
 * necessary to call it in a normal user program.
 */
int led_strip_v2_set_write_firmware_pointer(LEDStripV2 *led_strip_v2, uint32_t pointer);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Writes 64 Bytes of firmware at the position as written by
 * {@link led_strip_v2_set_write_firmware_pointer} before. The firmware is written
 * to flash every 4 chunks.
 * 
 * You can only write firmware in bootloader mode.
 * 
 * This function is used by Brick Viewer during flashing. It should not be
 * necessary to call it in a normal user program.
 */
int led_strip_v2_write_firmware(LEDStripV2 *led_strip_v2, uint8_t data[64], uint8_t *ret_status);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Sets the status LED configuration. By default the LED shows
 * communication traffic between Brick and Bricklet, it flickers once
 * for every 10 received data packets.
 * 
 * You can also turn the LED permanently on/off or show a heartbeat.
 * 
 * If the Bricklet is in bootloader mode, the LED is will show heartbeat by default.
 */
int led_strip_v2_set_status_led_config(LEDStripV2 *led_strip_v2, uint8_t config);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Returns the configuration as set by {@link led_strip_v2_set_status_led_config}
 */
int led_strip_v2_get_status_led_config(LEDStripV2 *led_strip_v2, uint8_t *ret_config);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Returns the temperature as measured inside the microcontroller. The
 * value returned is not the ambient temperature!
 * 
 * The temperature is only proportional to the real temperature and it has bad
 * accuracy. Practically it is only useful as an indicator for
 * temperature changes.
 */
int led_strip_v2_get_chip_temperature(LEDStripV2 *led_strip_v2, int16_t *ret_temperature);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Calling this function will reset the Bricklet. All configurations
 * will be lost.
 * 
 * After a reset you have to create new device objects,
 * calling functions on the existing ones will result in
 * undefined behavior!
 */
int led_strip_v2_reset(LEDStripV2 *led_strip_v2);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Writes a new UID into flash. If you want to set a new UID
 * you have to decode the Base58 encoded UID string into an
 * integer first.
 * 
 * We recommend that you use Brick Viewer to change the UID.
 */
int led_strip_v2_write_uid(LEDStripV2 *led_strip_v2, uint32_t uid);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Returns the current UID as an integer. Encode as
 * Base58 to get the usual string version.
 */
int led_strip_v2_read_uid(LEDStripV2 *led_strip_v2, uint32_t *ret_uid);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Returns the UID, the UID where the Bricklet is connected to,
 * the position, the hardware and firmware version as well as the
 * device identifier.
 * 
 * The position can be 'a', 'b', 'c', 'd', 'e', 'f', 'g' or 'h' (Bricklet Port).
 * A Bricklet connected to an :ref:`Isolator Bricklet <isolator_bricklet>` is always at
 * position 'z'.
 * 
 * The device identifier numbers can be found :ref:`here <device_identifier>`.
 * |device_identifier_constant|
 */
int led_strip_v2_get_identity(LEDStripV2 *led_strip_v2, char ret_uid[8], char ret_connected_uid[8], char *ret_position, uint8_t ret_hardware_version[3], uint8_t ret_firmware_version[3], uint16_t *ret_device_identifier);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Sets the RGB(W) values for the LEDs starting from *index*.
 * You can set at most 2048 RGB values or 1536 RGBW values (6144 byte each).
 * 
 * To make the colors show correctly you need to configure the chip type
 * (see {@link led_strip_v2_set_chip_type}) and a channel mapping (see {@link led_strip_v2_set_channel_mapping})
 * according to the connected LEDs.
 * 
 * If the channel mapping has 3 colors, you need to give the data in the sequence
 * RGBRGBRGB... if the channel mapping has 4 colors you need to give data in the
 * sequence RGBWRGBWRGBW...
 * 
 * The data is double buffered and the colors will be transfered to the
 * LEDs when the next frame duration ends (see {@link led_strip_v2_set_frame_duration}).
 * 
 * Generic approach:
 * 
 * * Set the frame duration to a value that represents the number of frames per
 *   second you want to achieve.
 * * Set all of the LED colors for one frame.
 * * Wait for the {@link LED_STRIP_V2_CALLBACK_FRAME_STARTED} callback.
 * * Set all of the LED colors for next frame.
 * * Wait for the {@link LED_STRIP_V2_CALLBACK_FRAME_STARTED} callback.
 * * And so on.
 * 
 * This approach ensures that you can change the LED colors with a fixed frame rate.
 */
int led_strip_v2_set_led_values(LEDStripV2 *led_strip_v2, uint16_t index, uint8_t *value, uint16_t value_length);

/**
 * \ingroup BrickletLEDStripV2
 *
 * Returns *length* RGB(W) values starting from the
 * given *index*.
 * 
 * If the channel mapping has 3 colors, you will get the data in the sequence
 * RGBRGBRGB... if the channel mapping has 4 colors you will get the data in the
 * sequence RGBWRGBWRGBW...
 * (assuming you start at an index divisible by 3 (RGB) or 4 (RGBW)).
 */
int led_strip_v2_get_led_values(LEDStripV2 *led_strip_v2, uint16_t index, uint16_t length, uint8_t *ret_value, uint16_t *ret_value_length);

#ifdef __cplusplus
}
#endif

#endif
