/* ***********************************************************
 * This file was automatically generated on 2021-05-06.      *
 *                                                           *
 * C/C++ Bindings Version 2.1.32                             *
 *                                                           *
 * If you have a bugfix for this file and want to commit it, *
 * please fix the bug in the generator. You can find a link  *
 * to the generators git repository on tinkerforge.com       *
 *************************************************************/


#define IPCON_EXPOSE_INTERNALS

#include "bricklet_led_strip_v2.h"

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif



typedef void (*FrameStarted_CallbackFunction)(uint16_t length, void *user_data);

#if defined _MSC_VER || defined __BORLANDC__
	#pragma pack(push)
	#pragma pack(1)
	#define ATTRIBUTE_PACKED
#elif defined __GNUC__
	#ifdef _WIN32
		// workaround struct packing bug in GCC 4.7 on Windows
		// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=52991
		#define ATTRIBUTE_PACKED __attribute__((gcc_struct, packed))
	#else
		#define ATTRIBUTE_PACKED __attribute__((packed))
	#endif
#else
	#error unknown compiler, do not know how to enable struct packing
#endif

typedef struct {
	PacketHeader header;
	uint16_t index;
	uint16_t value_length;
	uint16_t value_chunk_offset;
	uint8_t value_chunk_data[58];
} ATTRIBUTE_PACKED SetLEDValuesLowLevel_Request;

typedef struct {
	PacketHeader header;
	uint16_t index;
	uint16_t length;
} ATTRIBUTE_PACKED GetLEDValuesLowLevel_Request;

typedef struct {
	PacketHeader header;
	uint16_t value_length;
	uint16_t value_chunk_offset;
	uint8_t value_chunk_data[60];
} ATTRIBUTE_PACKED GetLEDValuesLowLevel_Response;

typedef struct {
	PacketHeader header;
	uint16_t duration;
} ATTRIBUTE_PACKED SetFrameDuration_Request;

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED GetFrameDuration_Request;

typedef struct {
	PacketHeader header;
	uint16_t duration;
} ATTRIBUTE_PACKED GetFrameDuration_Response;

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED GetSupplyVoltage_Request;

typedef struct {
	PacketHeader header;
	uint16_t voltage;
} ATTRIBUTE_PACKED GetSupplyVoltage_Response;

typedef struct {
	PacketHeader header;
	uint16_t length;
} ATTRIBUTE_PACKED FrameStarted_Callback;

typedef struct {
	PacketHeader header;
	uint32_t frequency;
} ATTRIBUTE_PACKED SetClockFrequency_Request;

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED GetClockFrequency_Request;

typedef struct {
	PacketHeader header;
	uint32_t frequency;
} ATTRIBUTE_PACKED GetClockFrequency_Response;

typedef struct {
	PacketHeader header;
	uint16_t chip;
} ATTRIBUTE_PACKED SetChipType_Request;

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED GetChipType_Request;

typedef struct {
	PacketHeader header;
	uint16_t chip;
} ATTRIBUTE_PACKED GetChipType_Response;

typedef struct {
	PacketHeader header;
	uint8_t mapping;
} ATTRIBUTE_PACKED SetChannelMapping_Request;

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED GetChannelMapping_Request;

typedef struct {
	PacketHeader header;
	uint8_t mapping;
} ATTRIBUTE_PACKED GetChannelMapping_Response;

typedef struct {
	PacketHeader header;
	uint8_t enable;
} ATTRIBUTE_PACKED SetFrameStartedCallbackConfiguration_Request;

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED GetFrameStartedCallbackConfiguration_Request;

typedef struct {
	PacketHeader header;
	uint8_t enable;
} ATTRIBUTE_PACKED GetFrameStartedCallbackConfiguration_Response;

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED GetSPITFPErrorCount_Request;

typedef struct {
	PacketHeader header;
	uint32_t error_count_ack_checksum;
	uint32_t error_count_message_checksum;
	uint32_t error_count_frame;
	uint32_t error_count_overflow;
} ATTRIBUTE_PACKED GetSPITFPErrorCount_Response;

typedef struct {
	PacketHeader header;
	uint8_t mode;
} ATTRIBUTE_PACKED SetBootloaderMode_Request;

