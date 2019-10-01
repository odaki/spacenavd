/*
spacenavd - a free software replacement driver for 6dof space-mice.
Copyright (C) 2007-2019 John Tsiombikas <nuclear@member.fsf.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#if defined(__APPLE__) && defined(__MACH__)

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDLib.h>
#include "spnavd.h"
#include "dev.h"
#include "dev_usb.h"

int open_dev_usb(struct device *dev)
{
	io_registry_entry_t entry = MACH_PORT_NULL;
	IOHIDDeviceRef  handle = NULL;

	entry = IORegistryEntryFromPath(kIOMasterPortDefault, dev->path);
	if (entry == MACH_PORT_NULL) {
		logmsg(LOG_ERR, "IORegistryEntryFromPath failed\n");
		goto error_return;
	}
	handle = IOHIDDeviceCreate(kCFAllocatorDefault, entry);
	if (handle == NULL) {
		logmsg(LOG_ERR, "IOHIDDeviceCreate failed\n");
		goto error_return;
	}

	IOReturn ret = IOHIDDeviceOpen(handle, kIOHIDOptionsTypeSeizeDevice);
	if (ret != kIOReturnSuccess) {
		logmsg(LOG_ERR, "IOHIDDeviceOpen failed\n");
		goto error_return;
	}

	IOObjectRelease(entry);
	return 0;

error_return:
	if (handle != NULL) {
		CFRelease(handle);
	}
	if (entry != MACH_PORT_NULL) {
		IOObjectRelease(entry);
	}
	return -1;
}

struct usb_device_info *find_usb_devices(int (*match)(const struct usb_device_info*))
{
	struct usb_device_info *devlist = 0;
	struct usb_device_info devinfo;
	/*static const int vendor_id = 1133;*/	/* 3dconnexion */
	static char dev_path[512];
	io_object_t dev;
	io_iterator_t iter;
	CFMutableDictionaryRef match_dict;
	/* CFNumberRef number_ref; */

	//match_dict = IOServiceMatching(kIOHIDDeviceKey);
	match_dict = IOServiceMatching(kIOUSBDeviceClassName);

	/* add filter rule: vendor-id should be 3dconnexion's */
	/*number_ref = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &vendor_id);
	CFDictionarySetValue(match_dict, CFSTR(kIOHIDVendorIDKey), number_ref);
	CFRelease(number_ref);
	*/

	/* fetch... */
	if(IOServiceGetMatchingServices(kIOMasterPortDefault, match_dict, &iter) != kIOReturnSuccess) {
		logmsg(LOG_ERR, "failed to retrieve USB HID devices\n");
		return 0;
	}

	while((dev = IOIteratorNext(iter))) {
		memset(&devinfo, 0, sizeof devinfo);

		if (IORegistryEntryGetPath(dev, kIOServicePlane, dev_path) != KERN_SUCCESS) {
			logmsg(LOG_ERR, "failed to get device path\n");
			continue;
		}
		devinfo.devfiles[0] = strdup(dev_path);
		if(devinfo.devfiles[0] == NULL) {
			logmsg(LOG_ERR, "failed to allocate device file path buffer\n");
			continue;
		}
		devinfo.num_devfiles = 1;

		/* TODO retrieve vendor id and product id */
		io_name_t deviceName;
		if (IORegistryEntryGetName(dev, deviceName) != KERN_SUCCESS) {
			logmsg(LOG_ERR, "failed to get device name\n");
			free(devinfo.devfiles[0]);
			continue;
		}
		CFStringRef deviceNameAsCFString = CFStringCreateWithCString(
																					kCFAllocatorDefault,
 																					deviceName,
																					kCFStringEncodingASCII);
		devinfo.name = strdup(CFStringGetCStringPtr(deviceNameAsCFString, kCFStringEncodingASCII));
		CFRelease(deviceNameAsCFString);

		unsigned int vendorID = 0;
		CFNumberRef vendorIDRef = IORegistryEntrySearchCFProperty(
																dev,
																kIOServicePlane,
 																CFSTR(kUSBVendorID), //CFSTR(kIOHIDVendorIDKey),
 																kCFAllocatorDefault,
 																kIORegistryIterateRecursively);
		CFNumberGetValue(vendorIDRef, kCFNumberSInt32Type, &vendorID);
		devinfo.vendorid = vendorID;
		CFRelease(vendorIDRef);

		unsigned int productID = 0;
		CFNumberRef productIDRef = IORegistryEntrySearchCFProperty(
																dev,
 																kIOServicePlane,
 																CFSTR(kUSBProductID), //CFSTR(kIOHIDProductIDKey),
 																kCFAllocatorDefault,
																kIORegistryIterateRecursively);
		CFNumberGetValue(productIDRef, kCFNumberSInt32Type, &productID);
		devinfo.productid = productID;
		CFRelease(productIDRef);

		//print_usb_device_info(&devinfo);

		if(!match || match(&devinfo)) {
			struct usb_device_info *node = malloc(sizeof *node);
			if(node) {
				if(verbose) {
					logmsg(LOG_INFO, "found usb device: ");
					print_usb_device_info(&devinfo);
				}

				*node = devinfo;
				node->next = devlist;
				devlist = node;
			} else {
				logmsg(LOG_ERR, "failed to allocate usb device info node\n");
				free(devinfo.name);
				free(devinfo.devfiles[0]);
			}
		}
	}

	IOObjectRelease(dev);
	IOObjectRelease(iter);
	return devlist;
}

#else
int spacenavd_dev_usb_darwin_silence_empty_warning;
#endif	/* __APPLE__ && __MACH__ */
