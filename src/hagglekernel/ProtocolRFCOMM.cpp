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

#if defined(ENABLE_BLUETOOTH) && !defined(OS_MACOSX) && !defined(WIDCOMM_BLUETOOTH)

#include <string.h>
#include <haggleutils.h>

#include "ProtocolManager.h"
#include "ProtocolRFCOMM.h"
#include "HaggleKernel.h"

ProtocolRFCOMMClient::ProtocolRFCOMMClient(const InterfaceRef& _localIface, const InterfaceRef& _peerIface, const unsigned short channel, ProtocolManager *m) :
		ProtocolRFCOMM(_localIface, _peerIface, channel, PROT_FLAG_CLIENT, m)
{
	const Address *addr = peerIface->getAddressByType(AddressType_BTMAC);

	if (!addr) {
#if HAVE_EXCEPTION
		throw ProtocolException(-1, "No Bluetooth MAC address in peer interface");
#else
                return;
#endif
        }
	memcpy(mac, addr->getRaw(), addr->getLength());
}

ProtocolRFCOMM::ProtocolRFCOMM(SOCKET _sock, const char *_mac, const unsigned short _channel, const InterfaceRef& _localIface, const short flags, ProtocolManager *m) :
	ProtocolSocket(PROT_TYPE_RFCOMM, "ProtocolRFCOMM", _localIface, NULL, flags, m, _sock), channel(_channel)
{
	memcpy(mac, _mac, BT_ALEN);
	
	Address addr(AddressType_BTMAC, (unsigned char *) mac);

	peerIface = InterfaceRef(new Interface(IFTYPE_BLUETOOTH, mac, &addr, "Peer Bluetooth", IFFLAG_UP), "InterfacePeerRFCOMM");
}

ProtocolRFCOMM::ProtocolRFCOMM(const InterfaceRef& _localIface, const InterfaceRef& _peerIface, const unsigned short _channel, const short flags, ProtocolManager * m) : 
	ProtocolSocket(PROT_TYPE_RFCOMM, "ProtocolRFCOMM", _localIface,
		       _peerIface, flags, m), channel(_channel)
{
	struct sockaddr_bt localAddr;

	if (!openSocket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM, isServer())) {
#if HAVE_EXCEPTION
		throw SocketException(0, "Could not create RFCOMM socket");
#else
                return;
#endif
        }
	memset(&localAddr, 0, sizeof(struct sockaddr_bt));
	localAddr.bt_family = AF_BLUETOOTH;
	localAddr.bt_channel = _channel & 0xff;

#ifndef OS_WINDOWS
	if (localIface) {
		Address *addr = localIface->getAddressByType(AddressType_BTMAC);
		if (addr != NULL) {
                        // Binding RFCOMM sockets to a hardware address does not
                        // seem to work in Windows
			BDADDR_swap(&localAddr.bt_bdaddr, addr->getRaw());
                }
	}
#endif

	if (isServer()) {
		localAddr.bt_channel = channel & 0xff;
	} else {
		HAGGLE_DBG("Created RFCOMM client on channel=%d\n", channel);
		return;
	}
	/* If this is a server we bing to a specific channel to listen on */

	if (!bindSocket((struct sockaddr *)&localAddr, sizeof(localAddr))) {
		closeSocket();
#ifdef OS_WINDOWS
		HAGGLE_DBG("Bind failed for local address WSA error=%s\n", StrError(WSAGetLastError()));
#endif
#if HAVE_EXCEPTION
		throw BindException(0, "Could not bind local address for RFCOMM socket");
#endif
	} else {
                HAGGLE_DBG("Bound RFCOMM server to channel=%d\n", channel);
        }
}

ProtocolRFCOMM::~ProtocolRFCOMM()
{
}

ProtocolEvent ProtocolRFCOMMClient::connectToPeer()
{
	struct sockaddr_bt peerAddr;
	Address	*addr;
	
	if (!peerIface)
		return PROT_EVENT_ERROR;

	addr = peerIface->getAddressByType(AddressType_BTMAC);

	if(!addr)
		return PROT_EVENT_ERROR;

	memset(&peerAddr, 0, sizeof(peerAddr));
	peerAddr.bt_family = AF_BLUETOOTH;

	BDADDR_swap(&peerAddr.bt_bdaddr, mac);
	peerAddr.bt_channel = channel & 0xff;

	HAGGLE_DBG("%s Trying to connect over RFCOMM to [%s] channel=%u\n", 
		   getName(), addr->getAddrStr(), channel);

	ProtocolEvent ret = openConnection((struct sockaddr *) &peerAddr, sizeof(peerAddr));

	if (ret != PROT_EVENT_SUCCESS) {
		HAGGLE_DBG("%s Connection failed to [%s] channel=%u\n", 
			   getName(), addr->getAddrStr(), channel);
		return ret;
	}

	HAGGLE_DBG("%s Connected to [%s] channel=%u\n", 
		   getName(), addr->getAddrStr(), channel);

	return ret;
}


ProtocolRFCOMMClient::~ProtocolRFCOMMClient()
{
	HAGGLE_DBG("Destroying %s\n", getName());
}

ProtocolRFCOMMServer::ProtocolRFCOMMServer(const InterfaceRef& localIface, ProtocolManager *m,
					   unsigned short channel, int _backlog) :
	ProtocolRFCOMM(localIface, NULL, channel, PROT_FLAG_SERVER, m), backlog(_backlog) 
{
	if (!setListen(backlog)) {
#if HAVE_EXCEPTION
		throw ListenException();
#endif
        }
}

ProtocolRFCOMMServer::~ProtocolRFCOMMServer()
{
	HAGGLE_DBG("Destroying %s\n", getName());
}

ProtocolEvent ProtocolRFCOMMServer::acceptClient()
{
	SOCKET clientSock;
	struct sockaddr_bt clientAddr;
	socklen_t len;
	char btMac[BT_ALEN];

	HAGGLE_DBG("In RFCOMMServer receive\n");

	if (getMode() != PROT_MODE_LISTENING) {
		HAGGLE_DBG("Error: RFCOMMServer not in LISTEN mode\n");
		return PROT_EVENT_ERROR;
	}

	len = sizeof(clientAddr);

	clientSock = acceptOnSocket((struct sockaddr *) &clientAddr, &len);

	if (clientSock == INVALID_SOCKET)
		return PROT_EVENT_ERROR;

	/* Save address and channel information in client handle. */
	ProtocolManager *pm = static_cast<ProtocolManager *>(getManager());

	if (!pm) {
		HAGGLE_DBG("Error: No manager for protocol!\n");
		CLOSE_SOCKET(clientSock);
		return PROT_EVENT_ERROR;
	}

	BDADDR_swap(btMac, &clientAddr.bt_bdaddr);

	ProtocolRFCOMMClient *p = new ProtocolRFCOMMReceiver(clientSock, btMac,
							   (unsigned short) clientAddr.bt_channel,
							   this->getLocalInterface(), pm);

	p->setFlag(PROT_FLAG_CONNECTED);

	p->registerWithManager();

	HAGGLE_DBG("Accepted client with socket %d, starting client thread\n", clientSock);

	return p->startTxRx();
}

#endif /* ENABLE_BLUETOOTH && !OS_MACOSX */