typedef struct {
	PacketHeader header;
	uint8_t status;
} ATTRIBUTE_PACKED SetBootloaderMode_Response;

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED GetBootloaderMode_Request;

typedef struct {
	PacketHeader header;
	uint8_t mode;
} ATTRIBUTE_PACKED GetBootloaderMode_Response;

typedef struct {
	PacketHeader header;
	uint32_t pointer;
} ATTRIBUTE_PACKED SetWriteFirmwarePointer_Request;

typedef struct {
	PacketHeader header;
	uint8_t data[64];
} ATTRIBUTE_PACKED WriteFirmware_Request;

typedef struct {
	PacketHeader header;
	uint8_t status;
} ATTRIBUTE_PACKED WriteFirmware_Response;

typedef struct {
	PacketHeader header;
	uint8_t config;
} ATTRIBUTE_PACKED SetStatusLEDConfig_Request;

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED GetStatusLEDConfig_Request;

typedef struct {
	PacketHeader header;
	uint8_t config;
} ATTRIBUTE_PACKED GetStatusLEDConfig_Response;

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED GetChipTemperature_Request;

typedef struct {
	PacketHeader header;
	int16_t temperature;
} ATTRIBUTE_PACKED GetChipTemperature_Response;

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED Reset_Request;

typedef struct {
	PacketHeader header;
	uint32_t uid;
} ATTRIBUTE_PACKED WriteUID_Request;

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED ReadUID_Request;

typedef struct {
	PacketHeader header;
	uint32_t uid;
} ATTRIBUTE_PACKED ReadUID_Response;

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED GetIdentity_Request;

typedef struct {
	PacketHeader header;
	char uid[8];
	char connected_uid[8];
	char position;
	uint8_t hardware_version[3];
	uint8_t firmware_version[3];
	uint16_t device_identifier;
} ATTRIBUTE_PACKED GetIdentity_Response;

#if defined _MSC_VER || defined __BORLANDC__
	#pragma pack(pop)
#endif
#undef ATTRIBUTE_PACKED

static void led_strip_v2_callback_wrapper_frame_started(DevicePrivate *device_p, Packet *packet) {
	FrameStarted_CallbackFunction callback_function;
	void *user_data;
	FrameStarted_Callback *callback;

	if (packet->header.length != sizeof(FrameStarted_Callback)) {
		return; // silently ignoring callback with wrong length
	}

	callback_function = (FrameStarted_CallbackFunction)device_p->registered_callbacks[DEVICE_NUM_FUNCTION_IDS + LED_STRIP_V2_CALLBACK_FRAME_STARTED];
	user_data = device_p->registered_callback_user_data[DEVICE_NUM_FUNCTION_IDS + LED_STRIP_V2_CALLBACK_FRAME_STARTED];
	callback = (FrameStarted_Callback *)packet;
	(void)callback; // avoid unused variable warning

	if (callback_function == NULL) {
		return;
	}

	callback->length = leconvert_uint16_from(callback->length);

	callback_function(callback->length, user_data);
}

