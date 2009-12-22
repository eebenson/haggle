/* Copyright 2008-2009 Uppsala University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <libcpphaggle/Platform.h>

#if defined(OS_WINDOWS_MOBILE) && defined(ENABLE_BLUETOOTH) && defined(WIDCOMM_BLUETOOTH)

#include "ConnectivityBluetooth.h"
#include "ProtocolRFCOMM.h"
#include "WIDCOMMBluetooth.h"


int registerSDPService(CSdpService **sdp)
{
	unsigned char uuid[] = HAGGLE_BLUETOOTH_SDP_UUID;
	GUID guid;

	*sdp = new CSdpService();

	if (!*sdp)
		return -1;

	convertUUIDBytesToGUID((char *)uuid, &guid);

	(*sdp)->AddServiceClassIdList(1, &guid);
	(*sdp)->AddServiceName(L"Haggle");
	(*sdp)->AddRFCommProtocolDescriptor(RFCOMM_DEFAULT_CHANNEL);

	 // Make service browsable.
	(*sdp)->MakePublicBrowseable();

	return 0;
}

void unregisterSDPService(CSdpService **sdp)
{
	delete *sdp;
}
/*
void serviceDiscoveryCallback(struct RemoteDevice *rd, CSdpDiscoveryRec *records, int num_records, void *data)
{
	ConnectivityBluetooth *conn = static_cast<ConnectivityBluetooth *>(data);
	Address addr(AddressType_BTMAC, (unsigned char *) rd->bda);

	CM_DBG("Found Haggle Bluetooth device [%s:%s]\n", addr.getAddrStr(), rd->name.c_str());

	Interface foundInterface(IFTYPE_BLUETOOTH, rd->bda, &addr, rd->name, IFFLAG_UP);

	conn->report_interface(&foundInterface, conn->rootInterface, new ConnectivityInterfacePolicyTTL(2));
}

static void inquiryCallback(struct RemoteDevice *rd, void *data)
{
	ConnectivityBluetooth *conn = static_cast<ConnectivityBluetooth *>(data);
	unsigned char uuid[] = HAGGLE_BLUETOOTH_SDP_UUID;
	GUID guid;

	Address addr(AddressType_BTMAC, (unsigned char *) rd->bda);
	
	convertUUIDBytesToGUID((char *)uuid, &guid);

	int res = WIDCOMMBluetooth::doDiscovery(rd, &guid, serviceDiscoveryCallback, data);

	if (res > 0) {
		CM_DBG("Found Haggle Bluetooth device [%s:%s]\n", 
				addr.getAddrStr(), rd->name.c_str());
	} else {	
		CM_DBG("Bluetooth device [%s:%s] not a Haggle device\n", 
				addr.getAddrStr(), rd->name.c_str());
	}
}
*/
void bluetoothDiscovery(ConnectivityBluetooth *conn)
{
	const Address *addr = conn->rootInterface->getAddressByType(AddressType_BTMAC);
	int count = 0;

	if (!addr)
		return;

	CM_DBG("Doing inquiry on device %s - %s\n", conn->rootInterface->getName(), addr->getAddrStr());

	// Do a blocking inquiry
	int res = WIDCOMMBluetooth::doInquiry();

	if (res < 0) {
		CM_DBG("Inquiry failed... Already inquiring?\n");
		return;
	}
	
	if (!WIDCOMMBluetooth::enumerateRemoteDevicesStart()) {
		CM_DBG("Could not enumerate remote devices\n");	
		return;
	}

	CM_DBG("Inquiry done\n");

	while (true) {
		InterfaceStatus_t status;
		const RemoteDevice *rd = WIDCOMMBluetooth::getNextRemoteDevice();
		bool report_interface = false;

		if (rd == NULL)
			break;

		Address addr(AddressType_BTMAC, (unsigned char *) rd->bda);
		
		status = conn->is_known_interface(IFTYPE_BLUETOOTH, (char *)rd->bda);

		if (status == INTERFACE_STATUS_HAGGLE) {
			report_interface = true;
		} else if (status == INTERFACE_STATUS_UNKNOWN) {
			switch(ConnectivityBluetoothBase::classifyAddress(IFTYPE_BLUETOOTH, (char *)rd->bda))
			{
				case BLUETOOTH_ADDRESS_IS_UNKNOWN:
				{
					unsigned char uuid[] = HAGGLE_BLUETOOTH_SDP_UUID;
					GUID guid;
					
					convertUUIDBytesToGUID((char *)uuid, &guid);
					
					CM_DBG("Starting discovery for device %s\n", rd->name.c_str());

					int ret = WIDCOMMBluetooth::doDiscovery(rd, &guid);

					if (ret > 0) {
						report_interface = true;
						conn->report_known_interface(IFTYPE_BLUETOOTH, (char *)rd->bda, true);
					} else if (ret == 0) {
						conn->report_known_interface(IFTYPE_BLUETOOTH, (char *)rd->bda, false);
					}
				}
				break;

				case BLUETOOTH_ADDRESS_IS_HAGGLE_NODE:
					report_interface = true;
					conn->report_known_interface(IFTYPE_BLUETOOTH, (char *)rd->bda, true);
				break;
				
				case BLUETOOTH_ADDRESS_IS_NOT_HAGGLE_NODE:
					conn->report_known_interface(IFTYPE_BLUETOOTH, (char *)rd->bda, false);
				break;
			}
		} 

		if (report_interface) {
			CM_DBG("Found Haggle Bluetooth device [%s - %s]\n", 
				addr.getAddrStr(), rd->name.c_str());

			Interface foundInterface(IFTYPE_BLUETOOTH, rd->bda, &addr, rd->name, IFFLAG_UP);

			conn->report_interface(&foundInterface, conn->rootInterface, new ConnectivityInterfacePolicyTTL(2));

			count++;
		} else {	
			CM_DBG("Bluetooth device [%s - %s] not a Haggle device\n", 
				addr.getAddrStr(), rd->name.c_str());
		}
	}
	CM_DBG("Found %d Haggle devices\n", count);
}

void ConnectivityBluetooth::hookStopOrCancel()
{
	WIDCOMMBluetooth::stopInquiry();
}

void ConnectivityBluetooth::hookCleanup()
{
	unregisterSDPService(&sdp);
}

bool ConnectivityBluetooth::run()
{
	if (registerSDPService(&sdp) < 0)
		return false;

	cancelableSleep(5000);

	while (!shouldExit()) {
	
		bluetoothDiscovery(this);

		age_interfaces(rootInterface);

		cancelableSleep(TIME_TO_WAIT * 1000);
	}

	return false;
}

#endif
