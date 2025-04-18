/*++

Copyright (c) 2001-2021 Future Technology Devices International Limited

THIS SOFTWARE IS PROVIDED BY FUTURE TECHNOLOGY DEVICES INTERNATIONAL LIMITED "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
FUTURE TECHNOLOGY DEVICES INTERNATIONAL LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES LOSS OF USE, DATA, OR PROFITS OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

FTDI DRIVERS MAY BE USED ONLY IN CONJUNCTION WITH PRODUCTS BASED ON FTDI PARTS.

FTDI DRIVERS MAY BE DISTRIBUTED IN ANY FORM AS LONG AS LICENSE INFORMATION IS NOT MODIFIED.

IF A CUSTOM VENDOR ID AND/OR PRODUCT ID OR DESCRIPTION STRING ARE USED, IT IS THE
RESPONSIBILITY OF THE PRODUCT MANUFACTURER TO MAINTAIN ANY CHANGES AND SUBSEQUENT WHQL
RE-CERTIFICATION AS A RESULT OF MAKING THESE CHANGES.


Module Name:

ftd2xx.h

Abstract:

Native USB device driver for FTDI FT232x, FT245x, FT2232x, FT4232x, FT2233H and FT4233H devices
FTD2XX library definitions

Environment:

kernel & user mode


--*/

/** @file ftd2xx.h
 */

#ifndef FTD2XX_H
#define FTD2XX_H

#ifdef _WIN32
// Compiling on Windows
#include <windows.h>

// The following ifdef block is the standard way of creating macros
// which make exporting from a DLL simpler.  All files within this DLL
// are compiled with the FTD2XX_EXPORTS symbol defined on the command line.
// This symbol should not be defined on any project that uses this DLL.
// This way any other project whose source files include this file see
// FTD2XX_API functions as being imported from a DLL, whereas this DLL
// sees symbols defined with this macro as being exported.

#ifdef FTD2XX_EXPORTS
#define FTD2XX_API __declspec(dllexport)
#elif defined(FTD2XX_STATIC)
// Avoid decorations when linking statically to D2XX.
#define FTD2XX_API
// Static D2XX depends on these Windows libs:
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#else
#define FTD2XX_API __declspec(dllimport)
#endif

#else // _WIN32
// Compiling on non-Windows platform.
#include "WinTypes.h"
// No decorations needed.
#define FTD2XX_API

#endif // _WIN32

/** @name FT_HANDLE 
 * An opaque value used as a handle to an opened FT device.
 */
typedef PVOID	FT_HANDLE;

/** @{
 * @name FT_STATUS 
 * @details Return status values for API calls. 
 */
typedef ULONG	FT_STATUS;

enum {
	FT_OK,
	FT_INVALID_HANDLE,
	FT_DEVICE_NOT_FOUND,
	FT_DEVICE_NOT_OPENED,
	FT_IO_ERROR,
	FT_INSUFFICIENT_RESOURCES,
	FT_INVALID_PARAMETER,
	FT_INVALID_BAUD_RATE,

	FT_DEVICE_NOT_OPENED_FOR_ERASE,
	FT_DEVICE_NOT_OPENED_FOR_WRITE,
	FT_FAILED_TO_WRITE_DEVICE,
	FT_EEPROM_READ_FAILED,
	FT_EEPROM_WRITE_FAILED,
	FT_EEPROM_ERASE_FAILED,
	FT_EEPROM_NOT_PRESENT,
	FT_EEPROM_NOT_PROGRAMMED,
	FT_INVALID_ARGS,
	FT_NOT_SUPPORTED,
	FT_OTHER_ERROR,
	FT_DEVICE_LIST_NOT_READY,
};
/** @} */

/** @name FT_SUCCESS Macro 
 * Macro to determine success of an API call.
 * @returns Non-zero for successful call.
 */
#define FT_SUCCESS(status) ((status) == FT_OK)

/** @{
 * @name FT_OpenEx Flags
 * @see FT_OpenEx
 */
#define FT_OPEN_BY_SERIAL_NUMBER	1
#define FT_OPEN_BY_DESCRIPTION		2
#define FT_OPEN_BY_LOCATION			4

#define FT_OPEN_MASK (FT_OPEN_BY_SERIAL_NUMBER | \
					  FT_OPEN_BY_DESCRIPTION | \
					  FT_OPEN_BY_LOCATION)
/** @} */

/** @{
 * @name FT_ListDevices Flags
 * @remarks Used in conjunction with FT_OpenEx Flags
 * @see FT_ListDevices
 * @see FT_OpenEx
 */
#define FT_LIST_NUMBER_ONLY			0x80000000
#define FT_LIST_BY_INDEX			0x40000000
#define FT_LIST_ALL					0x20000000

#define FT_LIST_MASK (FT_LIST_NUMBER_ONLY|FT_LIST_BY_INDEX|FT_LIST_ALL)
/** @} */

/** @{ 
 * @name Baud Rates
 * Standard baud rates supported by many implementations and applications.
 * @see FT_SetBaudRate
 */
#define FT_BAUD_300			300
#define FT_BAUD_600			600
#define FT_BAUD_1200		1200
#define FT_BAUD_2400		2400
#define FT_BAUD_4800		4800
#define FT_BAUD_9600		9600
#define FT_BAUD_14400		14400
#define FT_BAUD_19200		19200
#define FT_BAUD_38400		38400
#define FT_BAUD_57600		57600
#define FT_BAUD_115200		115200
#define FT_BAUD_230400		230400
#define FT_BAUD_460800		460800
#define FT_BAUD_921600		921600
/** @} */

/** @{ 
 * @name Word Lengths
 * @see FT_SetDataCharacteristics
 */
#define FT_BITS_8			(UCHAR) 8
#define FT_BITS_7			(UCHAR) 7
/** @} */

/** @{ 
 * @name Stop Bits
 * @see FT_SetDataCharacteristics
 */
#define FT_STOP_BITS_1		(UCHAR) 0
#define FT_STOP_BITS_2		(UCHAR) 2
/** @} */

/* * @name Parity
 * @see FT_SetDataCharacteristics
 * @{ 
 */
#define FT_PARITY_NONE		(UCHAR) 0
#define FT_PARITY_ODD		(UCHAR) 1
#define FT_PARITY_EVEN		(UCHAR) 2
#define FT_PARITY_MARK		(UCHAR) 3
#define FT_PARITY_SPACE		(UCHAR) 4
/** @} */

/** @{ 
 * @name Flow Control
 * @see FT_SetFlowControl
 */
#define FT_FLOW_NONE		0x0000
#define FT_FLOW_RTS_CTS		0x0100
#define FT_FLOW_DTR_DSR		0x0200
#define FT_FLOW_XON_XOFF	0x0400
/** @} */

/** @{ 
 * @name Purge rx and tx buffers
 * @see FT_Purge
 */
#define FT_PURGE_RX			1
#define FT_PURGE_TX			2
/** @} */

/** @{ 
 * @name Events
 * @see FT_SetEventNotification
 */
typedef void(*PFT_EVENT_HANDLER)(DWORD, DWORD);

#define FT_EVENT_RXCHAR			1
#define FT_EVENT_MODEM_STATUS	2
#define FT_EVENT_LINE_STATUS	4
/** @} */

/** @{ 
 * @name Timeouts
 * @see FT_SetTimeouts
 */
#define FT_DEFAULT_RX_TIMEOUT	300
#define FT_DEFAULT_TX_TIMEOUT	300
/** @} */

/** @{ 
 * @name Device Types
 * @details Known supported FTDI device types supported by this library.
 */
typedef ULONG	FT_DEVICE;

enum {
	FT_DEVICE_BM,
	FT_DEVICE_AM,
	FT_DEVICE_100AX,
	FT_DEVICE_UNKNOWN,
	FT_DEVICE_2232C,
	FT_DEVICE_232R,
	FT_DEVICE_2232H,
	FT_DEVICE_4232H,
	FT_DEVICE_232H,
	FT_DEVICE_X_SERIES,
	FT_DEVICE_4222H_0,
	FT_DEVICE_4222H_1_2,
	FT_DEVICE_4222H_3,
	FT_DEVICE_4222_PROG,
	FT_DEVICE_900,
	FT_DEVICE_930,
	FT_DEVICE_UMFTPD3A,
	FT_DEVICE_2233HP,
	FT_DEVICE_4233HP,
	FT_DEVICE_2232HP,
	FT_DEVICE_4232HP,
	FT_DEVICE_233HP,
	FT_DEVICE_232HP,
	FT_DEVICE_2232HA,
	FT_DEVICE_4232HA,
	FT_DEVICE_232RN,
	FT_DEVICE_2233HPN,
	FT_DEVICE_4233HPN,
	FT_DEVICE_2232HPN,
	FT_DEVICE_4232HPN,
	FT_DEVICE_233HPN,
	FT_DEVICE_232HPN,
	FT_DEVICE_BM_A,
};
/** @} */

/** @{ 
 * @name Bit Modes
 * @see FT_SetBitMode FT_GetBitMode
 */
#define FT_BITMODE_RESET					0x00
#define FT_BITMODE_ASYNC_BITBANG			0x01
#define FT_BITMODE_MPSSE					0x02
#define FT_BITMODE_SYNC_BITBANG				0x04
#define FT_BITMODE_MCU_HOST					0x08
#define FT_BITMODE_FAST_SERIAL				0x10
#define FT_BITMODE_CBUS_BITBANG				0x20
#define FT_BITMODE_SYNC_FIFO				0x40
/** @} */

/** @{ 
 * @name FT232R CBUS Options EEPROM values
 */
#define FT_232R_CBUS_TXDEN					0x00	//	Tx Data Enable
#define FT_232R_CBUS_PWRON					0x01	//	Power On
#define FT_232R_CBUS_RXLED					0x02	//	Rx LED
#define FT_232R_CBUS_TXLED					0x03	//	Tx LED
#define FT_232R_CBUS_TXRXLED				0x04	//	Tx and Rx LED
#define FT_232R_CBUS_SLEEP					0x05	//	Sleep
#define FT_232R_CBUS_CLK48					0x06	//	48MHz clock
#define FT_232R_CBUS_CLK24					0x07	//	24MHz clock
#define FT_232R_CBUS_CLK12					0x08	//	12MHz clock
#define FT_232R_CBUS_CLK6					0x09	//	6MHz clock
#define FT_232R_CBUS_IOMODE					0x0A	//	IO Mode for CBUS bit-bang
#define FT_232R_CBUS_BITBANG_WR				0x0B	//	Bit-bang write strobe
#define FT_232R_CBUS_BITBANG_RD				0x0C	//	Bit-bang read strobe
#define FT_232R_CBUS0_RXF					0x0D	//	CBUS0 RXF#
#define FT_232R_CBUS1_TXE					0x0D	//	CBUS1 TXE#
#define FT_232R_CBUS2_RD					0x0D	//	CBUS2 RD#
#define FT_232R_CBUS3_WR					0x0D	//	CBUS3 WR#
/** @} */

/** @{ 
 * @name FT232H CBUS Options EEPROM values
 */
#define FT_232H_CBUS_TRISTATE				0x00	//	Tristate
#define FT_232H_CBUS_TXLED					0x01	//	Tx LED
#define FT_232H_CBUS_RXLED					0x02	//	Rx LED
#define FT_232H_CBUS_TXRXLED				0x03	//	Tx and Rx LED
#define FT_232H_CBUS_PWREN					0x04	//	Power Enable
#define FT_232H_CBUS_SLEEP					0x05	//	Sleep
#define FT_232H_CBUS_DRIVE_0				0x06	//	Drive pin to logic 0
#define FT_232H_CBUS_DRIVE_1				0x07	//	Drive pin to logic 1
#define FT_232H_CBUS_IOMODE					0x08	//	IO Mode for CBUS bit-bang
#define FT_232H_CBUS_TXDEN					0x09	//	Tx Data Enable
#define FT_232H_CBUS_CLK30					0x0A	//	30MHz clock
#define FT_232H_CBUS_CLK15					0x0B	//	15MHz clock
#define FT_232H_CBUS_CLK7_5					0x0C	//	7.5MHz clock
/** @} */

/** @{ 
 * @name FT X Series CBUS Options EEPROM values
 */
#define FT_X_SERIES_CBUS_TRISTATE			0x00	//	Tristate
#define FT_X_SERIES_CBUS_TXLED				0x01	//	Tx LED
#define FT_X_SERIES_CBUS_RXLED				0x02	//	Rx LED
#define FT_X_SERIES_CBUS_TXRXLED			0x03	//	Tx and Rx LED
#define FT_X_SERIES_CBUS_PWREN				0x04	//	Power Enable
#define FT_X_SERIES_CBUS_SLEEP				0x05	//	Sleep
#define FT_X_SERIES_CBUS_DRIVE_0			0x06	//	Drive pin to logic 0
#define FT_X_SERIES_CBUS_DRIVE_1			0x07	//	Drive pin to logic 1
#define FT_X_SERIES_CBUS_IOMODE				0x08	//	IO Mode for CBUS bit-bang
#define FT_X_SERIES_CBUS_TXDEN				0x09	//	Tx Data Enable
#define FT_X_SERIES_CBUS_CLK24				0x0A	//	24MHz clock
#define FT_X_SERIES_CBUS_CLK12				0x0B	//	12MHz clock
#define FT_X_SERIES_CBUS_CLK6				0x0C	//	6MHz clock
#define FT_X_SERIES_CBUS_BCD_CHARGER		0x0D	//	Battery charger detected
#define FT_X_SERIES_CBUS_BCD_CHARGER_N		0x0E	//	Battery charger detected inverted
#define FT_X_SERIES_CBUS_I2C_TXE			0x0F	//	I2C Tx empty
#define FT_X_SERIES_CBUS_I2C_RXF			0x10	//	I2C Rx full
#define FT_X_SERIES_CBUS_VBUS_SENSE			0x11	//	Detect VBUS
#define FT_X_SERIES_CBUS_BITBANG_WR			0x12	//	Bit-bang write strobe
#define FT_X_SERIES_CBUS_BITBANG_RD			0x13	//	Bit-bang read strobe
#define FT_X_SERIES_CBUS_TIMESTAMP			0x14	//	Toggle output when a USB SOF token is received
#define FT_X_SERIES_CBUS_KEEP_AWAKE			0x15	//	
/** @} */

/** @{ 
 * @name Driver Types
 */
#define FT_DRIVER_TYPE_D2XX		0
#define FT_DRIVER_TYPE_VCP		1
/** @} */

/** @{
 * @name FT_DEVICE_LIST_INFO_NODE Device Information Flags
 * @par Summary
 * These flags are used in the Flags member of FT_DEVICE_LIST_INFO_NODE to indicated the state of 
 * the device and speed of the device.
*/
enum {
	FT_FLAGS_OPENED = 1,
	FT_FLAGS_HISPEED = 2
};
/** @} */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef FTD2XX_STATIC
	FTD2XX_API FT_STATUS WINAPI FT_Initialise(
		void
		);

	FTD2XX_API void WINAPI FT_Finalise(
		void
		);
#endif // FTD2XX_STATIC

/**	 * @name D2XX Classic Functions 
 	The functions listed in this section are compatible with all FTDI devices.
 */
/** @{ */
#ifndef _WIN32

	/** @noop FT_SetVIDPID
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * @par Summary
	 * A command to include a custom VID and PID combination within the internal device list table. 
	 * This will allow the driver to load for the specified VID and PID combination.
	 * @param dwVID Device Vendor ID (VID)
	 * @param dwPID Device Product ID (PID)
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * By default, the driver will support a limited set of VID and PID matched devices (VID 0x0403 
	 * with PIDs for standard FTDI devices only).
	 * @n In order to use the driver with other VID and PID combinations the FT_SetVIDPID function 
	 * must be used prior to calling FT_ListDevices, FT_Open, FT_OpenEx or FT_CreateDeviceInfoList.
	 * @note Extra function for non-Windows platforms to compensate for lack of .INF file to specify 
	 * Vendor and Product IDs.
	 */
	FTD2XX_API FT_STATUS FT_SetVIDPID(
		DWORD dwVID,
		DWORD dwPID
		);

	/** @noop FT_GetVIDPID
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * @par Summary
	 * A command to retrieve the current VID and PID combination from within the internal device list table.
	 * @param pdwVID Pointer to DWORD that will contain the internal VID
	 * @param pdwPID Pointer to DWORD that will contain the internal PID
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * @note Extra function for non-Windows platforms to compensate for lack of .INF file to specify Vendor and Product IDs.
	 * @see FT_SetVIDPID.
	 */
	FTD2XX_API FT_STATUS FT_GetVIDPID(
		DWORD * pdwVID,
		DWORD * pdwPID
		);