void led_strip_v2_create(LEDStripV2 *led_strip_v2, const char *uid, IPConnection *ipcon) {
	IPConnectionPrivate *ipcon_p = ipcon->p;
	DevicePrivate *device_p;

	device_create(led_strip_v2, uid, ipcon_p, 2, 0, 0, LED_STRIP_V2_DEVICE_IDENTIFIER);

	device_p = led_strip_v2->p;

	device_p->response_expected[LED_STRIP_V2_FUNCTION_SET_LED_VALUES_LOW_LEVEL] = DEVICE_RESPONSE_EXPECTED_TRUE;
	device_p->response_expected[LED_STRIP_V2_FUNCTION_GET_LED_VALUES_LOW_LEVEL] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[LED_STRIP_V2_FUNCTION_SET_FRAME_DURATION] = DEVICE_RESPONSE_EXPECTED_FALSE;
	device_p->response_expected[LED_STRIP_V2_FUNCTION_GET_FRAME_DURATION] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[LED_STRIP_V2_FUNCTION_GET_SUPPLY_VOLTAGE] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[LED_STRIP_V2_FUNCTION_SET_CLOCK_FREQUENCY] = DEVICE_RESPONSE_EXPECTED_FALSE;
	device_p->response_expected[LED_STRIP_V2_FUNCTION_GET_CLOCK_FREQUENCY] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[LED_STRIP_V2_FUNCTION_SET_CHIP_TYPE] = DEVICE_RESPONSE_EXPECTED_FALSE;
	device_p->response_expected[LED_STRIP_V2_FUNCTION_GET_CHIP_TYPE] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[LED_STRIP_V2_FUNCTION_SET_CHANNEL_MAPPING] = DEVICE_RESPONSE_EXPECTED_FALSE;
	device_p->response_expected[LED_STRIP_V2_FUNCTION_GET_CHANNEL_MAPPING] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[LED_STRIP_V2_FUNCTION_SET_FRAME_STARTED_CALLBACK_CONFIGURATION] = DEVICE_RESPONSE_EXPECTED_TRUE;
	device_p->response_expected[LED_STRIP_V2_FUNCTION_GET_FRAME_STARTED_CALLBACK_CONFIGURATION] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[LED_STRIP_V2_FUNCTION_GET_SPITFP_ERROR_COUNT] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[LED_STRIP_V2_FUNCTION_SET_BOOTLOADER_MODE] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[LED_STRIP_V2_FUNCTION_GET_BOOTLOADER_MODE] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[LED_STRIP_V2_FUNCTION_SET_WRITE_FIRMWARE_POINTER] = DEVICE_RESPONSE_EXPECTED_FALSE;
	device_p->response_expected[LED_STRIP_V2_FUNCTION_WRITE_FIRMWARE] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[LED_STRIP_V2_FUNCTION_SET_STATUS_LED_CONFIG] = DEVICE_RESPONSE_EXPECTED_FALSE;
	device_p->response_expected[LED_STRIP_V2_FUNCTION_GET_STATUS_LED_CONFIG] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[LED_STRIP_V2_FUNCTION_GET_CHIP_TEMPERATURE] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[LED_STRIP_V2_FUNCTION_RESET] = DEVICE_RESPONSE_EXPECTED_FALSE;
	device_p->response_expected[LED_STRIP_V2_FUNCTION_WRITE_UID] = DEVICE_RESPONSE_EXPECTED_FALSE;
	device_p->response_expected[LED_STRIP_V2_FUNCTION_READ_UID] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[LED_STRIP_V2_FUNCTION_GET_IDENTITY] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;

	device_p->callback_wrappers[LED_STRIP_V2_CALLBACK_FRAME_STARTED] = led_strip_v2_callback_wrapper_frame_started;

	ipcon_add_device(ipcon_p, device_p);
}

void led_strip_v2_destroy(LEDStripV2 *led_strip_v2) {
	device_release(led_strip_v2->p);
}

int led_strip_v2_get_response_expected(LEDStripV2 *led_strip_v2, uint8_t function_id, bool *ret_response_expected) {
	return device_get_response_expected(led_strip_v2->p, function_id, ret_response_expected);
}

int led_strip_v2_set_response_expected(LEDStripV2 *led_strip_v2, uint8_t function_id, bool response_expected) {
	return device_set_response_expected(led_strip_v2->p, function_id, response_expected);
}

int led_strip_v2_set_response_expected_all(LEDStripV2 *led_strip_v2, bool response_expected) {
	return device_set_response_expected_all(led_strip_v2->p, response_expected);
}

void led_strip_v2_register_callback(LEDStripV2 *led_strip_v2, int16_t callback_id, void (*function)(void), void *user_data) {
	device_register_callback(led_strip_v2->p, callback_id, function, user_data);
}

int led_strip_v2_get_api_version(LEDStripV2 *led_strip_v2, uint8_t ret_api_version[3]) {
	return device_get_api_version(led_strip_v2->p, ret_api_version);
}

