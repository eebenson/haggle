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

#if defined(ENABLE_BLUETOOTH)

#include "ConnectivityBluetooth.h"

ConnectivityBluetooth::ConnectivityBluetooth(ConnectivityManager *m, const InterfaceRef& _iface) :
	Connectivity(m, "Bluetooth connectivity"),
	rootInterface(_iface)
{
	LOG_ADD("%s: Bluetooth connectivity starting. Scan time: %ld +- %ld seconds\n",
		Timeval::now().getAsString().c_str(),
		BASE_TIME_BETWEEN_SCANS,
		RANDOM_TIME_AMOUNT);
}

ConnectivityBluetooth::~ConnectivityBluetooth()
{
	LOG_ADD("%s: Bluetooth connectivity stopped.\n",
		Timeval::now().getAsString().c_str());
}

void ConnectivityBluetooth::handleInterfaceDown(const InterfaceRef &iface)
{
	if (iface == rootInterface)
		cancelDiscovery();
}

#endif