#endif // _WIN32        

	/** @noop FT_CreateDeviceInfoList
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later) 
	 * @par Summary
	 * This function builds a device information list and returns the number of D2XX devices connected to the
	 * system. The list contains information about both unopen and open devices.
	 * @param lpdwNumDevs Pointer to unsigned long to store the number of devices connected.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * An application can use this function to get the number of devices attached to the system. It can then
	 * allocate space for the device information list and retrieve the list using FT_GetDeviceInfoList or
	 * FT_GetDeviceInfoDetail.
	 * @n If the devices connected to the system change, the device info list will not be updated until
	 * FT_CreateDeviceInfoList is called again. 
	 * @see FT_GetDeviceInfoList
	 * @see FT_GetDeviceInfoDetail
	 */
	FTD2XX_API FT_STATUS WINAPI FT_CreateDeviceInfoList(
		LPDWORD lpdwNumDevs
		);

	/**  @noop FT_DEVICE_LIST_INFO_NODE
	 * @par Summary
	 * This structure is used for passing information about a device back from the FT_GetDeviceInfoList function.
	 * @see FT_GetDeviceInfoList
	 */
	typedef struct _ft_device_list_info_node {
		ULONG Flags;
		ULONG Type;
		ULONG ID;
		DWORD LocId;
		char SerialNumber[16];
		char Description[64];
		FT_HANDLE ftHandle;
	} FT_DEVICE_LIST_INFO_NODE;

	/** @noop FT_GetDeviceInfoList
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later) 
	 * @par Summary
	 * This function returns a device information list and the number of D2XX devices in the list.
	 * @param *pDest Pointer to an array of FT_DEVICE_LIST_INFO_NODE structures.
	 * @param lpdwNumDevs Pointer to the number of elements in the array.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * This function should only be called after calling FT_CreateDeviceInfoList. If the devices connected to the
	 * system change, the device info list will not be updated until FT_CreateDeviceInfoList is called again.
	 * Location ID information is not returned for devices that are open when FT_CreateDeviceInfoList is called.
	 * Information is not available for devices which are open in other processes. In this case, the Flags
	 * parameter of the FT_DEVICE_LIST_INFO_NODE will indicate that the device is open, but other fields will
	 * be unpopulated.
	 * @n The flag value is a 4-byte bit map containing miscellaneous data as defined Appendix A - Type
	 * Definitions. Bit 0 (least significant bit) of this number indicates if the port is open (1) or closed (0). Bit 1
	 * indicates if the device is enumerated as a high-speed USB device (2) or a full-speed USB device (0). The
	 * remaining bits (2 - 31) are reserved.
	 * @n The array of FT_DEVICE_LIST_INFO_NODES contains all available data on each device. The structure of
	 * FT_DEVICE_LIST_INFO_NODES is given in the Appendix. The storage for the list must be allocated by
	 * the application. The number of devices returned by FT_CreateDeviceInfoList can be used to do this.
	 * When programming in Visual Basic, LabVIEW or similar languages, FT_GetDeviceInfoDetail may be
	 * required instead of this function.
	 * @note Please note that Windows CE does not support location IDs. As such, the Location ID parameter in the 
	 * structure will be empty.
	 * @see FT_CreateDeviceInfoList
	 */
	FTD2XX_API FT_STATUS WINAPI FT_GetDeviceInfoList(
		FT_DEVICE_LIST_INFO_NODE *pDest,
		LPDWORD lpdwNumDevs
		);

	/** @noop FT_GetDeviceInfoDetail
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later) 
	 * @par Summary
	 * This function returns an entry from the device information list.
	 * @param dwIndex Index of the entry in the device info list.
	 * @param lpdwFlags Pointer to unsigned long to store the flag value.
	 * @param lpdwType Pointer to unsigned long to store device type.
	 * @param lpdwID Pointer to unsigned long to store device ID.
	 * @param lpdwLocId Pointer to unsigned long to store the device location ID.
	 * @param lpSerialNumber Pointer to buffer to store device serial number as a nullterminated string.
	 * @param lpDescription Pointer to buffer to store device description as a null-terminated string.
	 * @param *pftHandle Pointer to a variable of type FT_HANDLE where the handle will be stored.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * This function should only be called after calling FT_CreateDeviceInfoList. If the devices connected to the
	 * system change, the device info list will not be updated until FT_CreateDeviceInfoList is called again.
	 * @n The index value is zero-based.
	 * @n The flag value is a 4-byte bit map containing miscellaneous data as defined Appendix A - Type
	 * Definitions. Bit 0 (least significant bit) of this number indicates if the port is open (1) or closed (0). Bit 1
	 * indicates if the device is enumerated as a high-speed USB device (2) or a full-speed USB device (0). The
	 * remaining bits (2 - 31) are reserved.
	 * @n Location ID information is not returned for devices that are open when FT_CreateDeviceInfoList is called.
	 * Information is not available for devices which are open in other processes. In this case, the lpdwFlags
	 * parameter will indicate that the device is open, but other fields will be unpopulated.
	 * To return the whole device info list as an array of FT_DEVICE_LIST_INFO_NODE structures, use
	 * FT_CreateDeviceInfoList.
	 * @note Please note that Windows CE does not support location IDs. As such, the Location ID parameter in the 
	 * structure will be empty.
	 * @see FT_CreateDeviceInfoList
	 */
	FTD2XX_API FT_STATUS WINAPI FT_GetDeviceInfoDetail(
		DWORD dwIndex,
		LPDWORD lpdwFlags,
		LPDWORD lpdwType,
		LPDWORD lpdwID,
		LPDWORD lpdwLocId,
		LPVOID lpSerialNumber,
		LPVOID lpDescription,
		FT_HANDLE *pftHandle
		);

	/** @noop FT_ListDevices
	 * @par Summary
	 * Gets information concerning the devices currently connected. This function can return information such
	 * as the number of devices connected, the device serial number and device description strings, and the
	 * location IDs of connected devices.
	 * @param pvArg1 Meaning depends on dwFlags.
	 * @param pvArg2 Meaning depends on dwFlags.
	 * @param dwFlags Determines format of returned information.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * This function can be used in a number of ways to return different types of information. A more powerful
	 * way to get device information is to use the FT_CreateDeviceInfoList, FT_GetDeviceInfoList and
	 * FT_GetDeviceInfoDetail functions as they return all the available information on devices.
	 * In its simplest form, it can be used to return the number of devices currently connected. If
	 * FT_LIST_NUMBER_ONLY bit is set in dwFlags, the parameter pvArg1 is interpreted as a pointer to a
	 * DWORD location to store the number of devices currently connected.
	 * @n It can be used to return device information: if FT_OPEN_BY_SERIAL_NUMBER bit is set in dwFlags, the
	 * serial number string will be returned; if FT_OPEN_BY_DESCRIPTION bit is set in dwFlags, the product 
	 * description string will be returned; if FT_OPEN_BY_LOCATION bit is set in dwFlags, the Location ID will 
	 * be returned; if none of these bits is set, the serial number string will be returned by default.
	 * @n It can be used to return device string information for a single device. If FT_LIST_BY_INDEX and
	 * FT_OPEN_BY_SERIAL_NUMBER or FT_OPEN_BY_DESCRIPTION bits are set in dwFlags, the parameter
	 * pvArg1 is interpreted as the index of the device, and the parameter pvArg2 is interpreted as a pointer to
	 * a buffer to contain the appropriate string. Indexes are zero-based, and the error code
	 * FT_DEVICE_NOT_FOUND is returned for an invalid index.
	 * @n It can be used to return device string information for all connected devices. If FT_LIST_ALL and
	 * FT_OPEN_BY_SERIAL_NUMBER or FT_OPEN_BY_DESCRIPTION bits are set in dwFlags, the parameter
	 * pvArg1 is interpreted as a pointer to an array of pointers to buffers to contain the appropriate strings and
	 * the parameter pvArg2 is interpreted as a pointer to a DWORD location to store the number of devices
	 * currently connected. Note that, for pvArg1, the last entry in the array of pointers to buffers should be a
	 * NULL pointer so the array will contain one more location than the number of devices connected.
	 * @n The location ID of a device is returned if FT_LIST_BY_INDEX and FT_OPEN_BY_LOCATION bits are set in
	 * dwFlags. In this case the parameter pvArg1 is interpreted as the index of the device, and the parameter
	 * pvArg2 is interpreted as a pointer to a variable of type long to contain the location ID. Indexes are 
	 * zerobased, and the error code FT_DEVICE_NOT_FOUND is returned for an invalid index. Please note that
	 * Windows CE and Linux do not support location IDs.
	 * @n The location IDs of all connected devices are returned if FT_LIST_ALL and FT_OPEN_BY_LOCATION bits
	 * are set in dwFlags. In this case, the parameter pvArg1 is interpreted as a pointer to an array of variables
	 * of type long to contain the location IDs, and the parameter pvArg2 is interpreted as a pointer to a
	 * DWORD location to store the number of devices currently connected.
	 * @see FT_CreateDeviceInfoList
	 * @see FT_GetDeviceInfoList
	 * @see FT_GetDeviceInfoDetail
	 */
	FTD2XX_API FT_STATUS WINAPI FT_ListDevices(
		PVOID pvArg1,
		PVOID pvArg2,
		DWORD dwFlags
		);

	/** @noop FT_Open
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Open the device and return a handle which will be used for subsequent accesses.
	 * @param deviceNumber Index of the device to open. Indices are 0 based.
	 * @param pHandle Pointer to a variable of type FT_HANDLE where the handle will be stored. This handle must 
	 * be used to access the device.
	 * @return
	 * FT_OK if successful, otherwise the return value is an FT error code. 
	 * @remarks
	 * Although this function can be used to open multiple devices by setting iDevice to 0, 1, 2 etc. there is no
	 * ability to open a specific device. To open named devices, use the function FT_OpenEx.
	 * @see FT_OpenEx.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_Open(
		int deviceNumber,
		FT_HANDLE *pHandle
		);

	/** @noop FT_OpenEx
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Open the specified device and return a handle that will be used for subsequent accesses. The device can
	 * be specified by its serial number, device description or location.
	 * @n This function can also be used to open multiple devices simultaneously. Multiple devices can be specified
	 * by serial number, device description or location ID (location information derived from the physical
	 * location of a device on USB). Location IDs for specific USB ports can be obtained using the utility
	 * USBView and are given in hexadecimal format. Location IDs for devices connected to a system can be
	 * obtained by calling FT_GetDeviceInfoList or FT_ListDevices with the appropriate flags.
	 * @param pvArg1 Pointer to an argument whose type depends on the value of dwFlags. 
	 * It is normally be interpreted as a pointer to a null terminated string.
	 * @param dwFlags FT_OPEN_BY_SERIAL_NUMBER, FT_OPEN_BY_DESCRIPTION or FT_OPEN_BY_LOCATION.
	 * @param pHandle Pointer to a variable of type FT_HANDLE where the handle will be
	 * stored. This handle must be used to access the device.
	 * @returns 
	 * FT_OK if successful, otherwise the return value is an FT error code. 
	 * @remarks 
	 * The parameter specified in pvArg1 depends on dwFlags: if dwFlags is FT_OPEN_BY_SERIAL_NUMBER,
	 * pvArg1 is interpreted as a pointer to a null-terminated string that represents the serial number of the
	 * device; if dwFlags is FT_OPEN_BY_DESCRIPTION, pvArg1 is interpreted as a pointer to a nullterminated 
	 * string that represents the device description; if dwFlags is FT_OPEN_BY_LOCATION, pvArg1
	 * is interpreted as a long value that contains the location ID of the device. Please note that Windows CE
	 * and Linux do not support location IDs.
	 * @n ftHandle is a pointer to a variable of type FT_HANDLE where the handle is to be stored. This handle must
	 * be used to access the device.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_OpenEx(
		PVOID pvArg1,
		DWORD dwFlags,
		FT_HANDLE *pHandle
		);

	/** @noop FT_Close
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Close an open device.
	 * @param ftHandle Handle of the device.
	 * @returns 
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_Close(
		FT_HANDLE ftHandle
		);

	/** @noop FT_Read
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Read data from the device.
	 * @param ftHandle Handle of the device.
	 * @param lpBuffer Pointer to the buffer that receives the data from the device.
	 * @param dwBytesToRead Number of bytes to be read from the device.
	 * @param lpdwBytesReturned Pointer to a variable of type DWORD which receives the number of
	 * bytes read from the device.
	 * @returns
	 * FT_OK if successful, FT_IO_ERROR otherwise. $see FT_STATUS
	 * @remarks
	 * FT_Read always returns the number of bytes read in lpdwBytesReturned.
	 * @n This function does not return until dwBytesToRead bytes have been read into the buffer. The number of
	 * bytes in the receive queue can be determined by calling FT_GetStatus or FT_GetQueueStatus, and
	 * passed to FT_Read as dwBytesToRead so that the function reads the device and returns immediately.
	 * When a read timeout value has been specified in a previous call to FT_SetTimeouts, FT_Read returns
	 * when the timer expires or dwBytesToRead have been read, whichever occurs first. If the timeout
	 * occurred, FT_Read reads available data into the buffer and returns FT_OK.
	 * @n An application should use the function return value and lpdwBytesReturned when processing the buffer.
	 * If the return value is FT_OK, and lpdwBytesReturned is equal to dwBytesToRead then FT_Read has
	 * completed normally. If the return value is FT_OK, and lpdwBytesReturned is less then dwBytesToRead
	 * then a timeout has occurred and the read has been partially completed. Note that if a timeout occurred
	 * and no data was read, the return value is still FT_OK.
	 * @n A return value of FT_IO_ERROR suggests an error in the parameters of the function, or a fatal error like a
	 * USB disconnect has occurred.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_Read(
		FT_HANDLE ftHandle,
		LPVOID lpBuffer,
		DWORD dwBytesToRead,
		LPDWORD lpdwBytesReturned
		);

	/** @noop FT_Write
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Write data to the device.
	 * @param ftHandle Handle of the device.
	 * @param lpBuffer Pointer to the buffer that contains the data to be written to the device.
	 * @param dwBytesToWrite Number of bytes to write to the device.
	 * @param lpdwBytesWritten Pointer to a variable of type DWORD which receives the number of
	 * bytes written to the device.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_Write(
		FT_HANDLE ftHandle,
		LPVOID lpBuffer,
		DWORD dwBytesToWrite,
		LPDWORD lpdwBytesWritten
		);

	/** @noop FT_SetBaudRate
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * This function sets the baud rate for the device.
	 * @param ftHandle Handle of the device.
	 * @param dwBaudRate Baud rate.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code
	 */ 
	FTD2XX_API FT_STATUS WINAPI FT_SetBaudRate(
		FT_HANDLE ftHandle,
		ULONG dwBaudRate
		);

	/** @noop FT_SetDivisor
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * This function sets the baud rate for the device. It is used to set non-standard baud rates.
	 * @param ftHandle Handle of the device.
	 * @param usDivisor Divisor.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * This function is no longer required as FT_SetBaudRate will now automatically calculate the required
	 * divisor for a requested baud rate. The application note "Setting baud rates for the FT8U232AM" is
	 * available from the Application Notes section of the FTDI website describes how to calculate the divisor for
	 * a non-standard baud rate.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_SetDivisor(
		FT_HANDLE ftHandle,
		USHORT usDivisor
		);

	/** @noop FT_SetDataCharacteristics
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * This function sets the data characteristics for the device.
	 * @param ftHandle Handle of the device.
	 * @param uWordLength Number of bits per word - must be FT_BITS_8 or FT_BITS_7.
	 * @param uStopBits Number of stop bits - must be FT_STOP_BITS_1 or FT_STOP_BITS_2.
	 * @param uParity Parity - must be FT_PARITY_NONE, FT_PARITY_ODD, FT_PARITY_EVEN, 
	 * FT_PARITY_MARK or FT_PARITY SPACE.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_SetDataCharacteristics(
		FT_HANDLE ftHandle,
		UCHAR uWordLength,
		UCHAR uStopBits,
		UCHAR uParity
		);

	/** @noop FT_SetTimeouts
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * This function sets the read and write timeouts for the device.
	 * @param ftHandle Handle of the device.
	 * @param dwReadTimeout Read timeout in milliseconds.
	 * @param dwWriteTimeout Write timeout in milliseconds.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_SetTimeouts(
		FT_HANDLE ftHandle,
		ULONG dwReadTimeout,
		ULONG dwWriteTimeout
		);

	/** @noop FT_SetFlowControl
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * This function sets the flow control for the device.
	 * @param ftHandle Handle of the device.
	 * @param usFlowControl Must be one of FT_FLOW_NONE, FT_FLOW_RTS_CTS, FT_FLOW_DTR_DSR or 
	 * FT_FLOW_XON_XOFF.
	 * @param uXonChar Character used to signal Xon. Only used if flow control is FT_FLOW_XON_XOFF.
	 * @param uXoffChar Character used to signal Xoff. Only used if flow control is	FT_FLOW_XON_XOFF.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_SetFlowControl(
		FT_HANDLE ftHandle,
		USHORT usFlowControl,
		UCHAR uXonChar,
		UCHAR uXoffChar
		);

	/** @noop FT_SetDtr
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * This function sets the Data Terminal Ready (DTR) control signal.
	 * @param ftHandle Handle of the device.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * This function asserts the Data Terminal Ready (DTR) line of the device.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_SetDtr(
		FT_HANDLE ftHandle
		);

	/** @noop FT_ClrDtr
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * This function clears the Data Terminal Ready (DTR) control signal.
	 * @param ftHandle Handle of the device.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * This function de-asserts the Data Terminal Ready (DTR) line of the device.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_ClrDtr(
		FT_HANDLE ftHandle
		);

	/** @noop FT_SetRts
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * This function sets the Request To Send (RTS) control signal.
	 * @param ftHandle Handle of the device.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * This function asserts the Request To Send (RTS) line of the device.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_SetRts(
		FT_HANDLE ftHandle
		);

	/** @noop FT_ClrRts
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * This function clears the Request To Send (RTS) control signal.
	 * @param ftHandle Handle of the device.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * This function de-asserts the Request To Send (RTS) line of the device.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_ClrRts(
		FT_HANDLE ftHandle
		);

	/** @noop FT_GetModemStatus
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Gets the modem status and line status from the device.
	 * @param ftHandle Handle of the device.
	 * @param lpdwModemStatus Pointer to a variable of type DWORD which receives the modem
	 * status and line status from the device.
	 * @returns 
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * The least significant byte of the lpdwModemStatus value holds the modem status. On Windows and
	 * Windows CE, the line status is held in the second least significant byte of the lpdwModemStatus value.
	 * @n The modem status is bit-mapped as follows: Clear To Send (CTS) = 0x10, Data Set Ready (DSR) = 0x20,
	 * Ring Indicator (RI) = 0x40, Data Carrier Detect (DCD) = 0x80.
	 * @n The line status is bit-mapped as follows: Overrun Error (OE) = 0x02, Parity Error (PE) = 0x04, Framing
	 * Error (FE) = 0x08, Break Interrupt (BI) = 0x10.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_GetModemStatus(
		FT_HANDLE ftHandle,
		ULONG *lpdwModemStatus
		);

	/** @noop FT_GetQueueStatus
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Gets the number of bytes in the receive queue.
	 * @param ftHandle Handle of the device.
	 * @param lpdwAmountInRxQueue Pointer to a variable of type DWORD which receives the number of
	 * bytes in the receive queue.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_GetQueueStatus(
		FT_HANDLE ftHandle,
		DWORD *lpdwAmountInRxQueue
		);

	/** @noop FT_GetDeviceInfo
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Get device information for an open device.
	 * @param ftHandle Handle of the device.
	 * @param lpftDevice Pointer to unsigned long to store device type.
	 * @param lpdwID Pointer to unsigned long to store device ID.
	 * @param pcSerialNumber Pointer to buffer to store device serial number as a nullterminated string.
	 * @param pcDescription Pointer to buffer to store device description as a null-terminated string.
	 * @param pvDummy Reserved for future use - should be set to NULL.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * This function is used to return the device type, device ID, device description and serial number.
	 * The device ID is encoded in a DWORD - the most significant word contains the vendor ID, and the least
	 * significant word contains the product ID. So the returned ID 0x04036001 corresponds to the device ID
	 * VID_0403&PID_6001.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_GetDeviceInfo(
		FT_HANDLE ftHandle,
		FT_DEVICE *lpftDevice,
		LPDWORD lpdwID,
		PCHAR pcSerialNumber,
		PCHAR pcDescription,
		LPVOID pvDummy
		);

#ifndef _WIN32
	/**
	 * @note Extra function for non-Windows platforms to compensate for lack of .INF file to specify Vendor and Product IDs.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_GetDeviceLocId(
		FT_HANDLE ftHandle,
		LPDWORD lpdwLocId
		);
#endif // _WIN32        

	/** @noop FT_GetDriverVersion
	 * @par Supported Operating Systems
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * This function returns the D2XX driver version number.
	 * @param ftHandle Handle of the device.
	 * @param lpdwDriverVersion Pointer to the driver version number.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * A version number consists of major, minor and build version numbers contained in a 4-byte field
	 * (unsigned long). Byte0 (least significant) holds the build version, Byte1 holds the minor version, and
	 * Byte2 holds the major version. Byte3 is currently set to zero.
	 * @n For example, driver version "2.04.06" is represented as 0x00020406. Note that a device has to be
	 * opened before this function can be called.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_GetDriverVersion(
		FT_HANDLE ftHandle,
		LPDWORD lpdwDriverVersion
		);

	/** @noop FT_GetLibraryVersion
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @n
	 * This function returns D2XX DLL or library version number.
	 * @param lpdwDLLVersion Pointer to the DLL or library version number.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * A version number consists of major, minor and build version numbers contained in a 4-byte field
	 * (unsigned long). Byte0 (least significant) holds the build version, Byte1 holds the minor version, and
	 * Byte2 holds the major version. Byte3 is currently set to zero.
	 * @n For example, D2XX DLL version "3.01.15" is represented as 0x00030115. Note that this function does
	 * not take a handle, and so it can be called without opening a device.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_GetLibraryVersion(
		LPDWORD lpdwDLLVersion
		);

	/** @noop FT_GetComPortNumber
	 * @par Supported Operating Systems
	 * Windows (2000 and later)
	 * @par Summary
	 * Retrieves the COM port associated with a device.
	 * @param ftHandle Handle of the device.
	 * @param lplComPortNumber Pointer to a variable of type LONG which receives the COM port number
	 * associated with the device.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * This function is only available when using the Windows CDM driver as both the D2XX and VCP drivers can
	 * be installed at the same time.
	 * @n If no COM port is associated with the device, lplComPortNumber will have a value of -1
	 */
	FTD2XX_API FT_STATUS WINAPI FT_GetComPortNumber(
		FT_HANDLE ftHandle,
		LPLONG	lplComPortNumber
		);

	/** @noop FT_GetStatus
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Gets the device status including number of characters in the receive queue, number of characters in the
	 * transmit queue, and the current event status.
	 * @param ftHandle Handle of the device.
	 * @param lpdwAmountInRxQueue Pointer to a variable of type DWORD which receives the number of characters in
	 * the receive queue.
	 * @param lpdwAmountInTxQueue Pointer to a variable of type DWORD which receives the number of characters in
	 * the transmit queue.
	 * @param lpdwEventStatus Pointer to a variable of type DWORD which receives the current state of
	 * the event status.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * For an example of how to use this function, see the sample code in FT_SetEventNotification.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_GetStatus(
		FT_HANDLE ftHandle,
		DWORD *lpdwAmountInRxQueue,
		DWORD *lpdwAmountInTxQueue,
		DWORD *lpdwEventStatus
		);

	/** @noop FT_SetEventNotification
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Sets conditions for event notification.
	 * @param ftHandle Handle of the device.
	 * @param dwEventMask Conditions that cause the event to be set.
	 * @param pvArg Interpreted as the handle of an event.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * An application can use this function to setup conditions which allow a thread to block until one of the
	 * conditions is met. Typically, an application will create an event, call this function, then block on the
	 * event. When the conditions are met, the event is set, and the application thread unblocked.
	 * dwEventMask is a bit-map that describes the events the application is interested in. pvArg is interpreted
	 * as the handle of an event which has been created by the application. If one of the event conditions is
	 * met, the event is set.
	 * @n If FT_EVENT_RXCHAR is set in dwEventMask, the event will be set when a character has been received
	 * by the device.
	 * @n If FT_EVENT_MODEM_STATUS is set in dwEventMask, the event will be set when a change in the modem
	 * signals has been detected by the device.
	 * @n If FT_EVENT_LINE_STATUS is set in dwEventMask, the event will be set when a change in the line status
	 * has been detected by the device.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_SetEventNotification(
		FT_HANDLE ftHandle,
		DWORD dwEventMask,
		PVOID pvArg
		);

	/** @noop FT_SetChars
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * This function sets the special characters for the device.
	 * @param ftHandle Handle of the device.
	 * @param uEventChar Event character.
	 * @param uEventCharEnabled 0 if event character disabled, non-zero otherwise.
	 * @param uErrorChar Error character.
	 * @param uErrorCharEnabled 0 if error character disabled, non-zero otherwise.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * This function allows for inserting specified characters in the data stream to represent events firing or
	 * errors occurring.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_SetChars(
		FT_HANDLE ftHandle,
		UCHAR uEventChar,
		UCHAR uEventCharEnabled,
		UCHAR uErrorChar,
		UCHAR uErrorCharEnabled
		);

	/** @noop FT_SetBreakOn
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Sets the BREAK condition for the device.
	 * @param ftHandle Handle of the device.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code	
	 */
	FTD2XX_API FT_STATUS WINAPI FT_SetBreakOn(
		FT_HANDLE ftHandle
		);

	/** @noop FT_SetBreakOff
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Resets the BREAK condition for the device.
	 * @param ftHandle Handle of the device.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_SetBreakOff(
		FT_HANDLE ftHandle
		);

	/** @noop FT_Purge
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * This function purges receive and transmit buffers in the device.
	 * @param ftHandle Handle of the device.
	 * @param ulMask Combination of FT_PURGE_RX and FT_PURGE_TX.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_Purge(
		FT_HANDLE ftHandle,
		ULONG ulMask
		);

	/** @noop FT_ResetDevice
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later) 
	 * @par Summary
	 * This function sends a reset command to the device.
	 * @param ftHandle Handle of the device.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_ResetDevice(
		FT_HANDLE ftHandle
		);

	/** @noop FT_ResetPort
	 * @par Supported Operating Systems
	 * Windows (2000 and later)
	 * @par Summary
	 * Send a reset command to the port.
	 * @param ftHandle Handle of the device.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * This function is used to attempt to recover the port after a failure. It is not equivalent 
	 * to an unplug-replug event. For the equivalent of an unplug-replug event, use FT_CyclePort.
	 * @see FT_CyclePort
	 */
	FTD2XX_API FT_STATUS WINAPI FT_ResetPort(
		FT_HANDLE ftHandle
		);

	/** @noop FT_CyclePort
	 * @par Supported Operating Systems
	 * Windows (2000 and later)
	 * @par Summary
	 * Send a cycle command to the USB port.
	 * @param ftHandle Handle of the device.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * The effect of this function is the same as disconnecting then reconnecting the device from 
	 * USB. Possible use of this function is situations where a fatal error has occurred and it is 
	 * difficult, or not possible, to recover without unplugging and replugging the USB cable. 
	 * This function can also be used after reprogramming the EEPROM to force the FTDI device to 
	 * read the new EEPROM contents which would	otherwise require a physical disconnect-reconnect.
	 * @n As the current session is not restored when the driver is reloaded, the application must 
	 * be able to recover after calling this function. It is ithe responisbility of the application 
	 * to close the handle after successfully calling FT_CyclePort.
	 * @n For FT4232H, FT2232H and FT2232 devices, FT_CyclePort will only work under Windows XP and later.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_CyclePort(
		FT_HANDLE ftHandle
		);

	/** @noop FT_Rescan
	 * @par Supported Operating Systems
	 * Windows (2000 and later)
	 * @par Summary
	 * This function can be of use when trying to recover devices programatically.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * Calling FT_Rescan is equivalent to clicking the "Scan for hardware changes" button in the Device
	 * Manager. Only USB hardware is checked for new devices. All USB devices are scanned, not just FTDI
	 * devices.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_Rescan(
		void
		);

	/** @noop FT_Reload
	 * @par Supported Operating Systems
	 * Windows (2000 and later)
	 * @par Summary
	 * This function forces a reload of the driver for devices with a specific VID and PID combination.
	 * @param wVID Vendor ID of the devices to reload the driver for.
	 * @param wPID Product ID of the devices to reload the driver for.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * Calling FT_Reload forces the operating system to unload and reload the driver for the specified device
	 * IDs. If the VID and PID parameters are null, the drivers for USB root hubs will be reloaded, causing all
	 * USB devices connected to reload their drivers. Please note that this function will not work correctly on
	 * 64-bit Windows when called from a 32-bit application.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_Reload(
		WORD wVID,
		WORD wPID
		);

	/** @noop FT_SetResetPipeRetryCount
	 * @par Supported Operating Systems
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Set the ResetPipeRetryCount value.
	 * @param ftHandle Handle of the device.
	 * @param dwCount Unsigned long containing required ResetPipeRetryCount.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * This function is used to set the ResetPipeRetryCount. ResetPipeRetryCount controls the maximum
	 * number of times that the driver tries to reset a pipe on which an error has occurred.
	 * ResetPipeRequestRetryCount defaults to 50. It may be necessary to increase this value in noisy
	 * environments where a lot of USB errors occur.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_SetResetPipeRetryCount(
		FT_HANDLE ftHandle,
		DWORD dwCount
		);

	/** @noop FT_StopInTask
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Stops the driver's IN task.
	 * @param ftHandle Handle of the device.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * This function is used to put the driver's IN task (read) into a wait state. It can be used in situations
	 * where data is being received continuously, so that the device can be purged without more data being
	 * received. It is used together with FT_RestartInTask which sets the IN task running again.
	 * @see FT_RestartInTask
	 */
	FTD2XX_API FT_STATUS WINAPI FT_StopInTask(
		FT_HANDLE ftHandle
		);

	/** @noop FT_RestartInTask
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Restart the driver's IN task.
	 * @param ftHandle Handle of the device.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * This function is used to restart the driver's IN task (read) after it has been stopped by a call to
	 * FT_StopInTask.
	 * @see FT_StopInTask
	 */
	FTD2XX_API FT_STATUS WINAPI FT_RestartInTask(
		FT_HANDLE ftHandle
		);

	/** @noop FT_SetDeadmanTimeout
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * This function allows the maximum time in milliseconds that a USB request can remain outstanding to 
	 * be set.
	 * @param ftHandle Handle of the device.
	 * @param ulDeadmanTimeout Deadman timeout value in milliseconds. Default value is 5000.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * The deadman timeout is referred to in application note AN232B-10 Advanced Driver Options from the
	 * FTDI web site as the USB timeout. It is unlikely that this function will be required by most users.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_SetDeadmanTimeout(
		FT_HANDLE ftHandle,
		ULONG ulDeadmanTimeout
		);

	/** @noop FT_IoCtl
	 * Undocumented function.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_IoCtl(
		FT_HANDLE ftHandle,
		DWORD dwIoControlCode,
		LPVOID lpInBuf,
		DWORD nInBufSize,
		LPVOID lpOutBuf,
		DWORD nOutBufSize,
		LPDWORD lpBytesReturned,
		LPOVERLAPPED lpOverlapped
		);

	/** @noop FT_SetWaitMask
	 * Undocumented function.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_SetWaitMask(
		FT_HANDLE ftHandle,
		DWORD Mask
		);

	/** @noop FT_WaitOnMask
	 * Undocumented function.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_WaitOnMask(
		FT_HANDLE ftHandle,
		DWORD *Mask
		);

	/** @noop FT_GetEventStatus
	 * Undocumented function.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_GetEventStatus(
		FT_HANDLE ftHandle,
		DWORD *dwEventDWord
		);

/** @} */