int led_strip_v2_set_led_values_low_level(LEDStripV2 *led_strip_v2, uint16_t index, uint16_t value_length, uint16_t value_chunk_offset, uint8_t value_chunk_data[58]) {
	DevicePrivate *device_p = led_strip_v2->p;
	SetLEDValuesLowLevel_Request request;
	int ret;

	ret = device_check_validity(device_p);

	if (ret < 0) {
		return ret;
	}

	ret = packet_header_create(&request.header, sizeof(request), LED_STRIP_V2_FUNCTION_SET_LED_VALUES_LOW_LEVEL, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.index = leconvert_uint16_to(index);
	request.value_length = leconvert_uint16_to(value_length);
	request.value_chunk_offset = leconvert_uint16_to(value_chunk_offset);
	memcpy(request.value_chunk_data, value_chunk_data, 58 * sizeof(uint8_t));

	ret = device_send_request(device_p, (Packet *)&request, NULL, 0);

	return ret;
}

int led_strip_v2_get_led_values_low_level(LEDStripV2 *led_strip_v2, uint16_t index, uint16_t length, uint16_t *ret_value_length, uint16_t *ret_value_chunk_offset, uint8_t ret_value_chunk_data[60]) {
	DevicePrivate *device_p = led_strip_v2->p;
	GetLEDValuesLowLevel_Request request;
	GetLEDValuesLowLevel_Response response;
	int ret;

	ret = device_check_validity(device_p);

	if (ret < 0) {
		return ret;
	}

	ret = packet_header_create(&request.header, sizeof(request), LED_STRIP_V2_FUNCTION_GET_LED_VALUES_LOW_LEVEL, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.index = leconvert_uint16_to(index);
	request.length = leconvert_uint16_to(length);

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response, sizeof(response));

	if (ret < 0) {
		return ret;
	}

	*ret_value_length = leconvert_uint16_from(response.value_length);
	*ret_value_chunk_offset = leconvert_uint16_from(response.value_chunk_offset);
	memcpy(ret_value_chunk_data, response.value_chunk_data, 60 * sizeof(uint8_t));

	return ret;
}

int led_strip_v2_set_frame_duration(LEDStripV2 *led_strip_v2, uint16_t duration) {
	DevicePrivate *device_p = led_strip_v2->p;
	SetFrameDuration_Request request;
	int ret;

	ret = device_check_validity(device_p);

	if (ret < 0) {
		return ret;
	}

	ret = packet_header_create(&request.header, sizeof(request), LED_STRIP_V2_FUNCTION_SET_FRAME_DURATION, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.duration = leconvert_uint16_to(duration);

	ret = device_send_request(device_p, (Packet *)&request, NULL, 0);

	return ret;
}

int led_strip_v2_get_frame_duration(LEDStripV2 *led_strip_v2, uint16_t *ret_duration) {
	DevicePrivate *device_p = led_strip_v2->p;
	GetFrameDuration_Request request;
	GetFrameDuration_Response response;
	int ret;

	ret = device_check_validity(device_p);

	if (ret < 0) {
		return ret;
	}

	ret = packet_header_create(&request.header, sizeof(request), LED_STRIP_V2_FUNCTION_GET_FRAME_DURATION, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response, sizeof(response));

	if (ret < 0) {
		return ret;
	}

	*ret_duration = leconvert_uint16_from(response.duration);

	return ret;
}

int led_strip_v2_get_supply_voltage(LEDStripV2 *led_strip_v2, uint16_t *ret_voltage) {
	DevicePrivate *device_p = led_strip_v2->p;
	GetSupplyVoltage_Request request;
	GetSupplyVoltage_Response response;
	int ret;

	ret = device_check_validity(device_p);

	if (ret < 0) {
		return ret;
	}

	ret = packet_header_create(&request.header, sizeof(request), LED_STRIP_V2_FUNCTION_GET_SUPPLY_VOLTAGE, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response, sizeof(response));

	if (ret < 0) {
		return ret;
	}

	*ret_voltage = leconvert_uint16_from(response.voltage);

	return ret;
}

int led_strip_v2_set_clock_frequency(LEDStripV2 *led_strip_v2, uint32_t frequency) {
	DevicePrivate *device_p = led_strip_v2->p;
	SetClockFrequency_Request request;
	int ret;

	ret = device_check_validity(device_p);

	if (ret < 0) {
		return ret;
	}

	ret = packet_header_create(&request.header, sizeof(request), LED_STRIP_V2_FUNCTION_SET_CLOCK_FREQUENCY, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.frequency = leconvert_uint32_to(frequency);

	ret = device_send_request(device_p, (Packet *)&request, NULL, 0);

	return ret;
}

int led_strip_v2_get_clock_frequency(LEDStripV2 *led_strip_v2, uint32_t *ret_frequency) {
	DevicePrivate *device_p = led_strip_v2->p;
	GetClockFrequency_Request request;
	GetClockFrequency_Response response;
	int ret;

	ret = device_check_validity(device_p);

	if (ret < 0) {
		return ret;
	}

	ret = packet_header_create(&request.header, sizeof(request), LED_STRIP_V2_FUNCTION_GET_CLOCK_FREQUENCY, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response, sizeof(response));

	if (ret < 0) {
		return ret;
	}

	*ret_frequency = leconvert_uint32_from(response.frequency);

	return ret;
}

int led_strip_v2_set_chip_type(LEDStripV2 *led_strip_v2, uint16_t chip) {
	DevicePrivate *device_p = led_strip_v2->p;
	SetChipType_Request request;
	int ret;

	ret = device_check_validity(device_p);

	if (ret < 0) {
		return ret;
	}

	ret = packet_header_create(&request.header, sizeof(request), LED_STRIP_V2_FUNCTION_SET_CHIP_TYPE, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.chip = leconvert_uint16_to(chip);

	ret = device_send_request(device_p, (Packet *)&request, NULL, 0);

	return ret;
}

int led_strip_v2_get_chip_type(LEDStripV2 *led_strip_v2, uint16_t *ret_chip) {
	DevicePrivate *device_p = led_strip_v2->p;
	GetChipType_Request request;
	GetChipType_Response response;
	int ret;

	ret = device_check_validity(device_p);

	if (ret < 0) {
		return ret;
	}

	ret = packet_header_create(&request.header, sizeof(request), LED_STRIP_V2_FUNCTION_GET_CHIP_TYPE, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response, sizeof(response));

	if (ret < 0) {
		return ret;
	}

	*ret_chip = leconvert_uint16_from(response.chip);

	return ret;
}

int led_strip_v2_set_channel_mapping(LEDStripV2 *led_strip_v2, uint8_t mapping) {
	DevicePrivate *device_p = led_strip_v2->p;
	SetChannelMapping_Request request;
	int ret;

	ret = device_check_validity(device_p);

	if (ret < 0) {
		return ret;
	}

	ret = packet_header_create(&request.header, sizeof(request), LED_STRIP_V2_FUNCTION_SET_CHANNEL_MAPPING, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.mapping = mapping;

	ret = device_send_request(device_p, (Packet *)&request, NULL, 0);

	return ret;
}

int led_strip_v2_get_channel_mapping(LEDStripV2 *led_strip_v2, uint8_t *ret_mapping) {
	DevicePrivate *device_p = led_strip_v2->p;
	GetChannelMapping_Request request;
	GetChannelMapping_Response response;
	int ret;

	ret = device_check_validity(device_p);

	if (ret < 0) {
		return ret;
	}

	ret = packet_header_create(&request.header, sizeof(request), LED_STRIP_V2_FUNCTION_GET_CHANNEL_MAPPING, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response, sizeof(response));

	if (ret < 0) {
		return ret;
	}

	*ret_mapping = response.mapping;

	return ret;
}

int led_strip_v2_set_frame_started_callback_configuration(LEDStripV2 *led_strip_v2, bool enable) {
	DevicePrivate *device_p = led_strip_v2->p;
	SetFrameStartedCallbackConfiguration_Request request;
	int ret;

	ret = device_check_validity(device_p);

	if (ret < 0) {
		return ret;
	}

	ret = packet_header_create(&request.header, sizeof(request), LED_STRIP_V2_FUNCTION_SET_FRAME_STARTED_CALLBACK_CONFIGURATION, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.enable = enable ? 1 : 0;

	ret = device_send_request(device_p, (Packet *)&request, NULL, 0);

	return ret;
}

int led_strip_v2_get_frame_started_callback_configuration(LEDStripV2 *led_strip_v2, bool *ret_enable) {
	DevicePrivate *device_p = led_strip_v2->p;
	GetFrameStartedCallbackConfiguration_Request request;
	GetFrameStartedCallbackConfiguration_Response response;
	int ret;

	ret = device_check_validity(device_p);

	if (ret < 0) {
		return ret;
	}

	ret = packet_header_create(&request.header, sizeof(request), LED_STRIP_V2_FUNCTION_GET_FRAME_STARTED_CALLBACK_CONFIGURATION, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response, sizeof(response));

	if (ret < 0) {
		return ret;
	}

	*ret_enable = response.enable != 0;

	return ret;
}

int led_strip_v2_get_spitfp_error_count(LEDStripV2 *led_strip_v2, uint32_t *ret_error_count_ack_checksum, uint32_t *ret_error_count_message_checksum, uint32_t *ret_error_count_frame, uint32_t *ret_error_count_overflow) {
	DevicePrivate *device_p = led_strip_v2->p;
	GetSPITFPErrorCount_Request request;
	GetSPITFPErrorCount_Response response;
	int ret;

	ret = device_check_validity(device_p);

	if (ret < 0) {
		return ret;
	}

	ret = packet_header_create(&request.header, sizeof(request), LED_STRIP_V2_FUNCTION_GET_SPITFP_ERROR_COUNT, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response, sizeof(response));

	if (ret < 0) {
		return ret;
	}

	*ret_error_count_ack_checksum = leconvert_uint32_from(response.error_count_ack_checksum);
	*ret_error_count_message_checksum = leconvert_uint32_from(response.error_count_message_checksum);
	*ret_error_count_frame = leconvert_uint32_from(response.error_count_frame);
	*ret_error_count_overflow = leconvert_uint32_from(response.error_count_overflow);

	return ret;
}

int led_strip_v2_set_bootloader_mode(LEDStripV2 *led_strip_v2, uint8_t mode, uint8_t *ret_status) {
	DevicePrivate *device_p = led_strip_v2->p;
	SetBootloaderMode_Request request;
	SetBootloaderMode_Response response;
	int ret;

	ret = device_check_validity(device_p);

	if (ret < 0) {
		return ret;
	}

	ret = packet_header_create(&request.header, sizeof(request), LED_STRIP_V2_FUNCTION_SET_BOOTLOADER_MODE, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.mode = mode;

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response, sizeof(response));

	if (ret < 0) {
		return ret;
	}

	*ret_status = response.status;

	return ret;
}

int led_strip_v2_get_bootloader_mode(LEDStripV2 *led_strip_v2, uint8_t *ret_mode) {
	DevicePrivate *device_p = led_strip_v2->p;
	GetBootloaderMode_Request request;
	GetBootloaderMode_Response response;
	int ret;

	ret = device_check_validity(device_p);

	if (ret < 0) {
		return ret;
	}

	ret = packet_header_create(&request.header, sizeof(request), LED_STRIP_V2_FUNCTION_GET_BOOTLOADER_MODE, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response, sizeof(response));

	if (ret < 0) {
		return ret;
	}

	*ret_mode = response.mode;

	return ret;
}

int led_strip_v2_set_write_firmware_pointer(LEDStripV2 *led_strip_v2, uint32_t pointer) {
	DevicePrivate *device_p = led_strip_v2->p;
	SetWriteFirmwarePointer_Request request;
	int ret;

	ret = device_check_validity(device_p);

	if (ret < 0) {
		return ret;
	}

	ret = packet_header_create(&request.header, sizeof(request), LED_STRIP_V2_FUNCTION_SET_WRITE_FIRMWARE_POINTER, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.pointer = leconvert_uint32_to(pointer);

	ret = device_send_request(device_p, (Packet *)&request, NULL, 0);

	return ret;
}

int led_strip_v2_write_firmware(LEDStripV2 *led_strip_v2, uint8_t data[64], uint8_t *ret_status) {
	DevicePrivate *device_p = led_strip_v2->p;
	WriteFirmware_Request request;
	WriteFirmware_Response response;
	int ret;

	ret = device_check_validity(device_p);

	if (ret < 0) {
		return ret;
	}

	ret = packet_header_create(&request.header, sizeof(request), LED_STRIP_V2_FUNCTION_WRITE_FIRMWARE, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	memcpy(request.data, data, 64 * sizeof(uint8_t));

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response, sizeof(response));

	if (ret < 0) {
		return ret;
	}

	*ret_status = response.status;

	return ret;
}

int led_strip_v2_set_status_led_config(LEDStripV2 *led_strip_v2, uint8_t config) {
	DevicePrivate *device_p = led_strip_v2->p;
	SetStatusLEDConfig_Request request;
	int ret;

	ret = device_check_validity(device_p);

	if (ret < 0) {
		return ret;
	}

	ret = packet_header_create(&request.header, sizeof(request), LED_STRIP_V2_FUNCTION_SET_STATUS_LED_CONFIG, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.config = config;

	ret = device_send_request(device_p, (Packet *)&request, NULL, 0);

	return ret;
}

int led_strip_v2_get_status_led_config(LEDStripV2 *led_strip_v2, uint8_t *ret_config) {
	DevicePrivate *device_p = led_strip_v2->p;
	GetStatusLEDConfig_Request request;
	GetStatusLEDConfig_Response response;
	int ret;

	ret = device_check_validity(device_p);

	if (ret < 0) {
		return ret;
	}

	ret = packet_header_create(&request.header, sizeof(request), LED_STRIP_V2_FUNCTION_GET_STATUS_LED_CONFIG, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response, sizeof(response));

	if (ret < 0) {
		return ret;
	}

	*ret_config = response.config;

	return ret;
}

int led_strip_v2_get_chip_temperature(LEDStripV2 *led_strip_v2, int16_t *ret_temperature) {
	DevicePrivate *device_p = led_strip_v2->p;
	GetChipTemperature_Request request;
	GetChipTemperature_Response response;
	int ret;

	ret = device_check_validity(device_p);

	if (ret < 0) {
		return ret;
	}

	ret = packet_header_create(&request.header, sizeof(request), LED_STRIP_V2_FUNCTION_GET_CHIP_TEMPERATURE, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response, sizeof(response));

	if (ret < 0) {
		return ret;
	}

	*ret_temperature = leconvert_int16_from(response.temperature);

	return ret;
}

int led_strip_v2_reset(LEDStripV2 *led_strip_v2) {
	DevicePrivate *device_p = led_strip_v2->p;
	Reset_Request request;
	int ret;

	ret = device_check_validity(device_p);

	if (ret < 0) {
		return ret;
	}

	ret = packet_header_create(&request.header, sizeof(request), LED_STRIP_V2_FUNCTION_RESET, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	ret = device_send_request(device_p, (Packet *)&request, NULL, 0);

	return ret;
}

int led_strip_v2_write_uid(LEDStripV2 *led_strip_v2, uint32_t uid) {
	DevicePrivate *device_p = led_strip_v2->p;
	WriteUID_Request request;
	int ret;

	ret = device_check_validity(device_p);

	if (ret < 0) {
		return ret;
	}

	ret = packet_header_create(&request.header, sizeof(request), LED_STRIP_V2_FUNCTION_WRITE_UID, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.uid = leconvert_uint32_to(uid);

	ret = device_send_request(device_p, (Packet *)&request, NULL, 0);

	return ret;
}

int led_strip_v2_read_uid(LEDStripV2 *led_strip_v2, uint32_t *ret_uid) {
	DevicePrivate *device_p = led_strip_v2->p;
	ReadUID_Request request;
	ReadUID_Response response;
	int ret;

	ret = device_check_validity(device_p);

	if (ret < 0) {
		return ret;
	}

	ret = packet_header_create(&request.header, sizeof(request), LED_STRIP_V2_FUNCTION_READ_UID, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response, sizeof(response));

	if (ret < 0) {
		return ret;
	}

	*ret_uid = leconvert_uint32_from(response.uid);

	return ret;
}

int led_strip_v2_get_identity(LEDStripV2 *led_strip_v2, char ret_uid[8], char ret_connected_uid[8], char *ret_position, uint8_t ret_hardware_version[3], uint8_t ret_firmware_version[3], uint16_t *ret_device_identifier) {
	DevicePrivate *device_p = led_strip_v2->p;
	GetIdentity_Request request;
	GetIdentity_Response response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), LED_STRIP_V2_FUNCTION_GET_IDENTITY, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response, sizeof(response));

	if (ret < 0) {
		return ret;
	}

	memcpy(ret_uid, response.uid, 8);
	memcpy(ret_connected_uid, response.connected_uid, 8);
	*ret_position = response.position;
	memcpy(ret_hardware_version, response.hardware_version, 3 * sizeof(uint8_t));
	memcpy(ret_firmware_version, response.firmware_version, 3 * sizeof(uint8_t));
	*ret_device_identifier = leconvert_uint16_from(response.device_identifier);

	return ret;
}

int led_strip_v2_set_led_values(LEDStripV2 *led_strip_v2, uint16_t index, uint8_t *value, uint16_t value_length) {
	DevicePrivate *device_p = led_strip_v2->p;
	int ret = 0;
	uint16_t value_chunk_offset = 0;
	uint8_t value_chunk_data[58];
	uint16_t value_chunk_length;

	if (value_length == 0) {
		memset(&value_chunk_data, 0, sizeof(uint8_t) * 58);

		ret = led_strip_v2_set_led_values_low_level(led_strip_v2, index, value_length, value_chunk_offset, value_chunk_data);
	} else {
		mutex_lock(&device_p->stream_mutex);

		while (value_chunk_offset < value_length) {
			value_chunk_length = value_length - value_chunk_offset;

			if (value_chunk_length > 58) {
				value_chunk_length = 58;
			}

			memcpy(value_chunk_data, &value[value_chunk_offset], sizeof(uint8_t) * value_chunk_length);
			memset(&value_chunk_data[value_chunk_length], 0, sizeof(uint8_t) * (58 - value_chunk_length));

			ret = led_strip_v2_set_led_values_low_level(led_strip_v2, index, value_length, value_chunk_offset, value_chunk_data);

			if (ret < 0) {
				break;
			}

			value_chunk_offset += 58;
		}

		mutex_unlock(&device_p->stream_mutex);
	}

	return ret;
}

int led_strip_v2_get_led_values(LEDStripV2 *led_strip_v2, uint16_t index, uint16_t length, uint8_t *ret_value, uint16_t *ret_value_length) {
	DevicePrivate *device_p = led_strip_v2->p;
	int ret = 0;
	uint16_t value_length = 0;
	uint16_t value_chunk_offset;
	uint8_t value_chunk_data[60];
	bool value_out_of_sync;
	uint16_t value_chunk_length;

	*ret_value_length = 0;

	mutex_lock(&device_p->stream_mutex);

	ret = led_strip_v2_get_led_values_low_level(led_strip_v2, index, length, &value_length, &value_chunk_offset, value_chunk_data);

	if (ret < 0) {
		goto unlock;
	}

	value_out_of_sync = value_chunk_offset != 0;

	if (!value_out_of_sync) {
		value_chunk_length = value_length - value_chunk_offset;

		if (value_chunk_length > 60) {
			value_chunk_length = 60;
		}

		memcpy(ret_value, value_chunk_data, sizeof(uint8_t) * value_chunk_length);
		*ret_value_length = value_chunk_length;

		while (*ret_value_length < value_length) {
			ret = led_strip_v2_get_led_values_low_level(led_strip_v2, index, length, &value_length, &value_chunk_offset, value_chunk_data);

			if (ret < 0) {
				goto unlock;
			}

			value_out_of_sync = value_chunk_offset != *ret_value_length;

			if (value_out_of_sync) {
				break;
			}

			value_chunk_length = value_length - value_chunk_offset;

			if (value_chunk_length > 60) {
				value_chunk_length = 60;
			}

			memcpy(&ret_value[*ret_value_length], value_chunk_data, sizeof(uint8_t) * value_chunk_length);
			*ret_value_length += value_chunk_length;
		}
	}

	if (value_out_of_sync) {
		*ret_value_length = 0; // return empty array

		// discard remaining stream to bring it back in-sync
		while (value_chunk_offset + 60 < value_length) {
			ret = led_strip_v2_get_led_values_low_level(led_strip_v2, index, length, &value_length, &value_chunk_offset, value_chunk_data);

			if (ret < 0) {
				goto unlock;
			}
		}

		ret = E_STREAM_OUT_OF_SYNC;
	}

unlock:
	mutex_unlock(&device_p->stream_mutex);

	return ret;
}

#ifdef __cplusplus
}
#endif
