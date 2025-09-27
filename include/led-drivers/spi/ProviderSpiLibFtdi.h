#pragma once

#include <led-drivers/LedDevice.h>
#include <led-drivers/spi/ProviderSpiInterface.h>

#include <led-drivers/spi/ftdi/ftdi.h>
#include <led-drivers/spi/ftdi/libusb.h>

typedef struct ftdi_context* (*PTR_ftdi_new)(void);
typedef int (*PTR_ftdi_usb_open_bus_addr)(struct ftdi_context* ftdi, uint8_t bus, uint8_t addr);
typedef void (*PTR_ftdi_free)(struct ftdi_context* ftdi);
typedef int (*PTR_ftdi_usb_reset)(struct ftdi_context* ftdi);
typedef int (*PTR_ftdi_set_baudrate)(struct ftdi_context* ftdi, int baudrate);
typedef int (*PTR_ftdi_write_data_set_chunksize)(struct ftdi_context* ftdi, unsigned int chunksize);
typedef int (*PTR_ftdi_set_event_char)(struct ftdi_context* ftdi, unsigned char eventch, unsigned char enable);
typedef int (*PTR_ftdi_set_error_char)(struct ftdi_context* ftdi, unsigned char errorch, unsigned char enable);
typedef int (*PTR_ftdi_set_latency_timer)(struct ftdi_context* ftdi, unsigned char latency);
typedef int (*PTR_ftdi_setflowctrl)(struct ftdi_context* ftdi, int flowctrl);
typedef int (*PTR_ftdi_set_bitmode)(struct ftdi_context* ftdi, unsigned char bitmask, unsigned char mode);
typedef int (*PTR_ftdi_write_data)(struct ftdi_context* ftdi, const unsigned char* buf, int size);
typedef int (*PTR_ftdi_usb_close)(struct ftdi_context* ftdi);
typedef int (*PTR_ftdi_usb_find_all)(struct ftdi_context* ftdi, struct ftdi_device_list** devlist, int vendor, int product);
typedef void (*PTR_ftdi_list_free)(struct ftdi_device_list** devlist);
typedef const char* (*PTR_ftdi_get_error_string)(struct ftdi_context* ftdi);


class ProviderSpiLibFtdi : public QObject, public ProviderSpiInterface
{
	void*					_dllHandle;
	struct ftdi_context*	_deviceHandle;

	PTR_ftdi_new				_fun_ftdi_new;
	PTR_ftdi_usb_open_bus_addr	_fun_ftdi_usb_open_bus_addr;
	PTR_ftdi_free				_fun_ftdi_free;
	PTR_ftdi_usb_reset			_fun_ftdi_usb_reset;
	PTR_ftdi_set_baudrate		_fun_ftdi_set_baudrate;
	PTR_ftdi_write_data_set_chunksize _fun_ftdi_write_data_set_chunksize;
	PTR_ftdi_set_event_char		_fun_ftdi_set_event_char;
	PTR_ftdi_set_error_char		_fun_ftdi_set_error_char;
	PTR_ftdi_set_latency_timer	_fun_ftdi_set_latency_timer;
	PTR_ftdi_setflowctrl		_fun_ftdi_setflowctrl;
	PTR_ftdi_set_bitmode		_fun_ftdi_set_bitmode;
	PTR_ftdi_write_data			_fun_ftdi_write_data;
	PTR_ftdi_usb_close			_fun_ftdi_usb_close;
	PTR_ftdi_usb_find_all		_fun_ftdi_usb_find_all;
	PTR_ftdi_list_free			_fun_ftdi_list_free;
	PTR_ftdi_get_error_string	_fun_ftdi_get_error_string;

	bool loadLibrary();

public:
	ProviderSpiLibFtdi(Logger* _log);
	~ProviderSpiLibFtdi();

public:
	bool init(QJsonObject deviceConfig) override;

	QString open() override;
	int close() override;

	QJsonObject discover(const QJsonObject& params) override;

	int writeBytes(unsigned size, const uint8_t* data) override;
	int getRate() override;
	QString getSpiType() override;
	ProviderSpiInterface::SpiProvider getProviderType() override;
};
