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
#include <string.h>

#include <libcpphaggle/Platform.h>
#include <haggleutils.h>

#include "ProtocolTCP.h"
#include "ProtocolManager.h"
#include "Interface.h"

#if defined(ENABLE_IPv6)
#define SOCKADDR_SIZE sizeof(struct sockaddr_in6)
#else
#define SOCKADDR_SIZE sizeof(struct sockaddr_in)
#endif

ProtocolTCP::ProtocolTCP(SOCKET _sock, const struct sockaddr *peer_addr, const InterfaceRef& _localIface, const short flags, ProtocolManager * m) :
	ProtocolSocket(PROT_TYPE_TCP, "ProtocolTCP", _localIface, NULL, flags, m, _sock), localport(0)
{
	unsigned char *rawaddr = NULL;
	socklen_t addrlen = 0;
	AddressType_t atype = AddressType_EthMAC;

#if defined(ENABLE_IPv6)
	if (peer_addr->sa_family == AF_INET6) {
		struct sockaddr_in6 *saddr_in6 = (struct sockaddr_in6 *)peer_addr;
		addrlen = sizeof(struct sockaddr_in6);
		atype = AddressType_IPv6;
		rawaddr = (unsigned char *)&saddr_in6->sin6_addr;
		localport = ntohs(saddr_in6->sin6_port);
	}
#endif
	if (peer_addr->sa_family == AF_INET) {
		struct sockaddr_in *saddr_in = (struct sockaddr_in *)peer_addr;
		addrlen = sizeof(struct sockaddr_in);
		atype = AddressType_IPv4;
		rawaddr = (unsigned char *)&saddr_in->sin_addr;
		localport = ntohs(saddr_in->sin_port);
	}

	Address addr(atype, rawaddr, NULL, ProtocolSpecType_TCP, localport);

	setPeerInterface(&addr);
}

ProtocolTCP::ProtocolTCP(const InterfaceRef& _localIface, const InterfaceRef& _peerIface, const unsigned short _port, const short flags, ProtocolManager * m) : 
	ProtocolSocket(PROT_TYPE_TCP, "ProtocolTCP", _localIface, _peerIface, flags, m), localport(_port)
{
}

ProtocolTCP::~ProtocolTCP()
{
}

bool ProtocolTCP::initbase()
{
	int optval = 1;
        char buf[SOCKADDR_SIZE];
        struct sockaddr *local_addr = (struct sockaddr *)buf;
        socklen_t addrlen = 0;
        int af = AF_INET;
        unsigned short port = isClient() ? 0 : localport;

        if (!localIface) {
		HAGGLE_ERR("Local interface is NULL\n");
                return false;
        }

	// Check if we are already connected, i.e., we are a client
	// that was created from acceptClient()
	if (isConnected()) {
		// Nothing to initialize
		return true;
	}
        // Figure out the address type based on the local interface
#if defined(ENABLE_IPv6)
	if (localIface->getAddressByType(AddressType_IPv6))
                af = AF_INET6;
#endif

        /* Configure a sockaddr for binding to the given port. Do not
         * bind to a specific interface. */
        if (af == AF_INET) {
                struct sockaddr_in *sa = (struct sockaddr_in *)local_addr;
                sa->sin_family = AF_INET;
                sa->sin_addr.s_addr = htonl(INADDR_ANY);
                sa->sin_port = htons(port);
                addrlen = sizeof(struct sockaddr_in);
        }
#if defined(ENABLE_IPv6)
        else if (af == AF_INET6) {
                struct sockaddr_in6 *sa = (struct sockaddr_in6 *)local_addr;
                sa->sin6_family = AF_INET6;
                sa->sin6_len = SOCKADDR_SIZE;
                sa->sin6_addr = in6addr_any;
                sa->sin6_port = htons(port);
                addrlen = sizeof(struct sockaddr_in6);
        }
#endif

	if (!openSocket(local_addr->sa_family, SOCK_STREAM, IPPROTO_TCP, isServer())) {
		HAGGLE_ERR("Could not create TCP socket\n");
                return false;
	}

	if (!setSocketOption(SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))) {
		closeSocket();
		HAGGLE_ERR("setsockopt SO_REUSEADDR failed\n");
                return false;
	}

	if (!setSocketOption(SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval))) {
		closeSocket();
		HAGGLE_ERR("setsockopt SO_KEEPALIVE failed\n");
                return false;
	}

	if (!bindSocket(local_addr, addrlen)) {
		closeSocket();
		HAGGLE_ERR("Could not bind TCP socket\n");
                return false;
        }

        if (af == AF_INET) {
                HAGGLE_DBG("%s Created TCP socket - %s:%d\n", 
			getName(), ip_to_str(((struct sockaddr_in *)local_addr)->sin_addr), port);
        }

	return true;
}

