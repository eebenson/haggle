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
#ifndef _FORWARDINGMANAGER_H
#define _FORWARDINGMANAGER_H

/*
	Forward declarations of all data types declared in this file. This is to
	avoid circular dependencies. If/when a data type is added to this file,
	remember to add it here.
*/
class ForwardingManager;

#include <libcpphaggle/List.h>
#include <libcpphaggle/Pair.h>

using namespace haggle;

#include "Event.h"
#include "Manager.h"
#include "DataObject.h"
#include "Node.h"
#include "Forwarder.h"

typedef List< Pair< Pair<DataObjectRef, NodeRef>, int> > forwardingList;

/** */
class ForwardingManager : public Manager
{
	EventCallback<EventHandler> *dataObjectQueryCallback;
	EventCallback<EventHandler> *delayedDataObjectQueryCallback;
	EventCallback<EventHandler> *nodeQueryCallback;
	EventCallback<EventHandler> *forwardDobjCallback;
	EventCallback<EventHandler> *forwardRepositoryCallback;
	EventCallback<EventHandler> *forwardQueryCallback;
	EventCallback<EventHandler> *sendMetricCallback;
	
	forwardingList forwardedObjects;
	Forwarder *forwardingModule;
	EventType forwardingObjectEType;
	List<NodeRef> pendingQueryList;

        // See comment in ForwardingManager.cpp about isNeighbor()
        bool isNeighbor(NodeRef& node);
        bool addToSendList(DataObjectRef& dObj, NodeRef& node, int repeatCount = 0);
public:
	ForwardingManager(HaggleKernel *_kernel = haggleKernel);
	~ForwardingManager();
	Forwarder *getForwarder() { return forwardingModule; }
	bool shouldForward(DataObjectRef dObj, NodeRef node);
	void forwardByDelegate(DataObjectRef &dObj, NodeRef &target);
	void onShutdown();
	void onDataObjectForward(Event *e);
	void onSendDataObjectResult(Event *e);
	void onDataObjectQueryResult(Event *e);
	void onNodeQueryResult(Event *e);
	void onNodeUpdated(Event *e);
	void onNewDataObject(Event *e);
	void onForwardingDataObject(Event * e);
	void onNewNeighbor(Event *e);
	void onEndNeighbor(Event *e);
	void onForwardDobjsCallback(Event *e);
	void onForwardRepositoryCallback(Event *e);
	void onForwardQueryResult(Event *e);
	void onTargetNodes(Event *e);
	void onDelegateNodes(Event *e);
	void onDelayedDataObjectQuery(Event *e);
	void findMatchingDataObjectsAndTargets(NodeRef& node);
#ifdef DEBUG
	void onDebugCmd(Event *e);
#endif
	void onSendMetric(Event *e);
	/**
		Called by the forwarding module to alert the forwarding manager that it
		has updated the metric data object.
		
		NOTE: any forwarding module that inherits from ForwarderAsynchronous
		does not need to call this function, since ForwarderAsynchronous does 
		that.
	*/
	void sendMetric(void);
	
	class ForwardingException : public ManagerException
        {
        public:
                ForwardingException(const int err = 0, const char* data = "Forwarding manager Error") : ManagerException(err, data) {}
        };
};

#endif /* _FORWARDINGMANAGER_H */