/** @name EEPROM Programming Interface Functions
 * FTDI device EEPROMs can be both read and programmed using the functions listed in this section.
 * @n Please note the following information:
 * @li The Maximum length of the Manufacturer, ManufacturerId, Description and SerialNumber strings
 * is 48 words (1 word = 2 bytes).
 * @li The first two characters of the serial number are the manufacturer ID.
 * @li The Manufacturer string length plus the Description string length is less than or equal to 40
 * characters with the following functions: FT_EE_Read, FT_EE_Program, FT_EE_ProgramEx, 
 * FT_EEPROM_Read, FT_EEPROM_Program.
 * @li The serial number should be maximum 15 characters long on single port devices (eg FT232R, FT-X) 
 * and 14 characters on multi port devices (eg FT2232H, FT4232H). If it is longer then it may be
 * truncated and will not have a null terminator.

 * For instance a serial number which is 15 characters long on a multi-port device will have an 
 * effective serial number which is 16 characters long since the serial number is appended with the 
 * channel identifier (A,B,etc). The buffer used to return the string from the API is only 16 
 * characters in size so the NULL termination will be lost.
 * @n If the serial number or description are too long in the EEPROM or configuration of a device then the
 * strings returned by FT_GetDeviceInfo and FT_ListDevices may not be NULL terminated
 */
