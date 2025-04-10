/*
 * Public libusb header file
 * Copyright © 2001 Johannes Erdfelt <johannes@erdfelt.com>
 * Copyright © 2007-2008 Daniel Drake <dsd@gentoo.org>
 * Copyright © 2012 Pete Batard <pete@akeo.ie>
 * Copyright © 2012-2023 Nathan Hjelm <hjelmn@cs.unm.edu>
 * Copyright © 2014-2020 Chris Dickens <christopher.a.dickens@gmail.com>
 * For more information, please visit: https://libusb.info
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef LIBUSB_H
	#define LIBUSB_H

	#ifdef _MSC_VER
		typedef volatile LONG usbi_atomic_t;
		#define usbi_atomic_load(a)	(*(a))
		#define usbi_atomic_store(a, v)	(*(a)) = (v)
		#define usbi_atomic_inc(a)	InterlockedIncrement((a))
		#define usbi_atomic_dec(a)	InterlockedDecrement((a))
	#else
		#if defined(__HAIKU__) && defined(__GNUC__) && !defined(__clang__)
			 /* The Haiku port of libusb has some C++ files and GCC does not define
			  * anything in stdatomic.h when compiled in C++11 (only in C++23).
			  * This appears to be a bug in gcc's stdatomic.h, and should be fixed either
			  * in gcc or in Haiku. Until then, use the gcc builtins. */
			typedef long usbi_atomic_t;
			#define usbi_atomic_load(a)    __atomic_load_n((a), __ATOMIC_SEQ_CST)
			#define usbi_atomic_store(a, v)        __atomic_store_n((a), (v), __ATOMIC_SEQ_CST)
			#define usbi_atomic_inc(a)     __atomic_add_fetch((a), 1, __ATOMIC_SEQ_CST)
			#define usbi_atomic_dec(a)     __atomic_sub_fetch((a), 1, __ATOMIC_SEQ_CST)
		#else
			#include <stdatomic.h>
			typedef atomic_long usbi_atomic_t;
			#define usbi_atomic_load(a)	atomic_load((a))
			#define usbi_atomic_store(a, v)	atomic_store((a), (v))
			#define usbi_atomic_inc(a)	(atomic_fetch_add((a), 1) + 1)
			#define usbi_atomic_dec(a)	(atomic_fetch_add((a), -1) - 1)
			#endif
	#endif

	struct libusb_device_descriptor {
		/** Size of this descriptor (in bytes) */
		uint8_t  bLength;

		/** Descriptor type. Will have value
			* \ref libusb_descriptor_type::LIBUSB_DT_DEVICE LIBUSB_DT_DEVICE in this
			* context. */
		uint8_t  bDescriptorType;

		/** USB specification release number in binary-coded decimal. A value of
			* 0x0200 indicates USB 2.0, 0x0110 indicates USB 1.1, etc. */
		uint16_t bcdUSB;

		/** USB-IF class code for the device. See \ref libusb_class_code. */
		uint8_t  bDeviceClass;

		/** USB-IF subclass code for the device, qualified by the bDeviceClass
			* value */
		uint8_t  bDeviceSubClass;

		/** USB-IF protocol code for the device, qualified by the bDeviceClass and
			* bDeviceSubClass values */
		uint8_t  bDeviceProtocol;

		/** Maximum packet size for endpoint 0 */
		uint8_t  bMaxPacketSize0;

		/** USB-IF vendor ID */
		uint16_t idVendor;

		/** USB-IF product ID */
		uint16_t idProduct;

		/** Device release number in binary-coded decimal */
		uint16_t bcdDevice;

		/** Index of string descriptor describing manufacturer */
		uint8_t  iManufacturer;

		/** Index of string descriptor describing product */
		uint8_t  iProduct;

		/** Index of string descriptor containing device serial number */
		uint8_t  iSerialNumber;

		/** Number of possible configurations */
		uint8_t  bNumConfigurations;
	};

	struct list_head {
		struct list_head* prev, * next;
	};

	struct libusb_device {
		usbi_atomic_t refcnt;

		struct libusb_context* ctx;
		struct libusb_device* parent_dev;

		uint8_t bus_number;
		uint8_t port_number;
		uint8_t device_address;
		enum libusb_speed speed;

		struct list_head list;
		unsigned long session_data;

		struct libusb_device_descriptor device_descriptor;
		usbi_atomic_t attached;
	};


#endif