void ProtocolTCP::setPeerInterface(const Address *addr)
{
	int res;
        InterfaceRef pIface = NULL;
	
        if (!addr)
                return;

        pIface = getKernel()->getInterfaceStore()->retrieve(*addr);

	if (pIface) {
		HAGGLE_DBG("Peer interface is [%s]\n", pIface->getIdentifierStr());
	} else {
                char buf[SOCKADDR_SIZE];
                unsigned char mac[6];
                struct sockaddr *peer_addr = (struct sockaddr *)buf;
                addr->fillInSockaddr(peer_addr);
                
                HAGGLE_DBG("trying to figure out peer mac for IP %s on interface %s\n", addr->getAddrStr(), localIface->getName());

                res = get_peer_mac_address(peer_addr, localIface->getName(), mac, 6);

		if (res < 0) {
			HAGGLE_ERR("Error when retreiving mac address for peer %s, error=%d\n", addr->getAddrStr(), res);
		} else if (res == 0) {
			HAGGLE_ERR("No corresponding mac address for peer %s\n", addr->getAddrStr());
		} else {
			Address addr2(AddressType_EthMAC, mac);
			pIface = new Interface(localIface->getType(), mac, addr, "TCP peer", IFFLAG_UP);
			pIface->addAddress(&addr2);
			HAGGLE_DBG("Peer interface is [%s]\n", pIface->getIdentifierStr());
		}
	}
	peerIface = pIface;
}

bool ProtocolTCPClient::init()
{
	if (!peerIface) {
		HAGGLE_ERR("Client has no peer interface\n");
		return false;
	}
	return initbase();
}

ProtocolEvent ProtocolTCPClient::connectToPeer()
{
	socklen_t addrlen;
        char buf[SOCKADDR_SIZE];
        struct sockaddr *peer_addr = (struct sockaddr *)buf;
	unsigned short peerPort;
	Address *addr = NULL;
	InterfaceRef pIface;
	
	if (!peerIface ||
		!(peerIface->getAddressByType(AddressType_IPv4) || 
#if defined(ENABLE_IPv6)
		  peerIface->getAddressByType(AddressType_IPv6)
#else
		  0
#endif
		  ))
		return PROT_EVENT_ERROR;

#if defined(ENABLE_IPv6)
	addr = peerIface->getAddressByType(AddressType_IPv6);
#endif
	
	if (addr == NULL) {
		// Since the check above passed, we know there has to be an IPv4 or an 
		// IPv6 address associated with the interface, and since there was no IPv6...
		addr = peerIface->getAddressByType(AddressType_IPv4);
	}

	if (addr == NULL) {
		HAGGLE_DBG("No IP address to connect to\n");
		return PROT_EVENT_ERROR;
	}
	
        // FIXME: use other port than the default one?
        peerPort = TCP_DEFAULT_PORT; //addr->getProtocolPortOrChannel();

        addrlen = addr->fillInSockaddr(peer_addr, peerPort);

	ProtocolEvent ret = openConnection(peer_addr, addrlen);

	if (ret != PROT_EVENT_SUCCESS) {
		HAGGLE_DBG("%s Connection failed to [%s] tcp port=%u\n", 
			getName(), addr->getAddrStr(), peerPort);
		return ret;
	}

	HAGGLE_DBG("%s Connected to [%s] tcp port=%u\n", getName(), addr->getAddrStr(), peerPort);

	return ret;
}

ProtocolTCPServer::ProtocolTCPServer(const InterfaceRef& _localIface, ProtocolManager *m, const unsigned short _port, int _backlog) :
	ProtocolTCP(_localIface, NULL, _port, PROT_FLAG_SERVER, m), backlog(_backlog) 
{
}

ProtocolTCPServer::~ProtocolTCPServer()
{
}


bool ProtocolTCPServer::init()
{
	if (!initbase())
		return false;

	if (!setListen(backlog))  {
		HAGGLE_ERR("Could not set listen mode on socket\n");
        }
	return true;
}

ProtocolEvent ProtocolTCPServer::acceptClient()
{
        char buf[SOCKADDR_SIZE];
        struct sockaddr *peer_addr = (struct sockaddr *)buf;
	socklen_t len;
	SOCKET clientsock;
	ProtocolTCPClient *p = NULL;

	HAGGLE_DBG("In TCPServer receive\n");

	if (getMode() != PROT_MODE_LISTENING) {
		HAGGLE_DBG("Error: TCPServer not in LISTEN mode\n");
		return PROT_EVENT_ERROR;
	}

	len = SOCKADDR_SIZE;

	clientsock = acceptOnSocket((struct sockaddr *) peer_addr, &len);

	if (clientsock == SOCKET_ERROR)
		return PROT_EVENT_ERROR;

	if (!getManager()) {
		HAGGLE_DBG("Error: No manager for protocol!\n");
		CLOSE_SOCKET(clientsock);
		return PROT_EVENT_ERROR;
	}
	
        p = new ProtocolTCPReceiver(clientsock, (struct sockaddr *)peer_addr, this->getLocalInterface(), getManager());

	if (!p || !p->init()) {
		HAGGLE_DBG("Unable to create new TCP client on socket %d\n", clientsock);
		CLOSE_SOCKET(clientsock);
		return PROT_EVENT_ERROR;
	}

	p->registerWithManager();

	HAGGLE_DBG("Accepted client with socket %d, starting client thread\n", clientsock);

	return p->startTxRx();
}