/** @{ */

	/** @noop FT_ReadEE
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Read a value from an EEPROM location.
	 * @param ftHandle Handle of the device.
	 * @param dwWordOffset EEPROM location to read from.
	 * @param lpwValue Pointer to the WORD value read from the EEPROM.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * EEPROMs for FTDI devices are organised by WORD, so each value returned is 16-bits wide.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_ReadEE(
		FT_HANDLE ftHandle,
		DWORD dwWordOffset,
		LPWORD lpwValue
		);

	/** @noop FT_WriteEE
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Write a value to an EEPROM location.
	 * @param ftHandle Handle of the device.
	 * @param dwWordOffset EEPROM location to read from.
	 * @param wValue The WORD value write to the EEPROM.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * EEPROMs for FTDI devices are organised by WORD, so each value written to the EEPROM is 
	 * 16-bits wide.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_WriteEE(
		FT_HANDLE ftHandle,
		DWORD dwWordOffset,
		WORD wValue
		);

	/** @noop FT_EraseEE
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Erases the device EEPROM.
	 * @param ftHandle Handle of the device.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * This function will erase the entire contents of an EEPROM, including the user area.
	 * Note that the FT232R	and FT245R devices have an internal EEPROM that cannot be erased.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_EraseEE(
		FT_HANDLE ftHandle
		);

	/**
	 * Structure to hold program data for FT_EE_Program, FT_EE_ProgramEx, FT_EE_Read 
	 * and FT_EE_ReadEx functions.
	 * @see FT_EE_Read
	 * @see FT_EE_ReadEx
	 * @see FT_EE_Program
	 * @see FT_EE_ProgramEx
	 */
	typedef struct ft_program_data {

		DWORD Signature1;			/// Header - must be 0x00000000 
		DWORD Signature2;			/// Header - must be 0xffffffff
		DWORD Version;				/// Header - FT_PROGRAM_DATA version
		//			0 = original
		//			1 = FT2232 extensions
		//			2 = FT232R extensions
		//			3 = FT2232H extensions
		//			4 = FT4232H extensions
		//			5 = FT232H extensions

		WORD VendorId;				/// 0x0403
		WORD ProductId;				/// 0x6001
		char *Manufacturer;			/// "FTDI"
		char *ManufacturerId;		/// "FT"
		char *Description;			/// "USB HS Serial Converter"
		char *SerialNumber;			/// "FT000001" if fixed, or NULL
		WORD MaxPower;				/// 0 < MaxPower <= 500
		WORD PnP;					/// 0 = disabled, 1 = enabled
		WORD SelfPowered;			/// 0 = bus powered, 1 = self powered
		WORD RemoteWakeup;			/// 0 = not capable, 1 = capable
		/// Rev4 (FT232B) extensions
		UCHAR Rev4;					/// non-zero if Rev4 chip, zero otherwise
		UCHAR IsoIn;				/// non-zero if in endpoint is isochronous
		UCHAR IsoOut;				/// non-zero if out endpoint is isochronous
		UCHAR PullDownEnable;		/// non-zero if pull down enabled
		UCHAR SerNumEnable;			/// non-zero if serial number to be used
		UCHAR USBVersionEnable;		/// non-zero if chip uses USBVersion
		WORD USBVersion;			/// BCD (0x0200 => USB2)
		/// Rev 5 (FT2232) extensions
		UCHAR Rev5;					/// non-zero if Rev5 chip, zero otherwise
		UCHAR IsoInA;				/// non-zero if in endpoint is isochronous
		UCHAR IsoInB;				/// non-zero if in endpoint is isochronous
		UCHAR IsoOutA;				/// non-zero if out endpoint is isochronous
		UCHAR IsoOutB;				/// non-zero if out endpoint is isochronous
		UCHAR PullDownEnable5;		/// non-zero if pull down enabled
		UCHAR SerNumEnable5;		/// non-zero if serial number to be used
		UCHAR USBVersionEnable5;	/// non-zero if chip uses USBVersion
		WORD USBVersion5;			/// BCD (0x0200 => USB2)
		UCHAR AIsHighCurrent;		/// non-zero if interface is high current
		UCHAR BIsHighCurrent;		/// non-zero if interface is high current
		UCHAR IFAIsFifo;			/// non-zero if interface is 245 FIFO
		UCHAR IFAIsFifoTar;			/// non-zero if interface is 245 FIFO CPU target
		UCHAR IFAIsFastSer;			/// non-zero if interface is Fast serial
		UCHAR AIsVCP;				/// non-zero if interface is to use VCP drivers
		UCHAR IFBIsFifo;			/// non-zero if interface is 245 FIFO
		UCHAR IFBIsFifoTar;			/// non-zero if interface is 245 FIFO CPU target
		UCHAR IFBIsFastSer;			/// non-zero if interface is Fast serial
		UCHAR BIsVCP;				/// non-zero if interface is to use VCP drivers
		/// Rev 6 (FT232R) extensions
		UCHAR UseExtOsc;			/// Use External Oscillator
		UCHAR HighDriveIOs;			/// High Drive I/Os
		UCHAR EndpointSize;			/// Endpoint size
		UCHAR PullDownEnableR;		/// non-zero if pull down enabled
		UCHAR SerNumEnableR;		/// non-zero if serial number to be used
		UCHAR InvertTXD;			/// non-zero if invert TXD
		UCHAR InvertRXD;			/// non-zero if invert RXD
		UCHAR InvertRTS;			/// non-zero if invert RTS
		UCHAR InvertCTS;			/// non-zero if invert CTS
		UCHAR InvertDTR;			/// non-zero if invert DTR
		UCHAR InvertDSR;			/// non-zero if invert DSR
		UCHAR InvertDCD;			/// non-zero if invert DCD
		UCHAR InvertRI;				/// non-zero if invert RI
		UCHAR Cbus0;				/// Cbus Mux control
		UCHAR Cbus1;				/// Cbus Mux control
		UCHAR Cbus2;				/// Cbus Mux control
		UCHAR Cbus3;				/// Cbus Mux control
		UCHAR Cbus4;				/// Cbus Mux control
		UCHAR RIsD2XX;				/// non-zero if using D2XX driver
		/// Rev 7 (FT2232H) Extensions
		UCHAR PullDownEnable7;		/// non-zero if pull down enabled
		UCHAR SerNumEnable7;		/// non-zero if serial number to be used
		UCHAR ALSlowSlew;			/// non-zero if AL pins have slow slew
		UCHAR ALSchmittInput;		/// non-zero if AL pins are Schmitt input
		UCHAR ALDriveCurrent;		/// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR AHSlowSlew;			/// non-zero if AH pins have slow slew
		UCHAR AHSchmittInput;		/// non-zero if AH pins are Schmitt input
		UCHAR AHDriveCurrent;		/// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR BLSlowSlew;			/// non-zero if BL pins have slow slew
		UCHAR BLSchmittInput;		/// non-zero if BL pins are Schmitt input
		UCHAR BLDriveCurrent;		/// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR BHSlowSlew;			/// non-zero if BH pins have slow slew
		UCHAR BHSchmittInput;		/// non-zero if BH pins are Schmitt input
		UCHAR BHDriveCurrent;		/// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR IFAIsFifo7;			/// non-zero if interface is 245 FIFO
		UCHAR IFAIsFifoTar7;		/// non-zero if interface is 245 FIFO CPU target
		UCHAR IFAIsFastSer7;		/// non-zero if interface is Fast serial
		UCHAR AIsVCP7;				/// non-zero if interface is to use VCP drivers
		UCHAR IFBIsFifo7;			/// non-zero if interface is 245 FIFO
		UCHAR IFBIsFifoTar7;		/// non-zero if interface is 245 FIFO CPU target
		UCHAR IFBIsFastSer7;		/// non-zero if interface is Fast serial
		UCHAR BIsVCP7;				/// non-zero if interface is to use VCP drivers
		UCHAR PowerSaveEnable;		/// non-zero if using BCBUS7 to save power for self-powered designs
		/// Rev 8 (FT4232H) Extensions
		UCHAR PullDownEnable8;		/// non-zero if pull down enabled
		UCHAR SerNumEnable8;		/// non-zero if serial number to be used
		UCHAR ASlowSlew;			/// non-zero if A pins have slow slew
		UCHAR ASchmittInput;		/// non-zero if A pins are Schmitt input
		UCHAR ADriveCurrent;		/// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR BSlowSlew;			/// non-zero if B pins have slow slew
		UCHAR BSchmittInput;		/// non-zero if B pins are Schmitt input
		UCHAR BDriveCurrent;		/// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR CSlowSlew;			/// non-zero if C pins have slow slew
		UCHAR CSchmittInput;		/// non-zero if C pins are Schmitt input
		UCHAR CDriveCurrent;		/// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR DSlowSlew;			/// non-zero if D pins have slow slew
		UCHAR DSchmittInput;		/// non-zero if D pins are Schmitt input
		UCHAR DDriveCurrent;		/// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR ARIIsTXDEN;			/// non-zero if port A uses RI as RS485 TXDEN
		UCHAR BRIIsTXDEN;			/// non-zero if port B uses RI as RS485 TXDEN
		UCHAR CRIIsTXDEN;			/// non-zero if port C uses RI as RS485 TXDEN
		UCHAR DRIIsTXDEN;			/// non-zero if port D uses RI as RS485 TXDEN
		UCHAR AIsVCP8;				/// non-zero if interface is to use VCP drivers
		UCHAR BIsVCP8;				/// non-zero if interface is to use VCP drivers
		UCHAR CIsVCP8;				/// non-zero if interface is to use VCP drivers
		UCHAR DIsVCP8;				/// non-zero if interface is to use VCP drivers
		/// Rev 9 (FT232H) Extensions
		UCHAR PullDownEnableH;		/// non-zero if pull down enabled
		UCHAR SerNumEnableH;		/// non-zero if serial number to be used
		UCHAR ACSlowSlewH;			/// non-zero if AC pins have slow slew
		UCHAR ACSchmittInputH;		/// non-zero if AC pins are Schmitt input
		UCHAR ACDriveCurrentH;		/// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR ADSlowSlewH;			/// non-zero if AD pins have slow slew
		UCHAR ADSchmittInputH;		/// non-zero if AD pins are Schmitt input
		UCHAR ADDriveCurrentH;		/// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR Cbus0H;				/// Cbus Mux control
		UCHAR Cbus1H;				/// Cbus Mux control
		UCHAR Cbus2H;				/// Cbus Mux control
		UCHAR Cbus3H;				/// Cbus Mux control
		UCHAR Cbus4H;				/// Cbus Mux control
		UCHAR Cbus5H;				/// Cbus Mux control
		UCHAR Cbus6H;				/// Cbus Mux control
		UCHAR Cbus7H;				/// Cbus Mux control
		UCHAR Cbus8H;				/// Cbus Mux control
		UCHAR Cbus9H;				/// Cbus Mux control
		UCHAR IsFifoH;				/// non-zero if interface is 245 FIFO
		UCHAR IsFifoTarH;			/// non-zero if interface is 245 FIFO CPU target
		UCHAR IsFastSerH;			/// non-zero if interface is Fast serial
		UCHAR IsFT1248H;			/// non-zero if interface is FT1248
		UCHAR FT1248CpolH;			/// FT1248 clock polarity - clock idle high (1) or clock idle low (0)
		UCHAR FT1248LsbH;			/// FT1248 data is LSB (1) or MSB (0)
		UCHAR FT1248FlowControlH;	/// FT1248 flow control enable
		UCHAR IsVCPH;				/// non-zero if interface is to use VCP drivers
		UCHAR PowerSaveEnableH;		/// non-zero if using ACBUS7 to save power for self-powered designs

	} FT_PROGRAM_DATA, *PFT_PROGRAM_DATA;

	/** @noop FT_EE_Read
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Read the contents of the EEPROM.
	 * @param ftHandle Handle of the device.
	 * @param pData Pointer to structure of type FT_PROGRAM_DATA.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * This function interprets the parameter pData as a pointer to a structure of type FT_PROGRAM_DATA 
	 * that contains storage for the data to be read from the EEPROM.
	 * @n The function does not perform any checks on buffer sizes, so the buffers passed in the
	 * FT_PROGRAM_DATA structure must be big enough to accommodate their respective strings (including
	 * null terminators). The sizes shown in the following example are more than adequate and can be rounded
	 * down if necessary. The restriction is that the Manufacturer string length plus the Description string
	 * length is less than or equal to 40 characters.
	 * @note Note that the DLL must be informed which version of the FT_PROGRAM_DATA structure is being used.
	 * This is done through the Signature1, Signature2 and Version elements of the structure. Signature1
	 * should always be 0x00000000, Signature2 should always be 0xFFFFFFFF and Version can be set to use
	 * whichever version is required. For compatibility with all current devices Version should be set to the
	 * latest version of the FT_PROGRAM_DATA structure which is defined in FTD2XX.h. 
	 * @see FT_PROGRAM_DATA
	 * */
	FTD2XX_API FT_STATUS WINAPI FT_EE_Read(
		FT_HANDLE ftHandle,
		PFT_PROGRAM_DATA pData
		);

	/** @noop FT_EE_ReadEx
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Read the contents of the EEPROM and pass strings separately.
	 * @param ftHandle Handle of the device.
	 * @param pData Pointer to structure of type FT_PROGRAM_DATA.
	 * @param *Manufacturer Pointer to a null-terminated string containing the manufacturer name.
	 * @param *ManufacturerId Pointer to a null-terminated string containing the manufacturer ID.
	 * @param *Description Pointer to a null-terminated string containing the device description.
	 * @param *SerialNumber Pointer to a null-terminated string containing the device serial number.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * This variation of the standard FT_EE_Read function was included to provide support for languages such
	 * as LabVIEW where problems can occur when string pointers are contained in a structure.
	 * @n This function interprets the parameter pData as a pointer to a structure of type FT_PROGRAM_DATA that
	 * contains storage for the data to be read from the EEPROM.
	 * @n The function does not perform any checks on buffer sizes, so the buffers passed in the
	 * FT_PROGRAM_DATA structure must be big enough to accommodate their respective strings (including
	 * null terminators).
	 * @note Note that the DLL must be informed which version of the FT_PROGRAM_DATA structure is being used.
	 * This is done through the Signature1, Signature2 and Version elements of the structure. Signature1
	 * should always be 0x00000000, Signature2 should always be 0xFFFFFFFF and Version can be set to use
	 * whichever version is required. For compatibility with all current devices Version should be set to the
	 * latest version of the FT_PROGRAM_DATA structure which is defined in FTD2XX.h.
	 * @n The string parameters in the FT_PROGRAM_DATA structure should be passed as DWORDs to avoid
	 * overlapping of parameters. All string pointers are passed out separately from the FT_PROGRAM_DATA
	 * structure.
	 * @see FT_PROGRAM_DATA
	 * */
	FTD2XX_API FT_STATUS WINAPI FT_EE_ReadEx(
		FT_HANDLE ftHandle,
		PFT_PROGRAM_DATA pData,
		char *Manufacturer,
		char *ManufacturerId,
		char *Description,
		char *SerialNumber
		);

	/** @noop FT_EE_Program
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Program the EEPROM.
	 * @param ftHandle Handle of the device.
	 * @param pData Pointer to structure of type FT_PROGRAM_DATA.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * This function interprets the parameter pData as a pointer to a structure of type FT_PROGRAM_DATA 
	 * that	contains the data to write to the EEPROM. The data is written to EEPROM, then read back and 
	 * verified.
	 * @n If the SerialNumber field in FT_PROGRAM_DATA is NULL, or SerialNumber points to a NULL string, 
	 * a serial number based on the ManufacturerId and the current date and time will be generated. The
	 * Manufacturer string length plus the Description string length must be less than or equal to 40 
	 * characters.
	 * @note Note that the DLL must be informed which version of the FT_PROGRAM_DATA structure is being 
	 * used. This is done through the Signature1, Signature2 and Version elements of the structure. 
	 * Signature1 should always be 0x00000000, Signature2 should always be 0xFFFFFFFF and Version can be 
	 * set to use whichever version is required. For compatibility with all current devices Version 
	 * should be set to the latest version of the FT_PROGRAM_DATA structure which is defined in FTD2XX.h.
	 * If pData is NULL, the structure version will default to 0 (original BM series) and the device will 
	 * be programmed with the default data.
	 * @see FT_PROGRAM_DATA
	 */
	FTD2XX_API FT_STATUS WINAPI FT_EE_Program(
		FT_HANDLE ftHandle,
		PFT_PROGRAM_DATA pData
		);

	/** @noop FT_EE_ProgramEx
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Program the EEPROM and pass strings separately.
	 * @param ftHandle Handle of the device.
	 * @param pData Pointer to structure of type FT_PROGRAM_DATA.
	 * @param *Manufacturer Pointer to a null-terminated string containing the manufacturer name.
	 * @param *ManufacturerId Pointer to a null-terminated string containing the manufacturer ID.
	 * @param *Description Pointer to a null-terminated string containing the device description.
	 * @param *SerialNumber Pointer to a null-terminated string containing the device serial number.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * This variation of the FT_EE_Program function was included to provide support for languages such 
	 * as LabVIEW where problems can occur when string pointers are contained in a structure.
	 * @n This function interprets the parameter pData as a pointer to a structure of type FT_PROGRAM_DATA 
	 * that contains the data to write to the EEPROM. The data is written to EEPROM, then read back and 
	 * verified. The string pointer parameters in the FT_PROGRAM_DATA structure should be allocated as 
	 * DWORDs to avoid overlapping of parameters. The string parameters are then passed in separately.
	 * @n If the SerialNumber field is NULL, or SerialNumber points to a NULL string, a serial number based 
	 * on the ManufacturerId and the current date and time will be generated. The Manufacturer string 
	 * length plus the Description string length must be less than or equal to 40 characters.
	 * @note Note that the DLL must be informed which version of the FT_PROGRAM_DATA structure is being used.
	 * This is done through the Signature1, Signature2 and Version elements of the structure. Signature1
	 * should always be 0x00000000, Signature2 should always be 0xFFFFFFFF and Version can be set to use
	 * whichever version is required. For compatibility with all current devices Version should be set to the
	 * latest version of the FT_PROGRAM_DATA structure which is defined in FTD2XX.h.
	 * If pData is NULL, the structure version will default to 0 (original BM series) and the device will be
	 * programmed with the default data.
	 * @see FT_PROGRAM_DATA
	 */
	FTD2XX_API FT_STATUS WINAPI FT_EE_ProgramEx(
		FT_HANDLE ftHandle,
		PFT_PROGRAM_DATA pData,
		char *Manufacturer,
		char *ManufacturerId,
		char *Description,
		char *SerialNumber
		);

	/** @noop FT_EE_UASize
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Get the available size of the EEPROM user area.
	 * @param ftHandle Handle of the device.
	 * @param lpdwSize Pointer to a DWORD that receives the available size, in bytes, of the EEPROM 
	 * user area.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * The user area of an FTDI device EEPROM is the total area of the EEPROM that is unused by device
	 * configuration information and descriptors. This area is available to the user to store information 
	 * specific	to their application. The size of the user area depends on the length of the Manufacturer,
	 * ManufacturerId, Description and SerialNumber strings programmed into the EEPROM.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_EE_UASize(
		FT_HANDLE ftHandle,
		LPDWORD lpdwSize
		);

	/** @noop FT_EE_UARead
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Read the contents of the EEPROM user area.
	 * @param ftHandle Handle of the device.
	 * @param pucData Pointer to a buffer that contains storage for data to be read.
	 * @param dwDataLen Size, in bytes, of buffer that contains storage for the data to be read.
	 * @param lpdwBytesRead Pointer to a DWORD that receives the number of bytes read.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * This function interprets the parameter pucData as a pointer to an array of bytes of size 
	 * dwDataLen that contains storage for the data to be read from the EEPROM user area. The actual 
	 * number of bytes read is stored in the DWORD referenced by lpdwBytesRead.
	 * @n If dwDataLen is less than the size of the EEPROM user area, then dwDataLen bytes are read 
	 * into the buffer. Otherwise, the whole of the EEPROM user area is read into the buffer. The 
	 * available user area size can be determined by calling FT_EE_UASize.
	 * @n An application should check the function return value and lpdwBytesRead when FT_EE_UARead 
	 * returns.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_EE_UARead(
		FT_HANDLE ftHandle,
		PUCHAR pucData,
		DWORD dwDataLen,
		LPDWORD lpdwBytesRead
		);

	/** @noop FT_EE_UAWrite
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Write data into the EEPROM user area.
	 * @param ftHandle Handle of the device.
	 * @param pucData Pointer to a buffer that contains the data to be written.
	 * @param dwDataLen Size, in bytes, of buffer that contains storage for the data to be read.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * This function interprets the parameter pucData as a pointer to an array of bytes of size 
	 * dwDataLen that contains the data to be written to the EEPROM user area. It is a programming 
	 * error for dwDataLen to be greater than the size of the EEPROM user area. The available user 
	 * area size can be determined by calling FT_EE_UASize.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_EE_UAWrite(
		FT_HANDLE ftHandle,
		PUCHAR pucData,
		DWORD dwDataLen
		);

	/**  @noop FT_EEPROM_HEADER
	 * @par Summary
	 * Structure to hold data for FT_EEPROM_Program and FT_EEPROM_Read functions.
	 * The structure for the command includes one FT_EEPROM_HEADER with a device-specific
	 * structure appended.
	 * @see FT_EEPROM_Read
	 * @see FT_EEPROM_Program
	 */
	typedef struct ft_eeprom_header {
		FT_DEVICE deviceType;		/// FTxxxx device type to be programmed
		/// Device descriptor options
		WORD VendorId;				/// 0x0403
		WORD ProductId;				/// 0x6001
		UCHAR SerNumEnable;			/// non-zero if serial number to be used
		/// Config descriptor options
		WORD MaxPower;				/// 0 < MaxPower <= 500
		UCHAR SelfPowered;			/// 0 = bus powered, 1 = self powered
		UCHAR RemoteWakeup;			/// 0 = not capable, 1 = capable
		/// Hardware options
		UCHAR PullDownEnable;		/// non-zero if pull down in suspend enabled
	} FT_EEPROM_HEADER, *PFT_EEPROM_HEADER;

	/**  @noop FT_EEPROM_232B
	 * @par Summary
	 * Structure to hold data for the FT232B data in the FT_EEPROM_Program and FT_EEPROM_Read functions.
	 * This is appended to an FT_EEPROM_HEADER structure.
	 * @see FT_EEPROM_HEADER
	 */
	typedef struct ft_eeprom_232b {
		/// Common header
		FT_EEPROM_HEADER common;	/// common elements for all device EEPROMs
	} FT_EEPROM_232B, *PFT_EEPROM_232B;

	/**  @noop FT_EEPROM_2232
	 * @par Summary
	 * Structure to hold data for the FT2232C, FT2232D and FT2232L data in the FT_EEPROM_Program 
	 * and FT_EEPROM_Read functions.
	 * This is appended to an FT_EEPROM_HEADER structure.
	 * @see FT_EEPROM_HEADER
	 */
	typedef struct ft_eeprom_2232 {
		/// Common header
		FT_EEPROM_HEADER common;	/// common elements for all device EEPROMs
		/// Drive options
		UCHAR AIsHighCurrent;		/// non-zero if interface is high current
		UCHAR BIsHighCurrent;		/// non-zero if interface is high current
		/// Hardware options
		UCHAR AIsFifo;				/// non-zero if interface is 245 FIFO
		UCHAR AIsFifoTar;			/// non-zero if interface is 245 FIFO CPU target
		UCHAR AIsFastSer;			/// non-zero if interface is Fast serial
		UCHAR BIsFifo;				/// non-zero if interface is 245 FIFO
		UCHAR BIsFifoTar;			/// non-zero if interface is 245 FIFO CPU target
		UCHAR BIsFastSer;			/// non-zero if interface is Fast serial
		/// Driver option
		UCHAR ADriverType;			/// non-zero if interface is to use VCP drivers
		UCHAR BDriverType;			/// non-zero if interface is to use VCP drivers
	} FT_EEPROM_2232, *PFT_EEPROM_2232;

	/**  @noop FT_EEPROM_232R
	 * @par Summary
	 * Structure to hold data for the FT232R data in the FT_EEPROM_Program and FT_EEPROM_Read functions.
	 * This is appended to an FT_EEPROM_HEADER structure.
	 * @see FT_EEPROM_HEADER
	 */
	typedef struct ft_eeprom_232r {
		/// Common header
		FT_EEPROM_HEADER common;	/// common elements for all device EEPROMs
		/// Drive options
		UCHAR IsHighCurrent;		/// non-zero if interface is high current
		/// Hardware options
		UCHAR UseExtOsc;			/// Use External Oscillator
		UCHAR InvertTXD;			/// non-zero if invert TXD
		UCHAR InvertRXD;			/// non-zero if invert RXD
		UCHAR InvertRTS;			/// non-zero if invert RTS
		UCHAR InvertCTS;			/// non-zero if invert CTS
		UCHAR InvertDTR;			/// non-zero if invert DTR
		UCHAR InvertDSR;			/// non-zero if invert DSR
		UCHAR InvertDCD;			/// non-zero if invert DCD
		UCHAR InvertRI;				/// non-zero if invert RI
		UCHAR Cbus0;				/// Cbus Mux control
		UCHAR Cbus1;				/// Cbus Mux control
		UCHAR Cbus2;				/// Cbus Mux control
		UCHAR Cbus3;				/// Cbus Mux control
		UCHAR Cbus4;				/// Cbus Mux control
		/// Driver option
		UCHAR DriverType;			/// non-zero if using D2XX driver
	} FT_EEPROM_232R, *PFT_EEPROM_232R;

	/**  @noop FT_EEPROM_2232H
	 * @par Summary
	 * Structure to hold data for the FT2232H data in the FT_EEPROM_Program and FT_EEPROM_Read functions.
	 * This is appended to an FT_EEPROM_HEADER structure.
	 * @see FT_EEPROM_HEADER
	 */
	typedef struct ft_eeprom_2232h {
		/// Common header
		FT_EEPROM_HEADER common;	/// common elements for all device EEPROMs
		/// Drive options
		UCHAR ALSlowSlew;			/// non-zero if AL pins have slow slew
		UCHAR ALSchmittInput;		/// non-zero if AL pins are Schmitt input
		UCHAR ALDriveCurrent;		/// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR AHSlowSlew;			/// non-zero if AH pins have slow slew
		UCHAR AHSchmittInput;		/// non-zero if AH pins are Schmitt input
		UCHAR AHDriveCurrent;		/// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR BLSlowSlew;			/// non-zero if BL pins have slow slew
		UCHAR BLSchmittInput;		/// non-zero if BL pins are Schmitt input
		UCHAR BLDriveCurrent;		/// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR BHSlowSlew;			/// non-zero if BH pins have slow slew
		UCHAR BHSchmittInput;		/// non-zero if BH pins are Schmitt input
		UCHAR BHDriveCurrent;		/// valid values are 4mA, 8mA, 12mA, 16mA
		/// Hardware options
		UCHAR AIsFifo;				/// non-zero if interface is 245 FIFO
		UCHAR AIsFifoTar;			/// non-zero if interface is 245 FIFO CPU target
		UCHAR AIsFastSer;			/// non-zero if interface is Fast serial
		UCHAR BIsFifo;				/// non-zero if interface is 245 FIFO
		UCHAR BIsFifoTar;			/// non-zero if interface is 245 FIFO CPU target
		UCHAR BIsFastSer;			/// non-zero if interface is Fast serial
		UCHAR PowerSaveEnable;		/// non-zero if using BCBUS7 to save power for self-powered designs
		/// Driver option
		UCHAR ADriverType;			/// non-zero if interface is to use VCP drivers
		UCHAR BDriverType;			/// non-zero if interface is to use VCP drivers
	} FT_EEPROM_2232H, *PFT_EEPROM_2232H;

	/**  @noop FT_EEPROM_4232H
	 * @par Summary
	 * Structure to hold data for the FT4232H data in the FT_EEPROM_Program and FT_EEPROM_Read functions.
	 * This is appended to an FT_EEPROM_HEADER structure.
	 * @see FT_EEPROM_HEADER
	 */
	typedef struct ft_eeprom_4232h {
		/// Common header
		FT_EEPROM_HEADER common;	/// common elements for all device EEPROMs
		/// Drive options
		UCHAR ASlowSlew;			/// non-zero if A pins have slow slew
		UCHAR ASchmittInput;		/// non-zero if A pins are Schmitt input
		UCHAR ADriveCurrent;		/// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR BSlowSlew;			/// non-zero if B pins have slow slew
		UCHAR BSchmittInput;		/// non-zero if B pins are Schmitt input
		UCHAR BDriveCurrent;		/// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR CSlowSlew;			/// non-zero if C pins have slow slew
		UCHAR CSchmittInput;		/// non-zero if C pins are Schmitt input
		UCHAR CDriveCurrent;		/// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR DSlowSlew;			/// non-zero if D pins have slow slew
		UCHAR DSchmittInput;		/// non-zero if D pins are Schmitt input
		UCHAR DDriveCurrent;		/// valid values are 4mA, 8mA, 12mA, 16mA
		/// Hardware options
		UCHAR ARIIsTXDEN;			/// non-zero if port A uses RI as RS485 TXDEN
		UCHAR BRIIsTXDEN;			/// non-zero if port B uses RI as RS485 TXDEN
		UCHAR CRIIsTXDEN;			/// non-zero if port C uses RI as RS485 TXDEN
		UCHAR DRIIsTXDEN;			/// non-zero if port D uses RI as RS485 TXDEN
		/// Driver option
		UCHAR ADriverType;			/// non-zero if interface is to use VCP drivers
		UCHAR BDriverType;			/// non-zero if interface is to use VCP drivers
		UCHAR CDriverType;			/// non-zero if interface is to use VCP drivers
		UCHAR DDriverType;			/// non-zero if interface is to use VCP drivers
	} FT_EEPROM_4232H, *PFT_EEPROM_4232H;

	/**  @noop FT_EEPROM_232H
	 * @par Summary
	 * Structure to hold data for the FT232H data in the FT_EEPROM_Program and FT_EEPROM_Read functions.
	 * This is appended to an FT_EEPROM_HEADER structure.
	 * @see FT_EEPROM_HEADER
	 */
	typedef struct ft_eeprom_232h {
		/// Common header
		FT_EEPROM_HEADER common;	/// common elements for all device EEPROMs
		/// Drive options
		UCHAR ACSlowSlew;			/// non-zero if AC bus pins have slow slew
		UCHAR ACSchmittInput;		/// non-zero if AC bus pins are Schmitt input
		UCHAR ACDriveCurrent;		/// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR ADSlowSlew;			/// non-zero if AD bus pins have slow slew
		UCHAR ADSchmittInput;		/// non-zero if AD bus pins are Schmitt input
		UCHAR ADDriveCurrent;		/// valid values are 4mA, 8mA, 12mA, 16mA
		/// CBUS options
		UCHAR Cbus0;				/// Cbus Mux control
		UCHAR Cbus1;				/// Cbus Mux control
		UCHAR Cbus2;				/// Cbus Mux control
		UCHAR Cbus3;				/// Cbus Mux control
		UCHAR Cbus4;				/// Cbus Mux control
		UCHAR Cbus5;				/// Cbus Mux control
		UCHAR Cbus6;				/// Cbus Mux control
		UCHAR Cbus7;				/// Cbus Mux control
		UCHAR Cbus8;				/// Cbus Mux control
		UCHAR Cbus9;				/// Cbus Mux control
		/// FT1248 options
		UCHAR FT1248Cpol;			/// FT1248 clock polarity - clock idle high (1) or clock idle low (0)
		UCHAR FT1248Lsb;			/// FT1248 data is LSB (1) or MSB (0)
		UCHAR FT1248FlowControl;	/// FT1248 flow control enable
		/// Hardware options
		UCHAR IsFifo;				/// non-zero if interface is 245 FIFO
		UCHAR IsFifoTar;			/// non-zero if interface is 245 FIFO CPU target
		UCHAR IsFastSer;			/// non-zero if interface is Fast serial
		UCHAR IsFT1248;			/// non-zero if interface is FT1248
		UCHAR PowerSaveEnable;		/// 
		/// Driver option
		UCHAR DriverType;			/// non-zero if interface is to use VCP drivers
	} FT_EEPROM_232H, *PFT_EEPROM_232H;

	/**  @noop FT_EEPROM_X_SERIES
	 * @par Summary
	 * Structure to hold data for the FT-X series data in the FT_EEPROM_Program and FT_EEPROM_Read functions.
	 * This is appended to an FT_EEPROM_HEADER structure.
	 * @see FT_EEPROM_HEADER
	 */
	typedef struct ft_eeprom_x_series {
		/// Common header
		FT_EEPROM_HEADER common;	/// common elements for all device EEPROMs
		/// Drive options
		UCHAR ACSlowSlew;			/// non-zero if AC bus pins have slow slew
		UCHAR ACSchmittInput;		/// non-zero if AC bus pins are Schmitt input
		UCHAR ACDriveCurrent;		/// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR ADSlowSlew;			/// non-zero if AD bus pins have slow slew
		UCHAR ADSchmittInput;		/// non-zero if AD bus pins are Schmitt input
		UCHAR ADDriveCurrent;		/// valid values are 4mA, 8mA, 12mA, 16mA
		/// CBUS options
		UCHAR Cbus0;				/// Cbus Mux control
		UCHAR Cbus1;				/// Cbus Mux control
		UCHAR Cbus2;				/// Cbus Mux control
		UCHAR Cbus3;				/// Cbus Mux control
		UCHAR Cbus4;				/// Cbus Mux control
		UCHAR Cbus5;				/// Cbus Mux control
		UCHAR Cbus6;				/// Cbus Mux control
		/// UART signal options
		UCHAR InvertTXD;			/// non-zero if invert TXD
		UCHAR InvertRXD;			/// non-zero if invert RXD
		UCHAR InvertRTS;			/// non-zero if invert RTS
		UCHAR InvertCTS;			/// non-zero if invert CTS
		UCHAR InvertDTR;			/// non-zero if invert DTR
		UCHAR InvertDSR;			/// non-zero if invert DSR
		UCHAR InvertDCD;			/// non-zero if invert DCD
		UCHAR InvertRI;				/// non-zero if invert RI
		/// Battery Charge Detect options
		UCHAR BCDEnable;			/// Enable Battery Charger Detection
		UCHAR BCDForceCbusPWREN;	/// asserts the power enable signal on CBUS when charging port detected
		UCHAR BCDDisableSleep;		/// forces the device never to go into sleep mode
		/// I2C options
		WORD I2CSlaveAddress;		/// I2C slave device address
		DWORD I2CDeviceId;			/// I2C device ID
		UCHAR I2CDisableSchmitt;	/// Disable I2C Schmitt trigger
		/// FT1248 options
		UCHAR FT1248Cpol;			/// FT1248 clock polarity - clock idle high (1) or clock idle low (0)
		UCHAR FT1248Lsb;			/// FT1248 data is LSB (1) or MSB (0)
		UCHAR FT1248FlowControl;	/// FT1248 flow control enable
		/// Hardware options
		UCHAR RS485EchoSuppress;	/// 
		UCHAR PowerSaveEnable;		/// 
		/// Driver option
		UCHAR DriverType;			/// non-zero if interface is to use VCP drivers
	} FT_EEPROM_X_SERIES, *PFT_EEPROM_X_SERIES;

	/**  @noop FT_EEPROM_4222H
	 * @par Summary
	 * Structure to hold data for the FT4222H data in the FT_EEPROM_Program and FT_EEPROM_Read functions.
	 * This is appended to an FT_EEPROM_HEADER structure.
	 * @see FT_EEPROM_HEADER
	 */
	typedef struct ft_eeprom_4222h {
		/// Common header
		FT_EEPROM_HEADER common;	/// common elements for all device EEPROMs
		CHAR Revision;				/// 'A', 'B', 'C', or 'D'.
		UCHAR I2C_Slave_Address;
		/// Suspend
		UCHAR SPISuspend;			/// 0 for "Disable SPI, tristate pins", 2 for "Keep SPI pin status", 3 for "Enable SPI pin control"
		UCHAR SuspendOutPol;		/// 0 for negative, 1 for positive (not implemented on Rev A)
		UCHAR EnableSuspendOut;		/// non-zero to enable (not implemented on Rev A)
		/// QSPI
		UCHAR Clock_SlowSlew;		/// non-zero if clock pin has slow slew
		UCHAR Clock_Drive;			/// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR IO0_SlowSlew;			/// non-zero if IO0 pin has slow slew
		UCHAR IO1_SlowSlew;			/// non-zero if IO1 pin has slow slew
		UCHAR IO2_SlowSlew;			/// non-zero if IO2 pin has slow slew
		UCHAR IO3_SlowSlew;			/// non-zero if IO3 pin has slow slew
		UCHAR IO_Drive; 			/// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR SlaveSelect_PullUp;	/// non-zero to enable pull up
		UCHAR SlaveSelect_PullDown;	/// non-zero to enable pull down
		UCHAR SlaveSelect_Drive;	/// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR SlaveSelect_SlowSlew;	/// non-zero if slave select pin has slow slew
		UCHAR MISO_Suspend;			/// 2 for push-low, 3 for push high, 0 and 1 reserved
		UCHAR SIMO_Suspend;			/// 2 for push-low, 3 for push high, 0 and 1 reserved
		UCHAR IO2_IO3_Suspend;		/// 2 for push-low, 3 for push high, 0 and 1 reserved
		UCHAR SlaveSelect_Suspend;	/// 0 for no-change (not implemented on Rev A), 2 for push-low, 3 for push high, 1 reserved
		/// GPIO
		UCHAR GPIO0_Drive;			/// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR GPIO1_Drive;			/// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR GPIO2_Drive;			/// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR GPIO3_Drive;			/// valid values are 4mA, 8mA, 12mA, 16mA
		UCHAR GPIO0_SlowSlew;		/// non-zero if IO0 pin has slow slew
		UCHAR GPIO1_SlowSlew;		/// non-zero if IO0 pin has slow slew
		UCHAR GPIO2_SlowSlew;		/// non-zero if IO0 pin has slow slew
		UCHAR GPIO3_SlowSlew;		/// non-zero if IO0 pin has slow slew
		UCHAR GPIO0_PullDown;		/// non-zero to enable pull down
		UCHAR GPIO1_PullDown;		/// non-zero to enable pull down
		UCHAR GPIO2_PullDown;		/// non-zero to enable pull down
		UCHAR GPIO3_PullDown;		/// non-zero to enable pull down
		UCHAR GPIO0_PullUp;			/// non-zero to enable pull up
		UCHAR GPIO1_PullUp;			/// non-zero to enable pull up
		UCHAR GPIO2_PullUp;			/// non-zero to enable pull up
		UCHAR GPIO3_PullUp;			/// non-zero to enable pull up
		UCHAR GPIO0_OpenDrain;		/// non-zero to enable open drain
		UCHAR GPIO1_OpenDrain;		/// non-zero to enable open drain
		UCHAR GPIO2_OpenDrain;		/// non-zero to enable open drain
		UCHAR GPIO3_OpenDrain;		/// non-zero to enable open drain
		UCHAR GPIO0_Suspend;		/// 0 for no-change, 1 for input (not implemented on Rev A), 2 for push-low, 3 for push high
		UCHAR GPIO1_Suspend;		/// 0 for no-change, 1 for input (not implemented on Rev A), 2 for push-low, 3 for push high
		UCHAR GPIO2_Suspend;		/// 0 for no-change, 1 for input (not implemented on Rev A), 2 for push-low, 3 for push high
		UCHAR GPIO3_Suspend;		/// 0 for no-change, 1 for input (not implemented on Rev A), 2 for push-low, 3 for push high
		UCHAR FallingEdge;			/// non-zero to change GPIO on falling edge
		/// BCD
		UCHAR BCD_Disable;			/// non-zero to disable BCD
		UCHAR BCD_OutputActiveLow;	/// non-zero to set BCD output active low
		UCHAR BCD_Drive;			/// valid values are 4mA, 8mA, 12mA, 16mA
	} FT_EEPROM_4222H, *PFT_EEPROM_4222H;

	/**  @noop FT_EEPROM_PD_PDO_mv_ma
	 * @par Summary
	 * Structure to hold PDO Configuration structure, mA supported values 0 to 10230mA, mV supported 
	 * values 0 to 51100mV. This is part of the FT_EEPROM_PD structure.
	 * @see FT_EEPROM_PD
	 */
	typedef struct ft_eeprom_PD_PDO_mv_ma {
		USHORT PDO1ma;	/// PDO1 mA
		USHORT PDO1mv;	/// PDO1 mV
		USHORT PDO2ma;	/// PDO2 mA
		USHORT PDO2mv;	/// PDO2 mV
		USHORT PDO3ma;	/// PDO3 mA
		USHORT PDO3mv;	/// PDO3 mV
		USHORT PDO4ma;	/// PDO4 mA
		USHORT PDO4mv;	/// PDO4 mV
		USHORT PDO5ma;	/// PDO5 mA (FTx233HP only)
		USHORT PDO5mv;	/// PDO5 mV (FTx233HP only)
		USHORT PDO6ma;	/// PDO6 mA (FTx233HP only)
		USHORT PDO6mv;	/// PDO6 mV (FTx233HP only)
		USHORT PDO7ma;	/// PDO7 mA (FTx233HP only)
		USHORT PDO7mv;	/// PDO7 mV (FTx233HP only)
	} FT_EEPROM_PD_PDO_mv_ma;

	/**  @noop FT_EEPROM_PD
	 * @par Summary
	 * Structure to hold power delivery configuration data for the FT4233PD, FT2233PD, FT4232PD, 
	 * FT2232PD, FT233PD and FT232PD in the FT_EEPROM_Program and FT_EEPROM_Read functions.
	 * This is appended to an FT_EEPROM_HEADER and a base device structure.
	 * e_g. @verbatim
	 *		struct {
	 *			FT_EEPROM_xxx base;
	 *			FT_EEPROM_PD pd;
	 *		};
	 * @endverbatim
	 * @remarks
 	 * Device GPIO values are:
	 * @li	FTx233HP - 0 to 7, 15 for N/A
	 * @li FTx232HP - 0 to 3, 15 for N/A
	 * @see FT_EEPROM_HEADER
	 * @see FT_EEPROM_PD_PDO_mv_ma
	 */
	typedef struct ft_eeprom_pd {
		/// Configuration
		UCHAR srprs;		/// non-zero to enable Sink Request Power Role Swap
		UCHAR sraprs;		/// non-zero to enable Sink Accept PR Swap
		UCHAR srrprs;		/// non-zero to enable Source Request PR SWAP
		UCHAR saprs;		/// non-zero to enable Source Accept PR SWAP
		UCHAR vconns;		/// non-zero to enable vConn Swap
		UCHAR passthru;		/// non-zero to enable Pass Through (FTx233HP only)
		UCHAR extmcu;		/// non-zero to enable External MCU
		UCHAR pd2en;		/// non-zero to enable PD2 (FTx233HP only)
		UCHAR pd1autoclk;	/// non-zero to enable PD1 Auto Clock
		UCHAR pd2autoclk;	/// non-zero to enable PD2 Auto Clock (FTx233HP only)
		UCHAR useefuse;		/// non-zero to Use EFUSE
		UCHAR extvconn;		/// non-zero to enable External vConn

		/// GPIO Configuration
		UCHAR count;		/// GPIO Count, supported values are 0 to 7 
		UCHAR gpio1;		/// GPIO Number 1, supports device GPIO values
		UCHAR gpio2;		/// GPIO Number 2, supports device GPIO values
		UCHAR gpio3;		/// GPIO Number 3, supports device GPIO values
		UCHAR gpio4;		/// GPIO Number 4, supports device GPIO values
		UCHAR gpio5;		/// GPIO Number 5, supports device GPIO values (FTx233HP only)
		UCHAR gpio6;		/// GPIO Number 6, supports device GPIO values (FTx233HP only)
		UCHAR gpio7;		/// GPIO Number 7, supports device GPIO values (FTx233HP only)
		UCHAR pd1lden;		/// PD1 Load Enable, supports device GPIO values
		UCHAR pd2lden;		/// PD2 Load Enable, supports device GPIO values (FTx233HP only)
		UCHAR dispin;		/// Discharge Pin, supports device GPIO values
		UCHAR disenbm;		/// Discharge Enable BM, 0 for "Drive Hi", 1 for "Drive Low", 2 for "Input Mode", 3 for "Don't Care"
		UCHAR disdisbm;		/// Discharge Disable BM, 0 for "Drive Hi", 1 for "Drive Low", 2 for "Input Mode", 3 for "Don't Care"
		UCHAR ccselect;		/// CC Select Indicator, supports device GPIO values

		/// ISET Configuration
		UCHAR iset1;		/// ISET1, supports device GPIO values
		UCHAR iset2;		/// ISET2, supports device GPIO values
		UCHAR iset3;		/// ISET3, supports device GPIO values
		UCHAR extiset;		/// non-zero to enable EXTEND_ISET
		UCHAR isetpd2;		/// non-zero to enable ISET_PD2
		UCHAR iseten;		/// non-zero to set ISET_ENABLED

		/// BM Configuration, 0 for "Drive Hi", 1 for "Drive Low", 2 for "Input Mode", 3 for "Don't Care"
		UCHAR PDO1_GPIO[7];		/// PDO1 GPIO1 to GPIO7
		UCHAR PDO2_GPIO[7];		/// PDO2 GPIO1 to GPIO7
		UCHAR PDO3_GPIO[7];		/// PDO3 GPIO1 to GPIO7
		UCHAR PDO4_GPIO[7];		/// PDO4 GPIO1 to GPIO7
		UCHAR PDO5_GPIO[7];		/// PDO5 GPIO1 to GPIO7 (FTx233HP only)
		UCHAR PDO6_GPIO[7];		/// PDO6 GPIO1 to GPIO7 (FTx233HP only)
		UCHAR PDO7_GPIO[7];		/// PDO7 GPIO1 to GPIO7 (FTx233HP only)
		UCHAR VSET0V_GPIO[7];	/// PDO7 GPIO1 to GPIO7
		UCHAR VSAFE5V_GPIO[7];	/// PDO7 GPIO1 to GPIO7

		FT_EEPROM_PD_PDO_mv_ma BM_PDO_Sink;
		FT_EEPROM_PD_PDO_mv_ma BM_PDO_Source;
		FT_EEPROM_PD_PDO_mv_ma BM_PDO_Sink_2; // (FTx233HP only)

		/// PD Timers
		UCHAR srt;			/// Sender Response Timer
		UCHAR hrt;			/// Hard Reset Timer
		UCHAR sct;			/// Source Capability Timer
		UCHAR dit;			/// Discover Identity Timer
		USHORT srcrt;		/// Source Recover Timer
		USHORT trt;			/// Transition Timer
		USHORT sofft;		/// Source off timer
		USHORT nrt;			/// No Response Timer
		USHORT swct;		/// Sink Wait Capability Timer
		USHORT snkrt;		/// Sink Request Timer
		UCHAR dt;			/// Discharge Timer
		UCHAR cnst;			/// Chunk not supported timer
		USHORT it;			/// Idle Timer

		/// PD Control
		UCHAR i2caddr;		/// I2C Address (hex)
		UINT prou;			/// Power Reserved for OWN use
		UINT trim1;			/// TRIM1
		UINT trim2;			/// TRIM2
		UCHAR extdc;		/// non-zero to enable ETERNAL_DC_POWER
	} FT_EEPROM_PD, *PFT_EEPROM_PD;

	/// FT2233HP EEPROM structure for use with FT_EEPROM_Read and FT_EEPROM_Program
	/// FT2232H with power delivery
	typedef struct _ft_eeprom_2233hp
	{
		FT_EEPROM_2232H	ft2232h;
		FT_EEPROM_PD	pd;
	} FT_EEPROM_2233HP, *PFT_EEPROM_2233HP;

	/// FT4233HP EEPROM structure for use with FT_EEPROM_Read and FT_EEPROM_Program
	/// FT4232H with power delivery
	typedef struct _ft_eeprom_4233hp
	{
		FT_EEPROM_4232H	ft4232h;
		FT_EEPROM_PD	pd;
	} FT_EEPROM_4233HP, *PFT_EEPROM_4233HP;

	/// FT2232HP EEPROM structure for use with FT_EEPROM_Read and FT_EEPROM_Program
	/// FT2232H with power delivery
	typedef struct _ft_eeprom_2232hp
	{
		FT_EEPROM_2232H	ft2232h;
		FT_EEPROM_PD	pd;
	} FT_EEPROM_2232HP, *PFT_EEPROM_2232HP;

	/// FT4232HP EEPROM structure for use with FT_EEPROM_Read and FT_EEPROM_Program
	/// FT4232H with power delivery
	typedef struct _ft_eeprom_4232hp
	{
		FT_EEPROM_4232H	ft4232h;
		FT_EEPROM_PD	pd;
	} FT_EEPROM_4232HP, *PFT_EEPROM_4232HP;

	/// FT233HP EEPROM structure for use with FT_EEPROM_Read and FT_EEPROM_Program
	/// FT233H with power delivery
	typedef struct _ft_eeprom_233hp
	{
		FT_EEPROM_232H	ft232h;
		FT_EEPROM_PD	pd;
	} FT_EEPROM_233HP, *PFT_EEPROM_233HP;

	/// FT232HP EEPROM structure for use with FT_EEPROM_Read and FT_EEPROM_Program
	/// FT232H with power delivery
	typedef struct _ft_eeprom_232hp
	{
		FT_EEPROM_232H	ft232h;
		FT_EEPROM_PD	pd;
	} FT_EEPROM_232HP, *PFT_EEPROM_232HP;

	/** @noop FT_EEPROM_Read
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (XP and later)
	 * @par Summary 
	 * Read data from the EEPROM, this command will work for all existing FTDI chipset, and must be 
	 * used for the FT-X series.
	 * @param ftHandle Handle of the device.
	 * @param *eepromData Pointer to a buffer that contains the data to be read.
	 * Note: This structure is different for each device type.
	 * @param eepromDataSize Size of the eepromData buffer that contains storage for the data to be read.
	 * @param *Manufacturer Pointer to a null-terminated string containing the manufacturer	name.
	 * @param *ManufacturerId Pointer to a null-terminated string containing the manufacturer ID.
	 * @param *Description Pointer to a null-terminated string containing the device description.
	 * @param *SerialNumber Pointer to a null-terminated string containing the device serial number.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * This function interprets the parameter *eepromDATA as a pointer to a structure matching the device
	 * type being accessed e.g.
	 * @li PFT_EEPROM_232B is the structure for FT2xxB devices.
	 * @li PFT_EEPROM_2232 is the structure for FT2232D devices.
	 * @li PFT_EEPROM_232R is the structure for FT232R devices.
	 * @li PFT_EEPROM_2232H is the structure for FT2232H devices.
	 * @li PFT_EEPROM_4232H is the structure for FT4232H devices.
	 * @li PFT_EEPROM_232H is the structure for FT232H devices.
	 * @li PFT_EEPROM_X_SERIES is the structure for FT2xxX devices.
	 * 
	 * The function does not perform any checks on buffer sizes, so the buffers passed in the eepromDATA
	 * structure must be big enough to accommodate their respective strings (including null terminators).
	 * The sizes shown in the following example are more than adequate and can be rounded down if necessary.
	 * The restriction is that the Manufacturer string length plus the Description string length is less than or
	 * equal to 40 characters.
	 * @note Note that the DLL must be informed which version of the eepromDATA structure is being used. This is
	 * done through the PFT_EEPROM_HEADER structure. The first element of this structure is deviceType and
	 * may be FT_DEVICE_BM, FT_DEVICE_AM, FT_DEVICE_2232C, FT_DEVICE_232R, FT_DEVICE_2232H,
	 * FT_DEVICE_4232H, FT_DEVICE_232H, or FT_DEVICE_X_SERIES as defined in FTD2XX.h.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_EEPROM_Read(
		FT_HANDLE ftHandle,
		void *eepromData,
		DWORD eepromDataSize,
		char *Manufacturer,
		char *ManufacturerId,
		char *Description,
		char *SerialNumber
		);

	/** @noop FT_EEPROM_Program
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (XP and later)
	 * @par Summary
	 * Write data into the EEPROM, this command will work for all existing FTDI chipset, and must be used for
	 * the FT-X series.
	 * @param ftHandle Handle of the device.
	 * @param *eepromData Pointer to a buffer that contains the data to be written.
	 * Note: This structure is different for each device type.
	 * @param eepromDataSize Size of the eepromData buffer that contains storage for the data to be written.
	 * @param *Manufacturer Pointer to a null-terminated string containing the manufacturer name.
	 * @param *ManufacturerId Pointer to a null-terminated string containing the manufacturer ID.
	 * @param *Description Pointer to a null-terminated string containing the device description.
	 * @param *SerialNumber Pointer to a null-terminated string containing the device serial number.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * This function interprets the parameter *eepromDATA as a pointer to a structure matching the device
	 * type being accessed e.g.
	 * @li PFT_EEPROM_232B is the structure for FT2xxB devices.
	 * @li PFT_EEPROM_2232 is the structure for FT2232D devices.
	 * @li PFT_EEPROM_232R is the structure for FT232R devices.
	 * @li PFT_EEPROM_2232H is the structure for FT2232H devices.
	 * @li PFT_EEPROM_4232H is the structure for FT4232H devices.
	 * @li PFT_EEPROM_232H is the structure for FT232H devices.
	 * @li PFT_EEPROM_X_SERIES is the structure for FT2xxX devices.
	 * 
	 * The function does not perform any checks on buffer sizes, so the buffers passed in the eepromDATA
	 * structure must be big enough to accommodate their respective strings (including null terminators).
	 * @n The sizes shown in the following example are more than adequate and can be rounded down if necessary.
	 * The restriction is that the Manufacturer string length plus the Description string length is less than or
	 * equal to 40 characters.
	 * @note Note that the DLL must be informed which version of the eepromDATA structure is being used. This is
	 * done through the PFT_EEPROM_HEADER structure. The first element of this structure is deviceType and
	 * may be FT_DEVICE_BM, FT_DEVICE_AM, FT_DEVICE_2232C, FT_DEVICE_232R, FT_DEVICE_2232H,
	 * FT_DEVICE_4232H, FT_DEVICE_232H, or FT_DEVICE_X_SERIES as defined in FTD2XX.h.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_EEPROM_Program(
		FT_HANDLE ftHandle,
		void *eepromData,
		DWORD eepromDataSize,
		char *Manufacturer,
		char *ManufacturerId,
		char *Description,
		char *SerialNumber
		);

/** @} */

