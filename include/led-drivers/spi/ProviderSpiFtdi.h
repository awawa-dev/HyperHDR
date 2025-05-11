#pragma once

#define NOMINMAX

#include <led-drivers/LedDevice.h>
#include <led-drivers/spi/ProviderSpiInterface.h>
#include <Windows.h>
#include <led-drivers/spi/ftdi/ftd2xx.h>

typedef FT_STATUS (*PTR_FT_ListDevices)(PVOID pvArg1, PVOID pvArg2, DWORD dwFlags);
typedef FT_STATUS (*PTR_FT_OpenEx)(PVOID pvArg1, DWORD dwFlags, FT_HANDLE* pHandle);
typedef FT_STATUS (*PTR_FT_ResetDevice)(FT_HANDLE ftHandle);
typedef FT_STATUS (*PTR_FT_SetUSBParameters)(FT_HANDLE ftHandle,ULONG ulInTransferSize,ULONG ulOutTransferSize);
typedef FT_STATUS (*PTR_FT_SetChars)(FT_HANDLE ftHandle, UCHAR uEventChar, UCHAR uEventCharEnabled, UCHAR uErrorChar, UCHAR uErrorCharEnabled);
typedef FT_STATUS (*PTR_FT_SetLatencyTimer)(FT_HANDLE ftHandle, UCHAR ucLatency);
typedef FT_STATUS (*PTR_FT_SetFlowControl)(FT_HANDLE ftHandle, USHORT usFlowControl, UCHAR uXonChar, UCHAR uXoffChar);
typedef FT_STATUS (*PTR_FT_SetBitMode)(FT_HANDLE ftHandle, UCHAR ucMask, UCHAR ucEnable);
typedef FT_STATUS (*PTR_FT_Close)(FT_HANDLE ftHandle);
typedef FT_STATUS (*PTR_FT_SetBaudRate)(FT_HANDLE ftHandle, ULONG dwBaudRate);
typedef FT_STATUS (*PTR_FT_Write)(FT_HANDLE ftHandle,LPVOID lpBuffer,DWORD dwBytesToWrite,LPDWORD lpdwBytesWritten);

class ProviderSpiFtdi : public QObject, public ProviderSpiInterface
{
	HMODULE				_dllHandle;
	FT_HANDLE			_deviceHandle;

	PTR_FT_ListDevices		_fun_FT_ListDevices;
	PTR_FT_OpenEx			_fun_FT_OpenEx;
	PTR_FT_Write			_fun_FT_Write;
	PTR_FT_ResetDevice		_fun_FT_ResetDevice;
	PTR_FT_SetChars			_fun_FT_SetChars;
	PTR_FT_SetBitMode		_fun_FT_SetBitMode;
	PTR_FT_Close			_fun_FT_Close;
	PTR_FT_SetBaudRate		_fun_FT_SetBaudRate;
	PTR_FT_SetUSBParameters	_fun_FT_SetUSBParameters;
	PTR_FT_SetLatencyTimer	_fun_FT_SetLatencyTimer;
	PTR_FT_SetFlowControl	_fun_FT_SetFlowControl;	

	bool loadLibrary();

public:
	ProviderSpiFtdi(Logger* _log);
	~ProviderSpiFtdi();

public:
	bool init(const QJsonObject& deviceConfig) override;
	
	QString open() override;
	int close() override;

	QJsonObject discover(const QJsonObject& params) override;

	int writeBytes(unsigned size, const uint8_t* data) override;
	int getRate() override;
	QString getSpiType() override;
	ProviderSpiInterface::SpiProvider getProviderType() override;
};
