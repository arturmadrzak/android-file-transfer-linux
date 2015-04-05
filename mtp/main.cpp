/*
 * Android File Transfer for Linux: MTP client for android devices
 * Copyright (C) 2015  Vladimir Menshakov

 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <stdio.h>
#include <stdexcept>

#include <usb/BulkPipe.h>
#include <usb/Context.h>
#include <mtp/ptp/Container.h>
#include <mtp/ptp/Device.h>
#include <mtp/ptp/Messages.h>

int main(int argc, char **argv)
{
	using namespace mtp;
	//USB_CALL(libusb_reset_device(device->GetHandle()));

	//printf("claiming interface %d...\n", interface->GetIndex());
	//USB_CALL(libusb_claim_interface(device->GetHandle(), interface->GetIndex()));
	//printf("claimed interface\n");
	//USB_CALL(libusb_set_interface_alt_setting(device->GetHandle(), mtp_interface, 0));

	DevicePtr mtp(Device::Find());
	if (!mtp)
	{
		printf("no mtp device found\n");
		return 1;
	}

	msg::DeviceInfo gdi = mtp->GetDeviceInfo();
	printf("%s\n", gdi.VendorExtensionDesc.c_str());
	printf("%s ", gdi.Manufactorer.c_str());
	printf("%s ", gdi.Model.c_str());
	printf("%s ", gdi.DeviceVersion.c_str());
	printf("%s\n", gdi.SerialNumber.c_str());
	printf("supported op codes: ");
	for(u16 code : gdi.OperationsSupported)
	{
		printf("%04x ", (unsigned)code);
	}
	printf("\n");
	printf("supported properties: ");
	for(u16 code : gdi.DevicePropertiesSupported)
	{
		printf("%04x ", (unsigned)code);
	}
	printf("\n");

	if (argc < 2)
		return 0;

	SessionPtr session = mtp->OpenSession(1);
	std::string command = argv[1];
	if (command == "list")
	{
		mtp::u32 parent = mtp::Session::Root;
		if (argc > 2)
			if (sscanf(argv[2], "%u", &parent) != 1)
				return 1;

		msg::ObjectHandles handles = session->GetObjectHandles(mtp::Session::AllStorages, mtp::Session::AllFormats, parent);

		for(u32 objectId : handles.ObjectHandles)
		{
			try
			{
				msg::ObjectInfo info = session->GetObjectInfo(objectId);
				printf("%-10u %04x %s %u %ux%u, %s\n", objectId, info.ObjectFormat, info.Filename.c_str(), info.ObjectCompressedSize, info.ImagePixWidth, info.ImagePixHeight, info.CaptureDate.c_str());
			}
			catch(const std::exception &ex)
			{
				printf("error: %s\n", ex.what());
			}
		}
		//libusb_release_interface(device->GetHandle(), interface->GetIndex());
	}
	else if (command == "get")
	{
		if (argc < 3)
			return 1;
		mtp::u32 objectId;
		if (sscanf(argv[2], "%u", &objectId) != 1)
			return 1;
		printf("object id = %u\n", objectId);
		msg::ObjectInfo info = session->GetObjectInfo(objectId);
		printf("filename = %s\n", info.Filename.c_str());
		FILE *f = fopen(info.Filename.c_str(), "wb");
		if (!f)
		{
			perror("open");
			return 1;
		}
		ByteArray object = session->GetObject(objectId);
		if (fwrite(object.data(), object.size(), 1, f) != 1)
			perror("fwriter");
		fclose(f);
	}
	else if (command == "set")
	{
		if (argc < 4)
			return 1;

		mtp::u32 parentObjectId;
		if (sscanf(argv[2], "%u", &parentObjectId) != 1)
			return 1;

		std::string filename(argv[3]);
		printf("uploading %s to %u\n", filename.c_str(), parentObjectId);
		FILE *f = fopen(filename.c_str(), "rb");
		if (!f)
		{
			perror("open");
			return 1;
		}
		msg::ObjectInfo oi;
		oi.Filename = filename;
		oi.ObjectFormat = ObjectFormatFromFilename(filename);

		fseek(f, 0, SEEK_END);
		oi.ObjectCompressedSize = ftell(f);
		rewind(f);

		ByteArray data(oi.ObjectCompressedSize);
		if (fread(data.data(), data.size(), 1, f) != 1)
			throw std::runtime_error("read failed");

		fclose(f);

		Session::NewObjectInfo noi = session->SendObjectInfo(oi, 0, parentObjectId);
		printf("new object id = %u\n", noi.ObjectId);
		session->SendObject(data);
		printf("done\n");
	}
	else if (command == "delete")
	{
		if (argc < 3)
			return 1;

		mtp::u32 objectId;
		if (sscanf(argv[2], "%u", &objectId) != 1)
			return 1;

		printf("object id = %u\n", objectId);
		session->DeleteObject(objectId);
	}
	else if (command == "mkdir")
	{
		if (argc < 3)
			return 1;

		mtp::u32 parentObjectId = mtp::Session::Root;
		if (argc > 3 && sscanf(argv[3], "%u", &parentObjectId) != 1)
			return 1;

		std::string filename = argv[2];
		msg::ObjectInfo oi;
		oi.Filename = filename;
		oi.ObjectFormat = ObjectFormat::Association;
		session->SendObjectInfo(oi, 0, parentObjectId);
	}
	else if (command == "properties")
	{
		if (argc < 3)
			return 1;

		mtp::u32 objectId;
		if (sscanf(argv[2], "%u", &objectId) != 1)
			return 1;
		msg::ObjectPropsSupported ops = session->GetObjectPropsSupported(objectId);
		printf("properties supported: ");
		for(u16 prop: ops.ObjectPropCodes)
		{
			printf("%02x ", prop);
		}
		printf("\n");
	}

	return 0;
}