/** @name Extended API Functions
 * The extended API functions do not apply to FT8U232AM or FT8U245AM devices. FTDI’s other USB-UART
 * and USB-FIFO ICs (the FT2232H, FT4232H, FT232R, FT245R, FT2232, FT232B and FT245B) do support
 * these functions. Note that there is device dependence in some of these functions.
 */
/** @{ */

	/** @noop FT_SetLatencyTimer
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Set the latency timer value.
	 * @param ftHandle Handle of the device.
	 * @param ucLatency Required value, in milliseconds, of latency timer. Valid range is 2 - 255.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * In the FT8U232AM and FT8U245AM devices, the receive buffer timeout that is used to flush remaining
	 * data from the receive buffer was fixed at 16 ms. In all other FTDI devices, this timeout is 
	 * programmable and can be set at 1 ms intervals between 2ms and 255 ms. This allows the device to 
	 * be better optimize for protocols requiring faster response times from short data packets.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_SetLatencyTimer(
		FT_HANDLE ftHandle,
		UCHAR ucLatency
		);

	/** @noop FT_GetLatencyTimer
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Get the current value of the latency timer.
	 * @param ftHandle Handle of the device.
	 * @param pucLatency Pointer to unsigned char to store latency timer value.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * In the FT8U232AM and FT8U245AM devices, the receive buffer timeout that is used to flush remaining
	 * data from the receive buffer was fixed at 16 ms. In all other FTDI devices, this timeout is 
	 * programmable and can be set at 1 ms intervals between 2ms and 255 ms. This allows the device to  
	 * be better optimized for protocols requiring faster response times from short data packets.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_GetLatencyTimer(
		FT_HANDLE ftHandle,
		PUCHAR pucLatency
		);

	/** @noop FT_SetBitMode
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Enables different chip modes.
	 * @param ftHandle Handle of the device.
	 * @param ucMask Required value for bit mode mask. This sets up which bits are inputs and outputs.
	 * A bit value of 0 sets the corresponding pin to an input, a bit value of 1 sets the corresponding 
	 * pin to an output.
	 * @n In the case of CBUS Bit Bang, the upper nibble of this value controls which pins are inputs 
	 * and outputs,	while the lower nibble controls which of the outputs are high and low.
	 * @param ucEnable Mode value. Can be one of the following:
	 * @li 0x0 = Reset
	 * @li 0x1 = Asynchronous Bit Bang
	 * @li 0x2 = MPSSE (FT2232, FT2232H, FT4232H and FT232H devices only)
	 * @li 0x4 = Synchronous Bit Bang (FT232R, FT245R, FT2232, FT2232H, FT4232H and FT232H devices only)
	 * @li 0x8 = MCU Host Bus Emulation Mode (FT2232, FT2232H, FT4232H and FT232H devices only)
	 * @li 0x10 = Fast Opto-Isolated Serial Mode (FT2232, FT2232H, FT4232H and FT232H devices only)
	 * @li 0x20 = CBUS Bit Bang Mode (FT232R and FT232H devices only)
	 * @li 0x40 = Single Channel Synchronous 245 FIFO Mode (FT2232H and FT232H devices only)
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * For a description of available bit modes for the FT232R, see the application note "Bit Bang Modes 
	 * for the FT232R and FT245R".
	 * @n For a description of available bit modes for the FT2232, see the application note "Bit Mode
	 * Functions for the FT2232".
	 * @n For a description of Bit Bang Mode for the FT232B and FT245B, see the application note 
	 * "FT232B/FT245B Bit Bang Mode".
	 * @n Application notes are available for download from the FTDI website.
	 * Note that to use CBUS Bit Bang for the FT232R, the CBUS must be configured for CBUS Bit Bang in the
	 * EEPROM.
	 * @note Note that to use Single Channel Synchronous 245 FIFO mode for the FT2232H, channel A must be
	 * configured for FT245 FIFO mode in the EEPROM.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_SetBitMode(
		FT_HANDLE ftHandle,
		UCHAR ucMask,
		UCHAR ucEnable
		);

	/** @noop FT_GetBitMode
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Gets the instantaneous value of the data bus.
	 * @param ftHandle Handle of the device.
	 * @param pucMode Pointer to unsigned char to store the instantaneous data bus value.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * For a description of available bit modes for the FT232R, see the application note "Bit Bang Modes 
	 * for the FT232R and FT245R".
	 * @n For a description of available bit modes for the FT2232, see the application note "Bit Mode 
	 * Functions for the FT2232".
	 * @n For a description of bit bang modes for the FT232B and FT245B, see the application note
	 * "FT232B/FT245B Bit Bang Mode".
	 * @n For a description of bit modes supported by the FT4232H and FT2232H devices, please see the 
	 * IC data sheets.
	 * @n These application notes are available for download from the FTDI website.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_GetBitMode(
		FT_HANDLE ftHandle,
		PUCHAR pucMode
		);

	/** @noop FT_SetUSBParameters
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Set the USB request transfer size.
	 * @param ftHandle Handle of the device.
	 * @param ulInTransferSize Transfer size for USB IN request.
	 * @param ulOutTransferSize Transfer size for USB OUT request.
	 * @returns
	 * FT_OK if successful, otherwise the return value is an FT error code.
	 * @remarks
	 * This function can be used to change the transfer sizes from the default transfer size of 4096 
	 * bytes to	better suit the application requirements. Transfer sizes must be set to a multiple 
	 * of 64 bytes between 64 bytes and 64k bytes.
	 * @n When FT_SetUSBParameters is called, the change comes into effect immediately and any data 
	 * that was	held in the driver at the time of the change is lost.
	 * @note Note that, at present, only ulInTransferSize is supported.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_SetUSBParameters(
		FT_HANDLE ftHandle,
		ULONG ulInTransferSize,
		ULONG ulOutTransferSize
		);

/** @} */

/** @name FT-Win32 API Functions
 * The functions in this section are supplied to ease porting from a Win32 serial port application. These
 * functions are supported under non-Windows platforms to assist with porting existing applications from
 * Windows. Note that classic D2XX functions and the Win32 D2XX functions should not be mixed unless
 * stated.
 */
/** @{ */

	/** @noop FT_W32_CreateFile
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Opens the specified device and return a handle which will be used for subsequent accesses. 
	 * The device can be specified by its serial number, device description, or location.
	 * @n This function must be used if overlapped I/O is required.
	 * @param lpszName Meaning depends on the value of dwAttrsAndFlags. Can be a pointer to a null 
	 * terminated string that contains the description or serial number of the device, or can be 
	 * the location of the device. These values can be obtained from the FT_CreateDeviceInfoList, 
	 * FT_GetDeviceInfoDetail or FT_ListDevices	functions.
	 * @param dwAccess Type of access to the device. Access can be GENERIC_READ,
	 * GENERIC_WRITE or both. Ignored in Linux.
	 * @param dwShareMode How the device is shared. This value must be set to 0.
	 * @param lpSecurityAttributes This parameter has no effect and should be set to NULL.
	 * @param dwCreate This parameter must be set to OPEN_EXISTING. Ignored in Linux.
	 * @param dwAttrsAndFlags File attributes and flags. This parameter is a combination of
	 * FILE_ATTRIBUTE_NORMAL, FILE_FLAG_OVERLAPPED if overlapped I/O is used,
	 * FT_OPEN_BY_SERIAL_NUMBER if lpszName is the device’s serial number, and
	 * FT_OPEN_BY_DESCRIPTION if lpszName is the device’s description.
	 * @param hTemplate This parameter must be NULL.
	 * @returns
	 * If the function is successful, the return value is a handle.
	 * If the function is unsuccessful, the return value is the Win32 error code INVALID_HANDLE_VALUE.
	 * @remarks
	 * The meaning of lpszName depends on dwAttrsAndFlags: if FT_OPEN_BY_SERIAL_NUMBER or
	 * FT_OPEN_BY_DESCRIPTION is set in dwAttrsAndFlags, lpszName contains a pointer to a null terminated
	 * string that contains the device's serial number or description; if FT_OPEN_BY_LOCATION is set in
	 * dwAttrsAndFlags, lpszName is interpreted as a value of type long that contains the location ID of 
	 * the device. dwAccess can be GENERIC_READ, GENERIC_WRITE or both; dwShareMode must be set to 0;
	 * lpSecurityAttributes must be set to NULL; dwCreate must be set to OPEN_EXISTING; dwAttrsAndFlags 
	 * is a combination of FILE_ATTRIBUTE_NORMAL, FILE_FLAG_OVERLAPPED if overlapped I/O is used,
	 * FT_OPEN_BY_SERIAL_NUMBER or FT_OPEN_BY_DESCRIPTION or FT_OPEN_BY_LOCATION; hTemplate
	 * must be NULL.
	 * @note Note that Linux, Mac OS X and Windows CE do not support overlapped IO. Windows CE
	 * does not support location IDs.
	 */
	FTD2XX_API FT_HANDLE WINAPI FT_W32_CreateFile(
		LPCTSTR					lpszName,
		DWORD					dwAccess,
		DWORD					dwShareMode,
		LPSECURITY_ATTRIBUTES	lpSecurityAttributes,
		DWORD					dwCreate,
		DWORD					dwAttrsAndFlags,
		HANDLE					hTemplate
		);

	/** @noop FT_W32_CloseHandle
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Close the specified device handle.
	 * @param ftHandle Handle of the device.
	 * @returns
	 * If the function is successful, the return value is nonzero.
	 * If the function is unsuccessful, the return value is zero.
	 */
	FTD2XX_API BOOL WINAPI FT_W32_CloseHandle(
		FT_HANDLE ftHandle
		);

	/** @noop FT_W32_ReadFile
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Read data from the device.
	 * @param ftHandle Handle of the device.
	 * @param lpBuffer Pointer to a buffer that receives the data from the device.
	 * @param nBufferSize Number of bytes to read from the device.
	 * @param lpdwBytesReturned Pointer to a variable that receives the number of bytes read from
	 * the device.
	 * @param lpOverlapped Pointer to an overlapped structure.
	 * @returns 
	 * If the function is successful, the return value is nonzero.
	 * If the function is unsuccessful, the return value is zero.
	 * @remarks
	 * This function supports both non-overlapped and overlapped I/O, except under Linux, Mac OS X 
	 * and Windows CE where only non-overlapped IO is supported.
	 * @n @b Non-overlapped @b I/O
	 * @n The parameter, lpOverlapped, must be NULL for non-overlapped I/O.
	 * @n This function always returns the number of bytes read in lpdwBytesReturned.
	 * This function does not return until dwBytesToRead have been read into the buffer. The number of 
	 * bytes in the receive queue can be determined by calling FT_GetStatus or FT_GetQueueStatus, and
	 * passed as dwBytesToRead so that the function reads the device and returns immediately.
	 * @n When a read timeout has been setup in a previous call to FT_W32_SetCommTimeouts, this function
	 * returns when the timer expires or dwBytesToRead have been read, whichever occurs first. If a 
	 * timeout occurred, any available data is read into lpBuffer and the function returns a non-zero value.
	 * @n An application should use the function return value and lpdwBytesReturned when processing the 
	 * buffer. If the return value is non-zero and lpdwBytesReturned is equal to dwBytesToRead then the 
	 * function has	completed normally. If the return value is non-zero and lpdwBytesReturned is less 
	 * then dwBytesToRead then a timeout has occurred, and the read request has been partially completed. 
	 * @note Note that if a timeout	occurred and no data was read, the return value is still non-zero.
	 * @n @b Overlapped @b I/O
	 * @n When the device has been opened for overlapped I/O, an application can issue a request and 
	 * perform some additional work while the request is pending. This contrasts with the case 
	 * of non-overlapped I/O in	which the application issues a request and receives control again only 
	 * after the request has been completed.
	 * @n The parameter, lpOverlapped, must point to an initialized OVERLAPPED structure. If there is 
	 * enough data in the receive queue to satisfy the request, the request completes immediately and 
	 * the return code is non-zero. The number of bytes read is returned in lpdwBytesReturned.
	 * @n If there is not enough data in the receive queue to satisfy the request, the request completes
	 * immediately, and the return code is zero, signifying an error. An application should call
	 * FT_W32_GetLastError to get the cause of the error. If the error code is ERROR_IO_PENDING, the
	 * overlapped operation is still in progress, and the application can perform other processing. 
	 * Eventually, the application checks the result of the overlapped request by calling 
	 * FT_W32_GetOverlappedResult.
	 * @n If successful, the number of bytes read is returned in lpdwBytesReturned.
	 */
	FTD2XX_API BOOL WINAPI FT_W32_ReadFile(
		FT_HANDLE ftHandle,
		LPVOID lpBuffer,
		DWORD nBufferSize,
		LPDWORD lpdwBytesReturned,
		LPOVERLAPPED lpOverlapped
		);

	/** @noop FT_W32_WriteFile
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Write data to the device.
	 * @param ftHandle Handle of the device.
	 * @param lpBuffer Pointer to the buffer that contains the data to write to the device.
	 * @param nBufferSize Number of bytes to be written to the device.
	 * @param lpdwBytesWritten Pointer to a variable that receives the number of bytes written to the device.
	 * @param lpOverlapped Pointer to an overlapped structure.
	 * @returns
	 * If the function is successful, the return value is nonzero.
	 * If the function is unsuccessful, the return value is zero.
	 * @remarks
	 * This function supports both non-overlapped and overlapped I/O, except under Linux, Mac OS X and
	 * Windows CE where only non-overlapped IO is supported.
	 * @n @b Non-overlapped @b I/O
	 * @n The parameter, lpOverlapped, must be NULL for non-overlapped I/O.
	 * @n This function always returns the number of bytes written in lpdwBytesWritten.
	 * This function does not return until dwBytesToWrite have been written to the device.
	 * When a write timeout has been setup in a previous call to FT_W32_SetCommTimeouts, this function
	 * returns when the timer expires or dwBytesToWrite have been written, whichever occurs first. If a 
	 * timeout occurred, lpdwBytesWritten contains the number of bytes actually written, and the function 
	 * returns a non-zero value.
	 * @n An application should always use the function return value and lpdwBytesWritten. If the return 
	 * value is	non-zero and lpdwBytesWritten is equal to dwBytesToWrite then the function has completed 
	 * normally. If	the return value is non-zero and lpdwBytesWritten is less then dwBytesToWrite then a 
	 * timeout has occurred, and the write request has been partially completed. 
	 * @note Note that if a timeout occurred and no data was written, the return value is still non-zero.
	 * @n @b Overlapped @b I/O
	 * @n When the device has been opened for overlapped I/O, an application can issue a request and perform
	 * some additional work while the request is pending. This contrasts with the case of non-overlapped 
	 * I/O in which the application issues a request and receives control again only after the request has 
	 * been	completed.
	 * @n The parameter, lpOverlapped, must point to an initialized OVERLAPPED structure.
	 * This function completes immediately, and the return code is zero, signifying an error. An application
	 * should call FT_W32_GetLastError to get the cause of the error. If the error code is ERROR_IO_PENDING,
	 * the overlapped operation is still in progress, and the application can perform other processing.
	 * Eventually, the application checks the result of the overlapped request by calling
	 * FT_W32_GetOverlappedResult.
	 * @n If successful, the number of bytes written is returned in lpdwBytesWritten.
	 */
	FTD2XX_API BOOL WINAPI FT_W32_WriteFile(
		FT_HANDLE ftHandle,
		LPVOID lpBuffer,
		DWORD nBufferSize,
		LPDWORD lpdwBytesWritten,
		LPOVERLAPPED lpOverlapped
		);

	/** @noop FT_W32_GetOverlappedResult
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Gets the result of an overlapped operation.
	 * @param ftHandle Handle of the device.
	 * @param lpOverlapped Pointer to an overlapped structure.
	 * @param lpdwBytesTransferred Pointer to a variable that receives the number of bytes transferred
	 * during the overlapped operation.
	 * @param bWait Set to TRUE if the function does not return until the operation has been completed.
	 * @returns
	 * If the function is successful, the return value is nonzero.
	 * If the function is unsuccessful, the return value is zero.
	 * @remarks
	 * This function is used with overlapped I/O and so is not supported in Linux, Mac OS X or Windows CE. For
	 * a description of its use, see FT_W32_ReadFile and FT_W32_WriteFile.
	 */
	FTD2XX_API BOOL WINAPI FT_W32_GetOverlappedResult(
		FT_HANDLE ftHandle,
		LPOVERLAPPED lpOverlapped,
		LPDWORD lpdwBytesTransferred,
		BOOL bWait
		);

	/** @noop FT_W32_EscapeCommFunction
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Perform an extended function.
	 * @param ftHandle Handle of the device.
	 * @param dwFunc The extended function to perform can be one of the following values:
	 * @li CLRDTR - Clear the DTR signal
	 * @li CLRRTS - Clear the RTS signal
	 * @li SETDTR - Set the DTR signal
	 * @li SETRTS - Set the RTS signal
	 * @li SETBREAK - Set the BREAK condition
	 * @li CLRBREAK - Clear the BREAK condition
	 * @returns
	 * If the function is successful, the return value is nonzero.
	 * If the function is unsuccessful, the return value is zero.
	 */
	FTD2XX_API BOOL WINAPI FT_W32_EscapeCommFunction(
		FT_HANDLE ftHandle,
		DWORD dwFunc
		);

	/** @noop FT_W32_GetCommModemStatus
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * This function gets the current modem control value.
	 * @param ftHandle Handle of the device.
	 * @param lpdwModemStatus Pointer to a variable to contain modem control value. The modem
	 * control value can be a combination of the following:
	 * @li MS_CTS_ON - Clear To Send (CTS) is on
	 * @li MS_DSR_ON - Data Set Ready (DSR) is on
	 * @li MS_RING_ON - Ring Indicator (RI) is on
	 * @li MS_RLSD_ON - Receive Line Signal Detect (RLSD) is on
	 * @returns
	 * If the function is successful, the return value is nonzero.
	 * If the function is unsuccessful, the return value is zero.
	 */
	FTD2XX_API BOOL WINAPI FT_W32_GetCommModemStatus(
		FT_HANDLE ftHandle,
		LPDWORD lpdwModemStatus
		);

	/** @noop FT_W32_SetupComm
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * This function sets the read and write buffers.
	 * @param ftHandle Handle of the device.
	 * @param dwReadBufferSize Length, in bytes, of the read buffer.
	 * @param dwWriteBufferSize Length, in bytes, of the write buffer.
	 * @returns
	 * If the function is successful, the return value is nonzero.
	 * If the function is unsuccessful, the return value is zero.
	 * @remarks
	 * This function has no effect. It is the responsibility of the driver to allocate sufficient 
	 * storage for I/O requests.
	 */
	FTD2XX_API BOOL WINAPI FT_W32_SetupComm(
		FT_HANDLE ftHandle,
		DWORD dwReadBufferSize,
		DWORD dwWriteBufferSize
		);

/**
 * Structure for FT_W32_ClearCommError lpftComstat parameter.
 * @see FT_W32_ClearCommError
 */
typedef struct _FTCOMSTAT {
	DWORD fCtsHold : 1;
	DWORD fDsrHold : 1;
	DWORD fRlsdHold : 1;
	DWORD fXoffHold : 1;
	DWORD fXoffSent : 1;
	DWORD fEof : 1;
	DWORD fTxim : 1;
	DWORD fReserved : 25;
	DWORD cbInQue;
	DWORD cbOutQue;
} FTCOMSTAT, *LPFTCOMSTAT;

/** Structure for FT_W32_SetCommState and FT_W32_GetCommState lpftDcb parameter.
 * @see FT_W32_SetCommState
 * @see FT_W32_GetCommState
 */
typedef struct _FTDCB {
	DWORD DCBlength;              /** sizeof(FTDCB)                        */
	DWORD BaudRate;               /** Baudrate at which running            */
	DWORD fBinary : 1;            /** Binary Mode (skip EOF check)         */
	DWORD fParity : 1;            /** Enable parity checking               */
	DWORD fOutxCtsFlow : 1;       /** CTS handshaking on output            */
	DWORD fOutxDsrFlow : 1;       /** DSR handshaking on output            */
	DWORD fDtrControl : 2;        /** DTR Flow control                     */
	DWORD fDsrSensitivity : 1;    /** DSR Sensitivity                      */
	DWORD fTXContinueOnXoff : 1;  /** Continue TX when Xoff sent           */
	DWORD fOutX : 1;              /** Enable output X-ON/X-OFF             */
	DWORD fInX : 1;               /** Enable input X-ON/X-OFF              */
	DWORD fErrorChar : 1;         /** Enable Err Replacement               */
	DWORD fNull : 1;              /** Enable Null stripping                */
	DWORD fRtsControl : 2;        /** Rts Flow control                     */
	DWORD fAbortOnError : 1;      /** Abort all reads and writes on Error  */
	DWORD fDummy2 : 17;           /** Reserved                             */
	WORD wReserved;               /** Not currently used                   */
	WORD XonLim;                  /** Transmit X-ON threshold              */
	WORD XoffLim;                 /** Transmit X-OFF threshold             */
	BYTE ByteSize;                /** Number of bits/byte, 4-8             */
	BYTE Parity;                  /** 0-4=None,Odd,Even,Mark,Space         */
	BYTE StopBits;                /** FT_STOP_BITS_1 or FT_STOP_BITS_2     */
	char XonChar;                 /** Tx and Rx X-ON character             */
	char XoffChar;                /** Tx and Rx X-OFF character            */
	char ErrorChar;               /** Error replacement char               */
	char EofChar;                 /** End of Input character               */
	char EvtChar;                 /** Received Event character             */
	WORD wReserved1;              /** Fill for now.                        */
} FTDCB, *LPFTDCB;

/** Structure for FT_W32_SetCommTimeouts and FT_W32_GetCommTimeouts lpftTimeouts parameter.
 * @see FT_W32_SetCommTimeouts
 * @see FT_W32_GetCommTimeouts
 */
typedef struct _FTTIMEOUTS {
	DWORD ReadIntervalTimeout;          /** Maximum time between read chars.  */
	DWORD ReadTotalTimeoutMultiplier;   /** Multiplier of characters.         */
	DWORD ReadTotalTimeoutConstant;     /** Constant in milliseconds.         */
	DWORD WriteTotalTimeoutMultiplier;  /** Multiplier of characters.         */
	DWORD WriteTotalTimeoutConstant;    /** Constant in milliseconds.         */
} FTTIMEOUTS, *LPFTTIMEOUTS;

	/** @noop FT_W32_SetCommState
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * This function sets the state of the device according to the contents of a device control block (DCB).
	 * @param ftHandle Handle of the device.
	 * @param lpftDcb Pointer to an FTDCB structure.
	 * @returns
	 * If the function is successful, the return value is nonzero.
	 * If the function is unsuccessful, the return value is zero.
	 */
	FTD2XX_API BOOL WINAPI FT_W32_SetCommState(
		FT_HANDLE ftHandle,
		LPFTDCB lpftDcb
		);

	/** @noop FT_W32_GetCommState
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * This function gets the current device state.
	 * @param ftHandle Handle of the device.
	 * @param lpftDcb Pointer to an FTDCB structure.
	 * @returns
	 * If the function is successful, the return value is nonzero.
	 * If the function is unsuccessful, the return value is zero.
	 * @remarks
	 * The current state of the device is returned in a device control block.
	 */
	FTD2XX_API BOOL WINAPI FT_W32_GetCommState(
		FT_HANDLE ftHandle,
		LPFTDCB lpftDcb
		);

	/** @noop FT_W32_SetCommTimeouts
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * This function sets the timeout parameters for I/O requests.
	 * @param ftHandle Handle of the device.
	 * @param pftTimeouts Pointer to an FTTIMEOUTS structure to store timeout information.
	 * @returns
	 * If the function is unsuccessful, the return value is zero.
	 * @remarks
	 * Timeouts are calculated using the information in the FTTIMEOUTS structure.
	 * @n For read requests, the number of bytes to be read is multiplied by the total timeout 
	 * multiplier, and added to the total timeout constant. So, if TS is an FTTIMEOUTS structure 
	 * and the number of bytes to read is dwToRead, the read timeout, rdTO, is calculated as follows.
	 * @n rdTO = (dwToRead * TS.ReadTotalTimeoutMultiplier) + TS.ReadTotalTimeoutConstant
	 * @n For write requests, the number of bytes to be written is multiplied by the total timeout 
	 * multiplier, and added to the total timeout constant. So, if TS is an FTTIMEOUTS structure 
	 * and the number of bytes to write is dwToWrite, the write timeout, wrTO, is calculated as follows.
	 * @n wrTO = (dwToWrite * TS.WriteTotalTimeoutMultiplier) + TS.WriteTotalTimeoutConstant
	 * @n Linux and Mac OS X currently ignore the ReadIntervalTimeout, ReadTotalTimeoutMultiplier and
	 * WriteTotalTimeoutMultiplier.
	 */
	FTD2XX_API BOOL WINAPI FT_W32_SetCommTimeouts(
		FT_HANDLE ftHandle,
		FTTIMEOUTS *pftTimeouts
		);

	/** @noop FT_W32_GetCommTimeouts
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * This function gets the current read and write request timeout parameters for the specified device.
	 * @param ftHandle Handle of the device.
	 * @param pftTimeouts Pointer to an FTTIMEOUTS structure to store timeout information.
	 * @returns
	 * If the function is successful, the return value is nonzero.
	 * If the function is unsuccessful, the return value is zero.
	 * @remarks
	 * For an explanation of how timeouts are used, see FT_W32_SetCommTimeouts.
	 */
	FTD2XX_API BOOL WINAPI FT_W32_GetCommTimeouts(
		FT_HANDLE ftHandle,
		FTTIMEOUTS *pftTimeouts
		);

	/** @noop FT_W32_SetCommBreak
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Puts the communications line in the BREAK state.
	 * @param ftHandle Handle of the device.
	 * @returns
	 * If the function is successful, the return value is nonzero.
	 * If the function is unsuccessful, the return value is zero.
	 */
	FTD2XX_API BOOL WINAPI FT_W32_SetCommBreak(
		FT_HANDLE ftHandle
		);

	/** @noop FT_W32_ClearCommBreak
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Puts the communications line in the non-BREAK state.
	 * @param ftHandle Handle of the device.
	 * @returns
	 * If the function is successful, the return value is nonzero.
	 * If the function is unsuccessful, the return value is zero.
	 */
	FTD2XX_API BOOL WINAPI FT_W32_ClearCommBreak(
		FT_HANDLE ftHandle
		);

	/** @noop FT_W32_SetCommMask
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * This function specifies events that the device has to monitor.
	 * @param ftHandle Handle of the device.
	 * @param ulEventMask Mask containing events that the device has to monitor. This can be a combination of 
	 * the following:
	 * @li EV_BREAK - BREAK condition detected
	 * @li EV_CTS - Change in Clear To Send (CTS)
	 * @li EV_DSR - Change in Data Set Ready (DSR)
	 * @li EV_ERR - Error in line status
	 * @li EV_RING - Change in Ring Indicator (RI)
	 * @li EV_RLSD - Change in Receive Line Signal Detect (RLSD)
	 * @li EV_RXCHAR - Character received
	 * @li EV_RXFLAG - Event character received
	 * @li EV_TXEMPTY - Transmitter empty
	 * @returns
	 * If the function is successful, the return value is nonzero.
	 * If the function is unsuccessful, the return value is zero.
	 * @remarks
	 * This function specifies the events that the device should monitor. An application can call the 
	 * function FT_W32_WaitCommEvent to wait for an event to occur.
	 */
	FTD2XX_API BOOL WINAPI FT_W32_SetCommMask(
		FT_HANDLE ftHandle,
		ULONG ulEventMask
		);

	/** @noop 6 FT_W32_GetCommMask
	 * @par Supported Operating Systems
	 * Windows (2000 and later)
	 * @par Summary
	 * Retrieves the events that are currently being monitored by a device.
	 * @param ftHandle Handle of the device.
	 * @param lpdwEventMask Pointer to a location that receives a mask that contains the events that are 
	 * currently enabled. This parameter can be one or more of the following values:
	 * @li EV_BREAK - BREAK condition detected
	 * @li EV_CTS - Change in Clear To Send (CTS)
	 * @li EV_DSR - Change in Data Set Ready (DSR)
	 * @li EV_ERR - Error in line status
	 * @li EV_RING - Change in Ring Indicator (RI)
	 * @li EV_RLSD - Change in Receive Line Signal Detect (RLSD)
	 * @li EV_RXCHAR - Character received
	 * @li EV_RXFLAG - Event character received
	 * @li EV_TXEMPTY - Transmitter empty
	 * @returns
	 * If the function is successful, the return value is nonzero.
	 * If the function is unsuccessful, the return value is zero.
	 * @remarks
	 * This function returns events currently being monitored by the device. Event monitoring for these 
	 * events is enabled by the FT_W32_SetCommMask function.
	 */
	FTD2XX_API BOOL WINAPI FT_W32_GetCommMask(
		FT_HANDLE ftHandle,
		LPDWORD lpdwEventMask
		);

	/** @noop FT_W32_WaitCommEvent
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * This function waits for an event to occur.
	 * @param ftHandle Handle of the device.
	 * @param pulEvent Pointer to a location that receives a mask that contains the events that occurred.
	 * @param lpOverlapped Pointer to an overlapped structure.
	 * @returns 
	 * If the function is successful, the return value is nonzero.
	 * If the function is unsuccessful, the return value is zero.
	 * @remarks
	 * This function supports both non-overlapped and overlapped I/O, except under Windows CE and Linux
	 * where only non-overlapped IO is supported.
	 * @n @b Non-overlapped @b I/O
	 * The parameter, lpOverlapped, must be NULL for non-overlapped I/O.
	 * @n This function does not return until an event that has been specified in a call to 
	 * FT_W32_SetCommMask has occurred. The events that occurred and resulted in this function returning 
	 * are stored in lpdwEvent.
	 * @n @b Overlapped @b I/O
	 * @n When the device has been opened for overlapped I/O, an application can issue a request and perform
	 * some additional work while the request is pending. This contrasts with the case of non-overlapped 
	 * I/O in which the application issues a request and receives control again only after the request has 
	 * been	completed.
	 * @n The parameter, lpOverlapped, must point to an initialized OVERLAPPED structure.
	 * This function does not return until an event that has been specified in a call to FT_W32_SetCommMask
	 * has occurred.
	 * @n If an event has already occurred, the request completes immediately, and the return code is non-zero.
	 * @n The events that occurred are stored in lpdwEvent.
	 * @n If an event has not yet occurred, the request completes immediately, and the return code is zero,
	 * signifying an error. An application should call FT_W32_GetLastError to get the cause of the error. If 
	 * the error code is ERROR_IO_PENDING, the overlapped operation is still in progress, and the application 
	 * can perform other processing. Eventually, the application checks the result of the overlapped request 
	 * by calling FT_W32_GetOverlappedResult. The events that occurred and resulted in this function 
	 * returning are stored in lpdwEvent.
	 */
	FTD2XX_API BOOL WINAPI FT_W32_WaitCommEvent(
		FT_HANDLE ftHandle,
		PULONG pulEvent,
		LPOVERLAPPED lpOverlapped
		);

	/** @noop FT_W32_PurgeComm
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * This function purges the device.
	 * @param ftHandle Handle of the device.
	 * @param dwMask Specifies the action to take. The action can be a combination of the following:
	 * @li PURGE_TXABORT - Terminate outstanding overlapped writes
	 * @li PURGE_RXABORT - Terminate outstanding overlapped reads
	 * @li PURGE_TXCLEAR - Clear the transmit buffer
	 * @li PURGE_RXCLEAR - Clear the receive buffer
	 * @returns
	 * If the function is successful, the return value is nonzero.
	 * If the function is unsuccessful, the return value is zero.
	 */
	FTD2XX_API BOOL WINAPI FT_W32_PurgeComm(
		FT_HANDLE ftHandle,
		DWORD dwMask
		);

	/** @noop FT_W32_GetLastError
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Gets the last error that occurred on the device.
	 * @param ftHandle Handle of the device.
	 * @returns
	 * If the function is successful, the return value is nonzero.
	 * If the function is unsuccessful, the return value is zero.
	 * @remarks
	 * This function is normally used with overlapped I/O and so is not supported in Windows CE. For a
	 * description of its use, see FT_W32_ReadFile and FT_W32_WriteFile.
	 * @n In Linux and Mac OS X, this function returns a DWORD that directly maps to the FT Errors (for 
	 * example the FT_INVALID_HANDLE error number).
	 */
	FTD2XX_API DWORD WINAPI FT_W32_GetLastError(
		FT_HANDLE ftHandle
		);

	/** @noop FT_W32_ClearCommError
	 * @par Supported Operating Systems
	 * Linux
	 * Mac OS X (10.4 and later)
	 * Windows (2000 and later)
	 * Windows CE (4.2 and later)
	 * @par Summary
	 * Gets information about a communications error and get current status of the device.
	 * @param ftHandle Handle of the device.
	 * @param lpdwErrors Variable that contains the error mask.
	 * @param lpftComstat Pointer to FTCOMSTAT structure.
	 * @returns
	 * If the function is successful, the return value is nonzero.
	 * If the function is unsuccessful, the return value is zero.
	 */
	FTD2XX_API BOOL WINAPI FT_W32_ClearCommError(
		FT_HANDLE ftHandle,
		LPDWORD lpdwErrors,
		LPFTCOMSTAT lpftComstat
		);

	/** @noop FT_W32_CancelIo
	 * Undocumented function.
	 */
	FTD2XX_API BOOL WINAPI FT_W32_CancelIo(
		FT_HANDLE ftHandle
		);


/** @} */

/**
 * @name FT232H Additional EEPROM Functions
 */
/** @{ */

	/** @noop FT_EE_ReadConfig
	 * Undocumented function.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_EE_ReadConfig(
		FT_HANDLE ftHandle,
		UCHAR ucAddress,
		PUCHAR pucValue
		);

	/** @noop FT_EE_WriteConfig
	 * Undocumented function.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_EE_WriteConfig(
		FT_HANDLE ftHandle,
		UCHAR ucAddress,
		UCHAR ucValue
		);

	/** @noop FT_EE_ReadECC
	 * Undocumented function.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_EE_ReadECC(
		FT_HANDLE ftHandle,
		UCHAR ucOption,
		LPWORD lpwValue
		);

	/** @noop FT_GetQueueStatusEx
	 * Undocumented function.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_GetQueueStatusEx(
		FT_HANDLE ftHandle,
		DWORD *dwRxBytes
		);

	/** @noop FT_ComPortIdle
	 * Undocumented function.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_ComPortIdle(
		FT_HANDLE ftHandle
		);

	/** @noop FT_ComPortCancelIdle
	 * Undocumented function.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_ComPortCancelIdle(
		FT_HANDLE ftHandle
		);

	/** @noop FT_VendorCmdGet
	 * Undocumented function.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_VendorCmdGet(
		FT_HANDLE ftHandle,
		UCHAR Request,
		UCHAR *Buf,
		USHORT Len
		);

	/** @noop FT_VendorCmdSet
	 * Undocumented function.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_VendorCmdSet(
		FT_HANDLE ftHandle,
		UCHAR Request,
		UCHAR *Buf,
		USHORT Len
		);

	/** @noop FT_VendorCmdGetEx
	 * Undocumented function.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_VendorCmdGetEx(
		FT_HANDLE ftHandle,
		USHORT wValue,
		UCHAR *Buf,
		USHORT Len
		);

	/** @noop FT_VendorCmdSetEx
	 * Undocumented function.
	 */
	FTD2XX_API FT_STATUS WINAPI FT_VendorCmdSetEx(
		FT_HANDLE ftHandle,
		USHORT wValue,
		UCHAR *Buf,
		USHORT Len
		);

/** @} */

#ifdef __cplusplus
}
#endif


#endif	/* FTD2XX_H */
