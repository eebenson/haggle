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
#include <stdlib.h>

#include <libcpphaggle/Platform.h>
#include <haggleutils.h>

#include "Event.h"
#include "DataObject.h"
#include "Interface.h"

const char *Event::eventNames[MAX_NUM_EVENT_TYPES] = {
	"EVENT_TYPE_PREPARE_STARTUP",
	"EVENT_TYPE_STARTUP",
	"EVENT_TYPE_PREPARE_SHUTDOWN",
	"EVENT_TYPE_SHUTDOWN",	
	"EVENT_TYPE_NODE_CONTACT_NEW",
	"EVENT_TYPE_NODE_CONTACT_END",
	"EVENT_TYPE_NODE_UPDATED",
	"EVENT_TYPE_NODE_DESCRIPTION_SEND",
	"EVENT_TYPE_NEIGHBOR_INTERFACE_UP",
	"EVENT_TYPE_NEIGHBOR_INTERFACE_DOWN",
	"EVENT_TYPE_LOCAL_INTERFACE_UP",
	"EVENT_TYPE_LOCAL_INTERFACE_DOWN",
	"EVENT_TYPE_DATAOBJECT_NEW",
	"EVENT_TYPE_DATAOBJECT_DELETED",
	"EVENT_TYPE_DATAOBJECT_FORWARD",
	"EVENT_TYPE_DATAOBJECT_SEND",
	"EVENT_TYPE_DATAOBJECT_VERIFIED",
	"EVENT_TYPE_DATAOBJECT_RECEIVED",
	"EVENT_TYPE_DATAOBJECT_SEND_SUCCESSFUL",
	"EVENT_TYPE_DATAOBJECT_SEND_FAILURE",
	"EVENT_TYPE_DATAOBJECT_INCOMING",
	"EVENT_TYPE_TARGET_NODES",
	"EVENT_TYPE_DELEGATE_NODES",
	"EVENT_TYPE_RESOURCE_POLICY_NEW",
#ifdef DEBUG
	"EVENT_TYPE_DEBUG_CMD",
#endif
	"EVENT_TYPE_CALLBACK",
};

int Event::num_event_types = MAX_NUM_PUBLIC_EVENT_TYPES;

// This will store the callback functions for registered private events
EventCallback < EventHandler > *Event::privCallbacks[MAX_NUM_PRIVATE_EVENT_TYPES];

Event::Event(EventType _type, const DataObjectRef& _dObjRef, double _delay) : 
#ifdef DEBUG_LEAKS
	LeakMonitor(LEAK_TYPE_EVENT),
#endif
	HeapItem(),
	type(_type),
	timeout(absolute_time_double(_delay)), 
	dObjRef(_dObjRef),
	data(NULL),
	doesHaveData(_dObjRef)
{
	if (!EVENT_TYPE(type)) {
		HAGGLE_DBG("ERROR: trying to allocate an invalid event type!\n");
#if HAVE_EXCEPTION
		throw EventException(type, "Unknown event type");
#else
                return;
#endif
	}
	if (dObjRef) {
		if (EVENT_TYPE_PRIVATE(type) || 
		    type == EVENT_TYPE_DATAOBJECT_RECEIVED || 
		    type == EVENT_TYPE_DATAOBJECT_VERIFIED || 
		    type == EVENT_TYPE_DATAOBJECT_NEW || 
		    type == EVENT_TYPE_DATAOBJECT_INCOMING) {
		} else if (type == EVENT_TYPE_DATAOBJECT_DELETED) {
			// For simplicity's sake, we allow a deleted with just one data 
			// object, but to do that, we need to add it to the data object 
			// list (because the recipient of the event can't know how the 
			// event was created).
			dObjs.push_back(dObjRef);
			// Clear the dObjRef, so this event looks just like if it was 
			// created with the (..., DataObjectRefList, ...) constructor
			dObjRef = NULL;
		} else {
			HAGGLE_DBG("ERROR: Event type %s does not accept a data object as data!\n",
				eventNames[type]);
#if HAVE_EXCEPTION
			throw EventException(type, "Event type %s does not accept a data object as data!");
#endif
		}
	}
	// Filters can have data and they are private
	/*if (EVENT_TYPE_PRIVATE(type) && data) {
	   HAGGLE_DBG("Private events currently cannot have data\n");
	   throw EventException(type, "Private event type with data - Illegal!");
	   } */
}

Event::Event(EventType _type, const InterfaceRef& _ifaceRef, double _delay) : 
#ifdef DEBUG_LEAKS
	LeakMonitor(LEAK_TYPE_EVENT),
#endif
	HeapItem(),
	type(_type),
	timeout(absolute_time_double(_delay)), 
	ifaceRef(_ifaceRef),
	data(NULL),
	doesHaveData(_ifaceRef)
{
	if (!EVENT_TYPE(type)) {
#if HAVE_EXCEPTION
		throw EventException(type, "Unknown event type");
#else
                return;
#endif
        }

	if (ifaceRef) {
		if (EVENT_TYPE_PRIVATE(type) || 
		    type == EVENT_TYPE_LOCAL_INTERFACE_UP || 
		    type == EVENT_TYPE_LOCAL_INTERFACE_DOWN || 
		    type == EVENT_TYPE_NEIGHBOR_INTERFACE_UP || 
		    type == EVENT_TYPE_NEIGHBOR_INTERFACE_DOWN) {
		} else {
			HAGGLE_DBG("ERROR: Event type %s does not accept an interface as data!\n",
				eventNames[type]);
#if HAVE_EXCEPTION
			throw EventException(type, "Event type %s does not accept an interface as data!");
#endif
		}
	}
	// Filters can have data and they are private
	/*if (EVENT_TYPE_PRIVATE(type) && data) {
	   HAGGLE_DBG("Private events currently cannot have data\n");
	   throw EventException(type, "Private event type with data - Illegal!");
	   } */
}

Event::Event(EventType _type, const NodeRef& _nodeRef, double _delay) : 
#ifdef DEBUG_LEAKS
	LeakMonitor(LEAK_TYPE_EVENT),
#endif
	HeapItem(),
	type(_type),
	timeout(absolute_time_double(_delay)), 
	nodeRef(_nodeRef),
	data(NULL),
	doesHaveData(_nodeRef)
{
	if (!EVENT_TYPE(type)) {
#if HAVE_EXCEPTION
		throw EventException(type, "Unknown event type");
#else
                return;
#endif
        }
	if (nodeRef) {
		if (EVENT_TYPE_PRIVATE(type) || 
		    type == EVENT_TYPE_NODE_CONTACT_NEW || 
		    type == EVENT_TYPE_NODE_CONTACT_END) {
		} else {
			HAGGLE_DBG("ERROR: Event type %s does not accept a node as data!\n",
				eventNames[type]);
#if HAVE_EXCEPTION
			throw EventException(type, "Event type %s does not accept a node as data!");
#endif
		}
	}
	// Filters can have data and they are private
	/*if (EVENT_TYPE_PRIVATE(type) && data) {
	   HAGGLE_DBG("Private events currently cannot have data\n");
	   throw EventException(type, "Private event type with data - Illegal!");
	   } */
}
Event::Event(EventType _type, const PolicyRef& _policyRef, double _delay) : 
#ifdef DEBUG_LEAKS
	LeakMonitor(LEAK_TYPE_EVENT),
#endif
	HeapItem(),
	type(_type),
	timeout(absolute_time_double(_delay)), 
	policyRef(_policyRef),
	data(NULL),
	doesHaveData(_policyRef)
{
	if (!EVENT_TYPE(type)) {
#if HAVE_EXCEPTION
		throw EventException(type, "Unknown event type");
#else
                return;
#endif
        }

	if (policyRef) {
		if (EVENT_TYPE_PRIVATE(type) || 
		    type == EVENT_TYPE_RESOURCE_POLICY_NEW) {
		} else {
			HAGGLE_DBG("ERROR: Event type %s does not accept a policy as data!\n",
				eventNames[type]);
#if HAVE_EXCEPTION
			throw EventException(type, "Event type %s does not accept a policy as data!");
#endif
		}
	}
}

Event::Event(EventType _type, const DataObjectRef&  _dObjRef, const NodeRef& _nodeRef, double _delay) :
#ifdef DEBUG_LEAKS
	LeakMonitor(LEAK_TYPE_EVENT),
#endif
	HeapItem(),
	type(_type),
	timeout(absolute_time_double(_delay)), 
	dObjRef(_dObjRef),
	nodeRef(_nodeRef),
	data(NULL),
	doesHaveData(_dObjRef && _nodeRef)
{
	if (!EVENT_TYPE(type)) {
#if HAVE_EXCEPTION
		throw EventException(type, "Unknown event type");
#else
                return;
#endif
        }
	if (doesHaveData) {
		if (EVENT_TYPE_PRIVATE(type) || 
		    type == EVENT_TYPE_DATAOBJECT_FORWARD || 
		    type == EVENT_TYPE_DATAOBJECT_SEND_SUCCESSFUL || 
		    type == EVENT_TYPE_DATAOBJECT_SEND_FAILURE) {
			
		} else if (
			type == EVENT_TYPE_DATAOBJECT_SEND){
			// For simplicity's sake, we allow a send with just a target node,
			// but to do that, we need to add it to the node list (because the
			// Recipient of the event can't know how the event was created).
			nodes.push_front(nodeRef);
			// Clear the nodeRef, so this event looks just like if it was 
			// created with the (..., DataObject, NodeRefList, ...) constructor
			nodeRef = NULL;
		} else {
			HAGGLE_DBG("ERROR: Event type %s does not accept a data object and a node as data!\n",
				eventNames[type]);
#if HAVE_EXCEPTION
			throw EventException(type, "Event type %s does not accept a data object and a node as data!");
#endif
		}
	}
}

#ifdef DEBUG

Event::Event(const DebugCmdRef& _dbgCmdRef, double _delay) :
#ifdef DEBUG_LEAKS
	LeakMonitor(LEAK_TYPE_EVENT),
#endif 
	HeapItem(),
	type(EVENT_TYPE_DEBUG_CMD),
	timeout(absolute_time_double(_delay)), 
	dbgCmdRef(_dbgCmdRef),
	data(NULL),
	doesHaveData(_dbgCmdRef)
{
	if (!EVENT_TYPE(type)) {
#if HAVE_EXCEPTION
		throw EventException(type, "Unknown event type");
#else
                return;
#endif
        }

	if (dbgCmdRef) {
		if (EVENT_TYPE_PRIVATE(type) || 
		    type == EVENT_TYPE_DEBUG_CMD) {
		} else {
			HAGGLE_DBG("ERROR: Event type %s does not accept a debug command as data!\n",eventNames[type]);
#if HAVE_EXCEPTION
			throw EventException(type, "Event type %s does not accept a debug command as data!");
#endif
		}
	}
}
        
#endif /* DEBUG */

Event::Event(EventType _type, const NodeRef& _nodeRef, const NodeRefList& _nodes, double _delay) : 
#ifdef DEBUG_LEAKS
	LeakMonitor(LEAK_TYPE_EVENT),
#endif
	HeapItem(),
	type(_type),
	timeout(absolute_time_double(_delay)), 
	nodeRef(_nodeRef),
	nodes(_nodes),
	data(NULL),
	doesHaveData(_nodeRef)
{
	if (!EVENT_TYPE(type)) {
#if HAVE_EXCEPTION
		throw EventException(type, "Unknown event type");
#else
                return;
#endif
        }
	if (nodeRef && !(nodes.empty())) {
		if (EVENT_TYPE_PRIVATE(type) || 
		    type == EVENT_TYPE_TARGET_NODES || 
		    type == EVENT_TYPE_NODE_UPDATED) {
		} else {
			HAGGLE_DBG("ERROR: Event type %s does not accept a node and a node list as data!\n",
				eventNames[type]);
#if HAVE_EXCEPTION
			throw EventException(type, "Event type %s does not accept a node and a node list as data!");
#endif
		}
	}
}

Event::Event(EventType _type, const DataObjectRef& _dObjRef, const NodeRefList& _nodes, double _delay) : 
#ifdef DEBUG_LEAKS
	LeakMonitor(LEAK_TYPE_EVENT),
#endif
	HeapItem(),
	type(_type),
	timeout(absolute_time_double(_delay)), 
	dObjRef(_dObjRef),
	nodes(_nodes),
	data(NULL),
	doesHaveData(_dObjRef && !(_nodes.empty()))
{
	if (!EVENT_TYPE(type)) {
#if HAVE_EXCEPTION
		throw EventException(type, "Unknown event type");
#else
                return;
#endif
        }
	if (dObjRef && !(nodes.empty())) {
		if (EVENT_TYPE_PRIVATE(type) ||
			type == EVENT_TYPE_DATAOBJECT_SEND) {
		} else {
			HAGGLE_DBG("ERROR: Event type %s does not accept a data object and a node list as data!\n",
				eventNames[type]);
#if HAVE_EXCEPTION
			throw EventException(type, "Event type %s does not accept a data object and a node list as data!");
#endif
		}
	}
}

Event::Event(EventType _type, const DataObjectRef& _dObjRef, const NodeRef& _nodeRef, const NodeRefList& _nodes, double _delay) : 
#ifdef DEBUG_LEAKS
	LeakMonitor(LEAK_TYPE_EVENT),
#endif
	HeapItem(),
	type(_type),
	timeout(absolute_time_double(_delay)), 
	dObjRef(_dObjRef),
	nodeRef(_nodeRef),
	nodes(_nodes),
	data(NULL),
	doesHaveData(_dObjRef && !(_nodes.empty()))
{
	if (!EVENT_TYPE(type)) {
#if HAVE_EXCEPTION
		throw EventException(type, "Unknown event type");
#else
                return;
#endif
        }
	if (dObjRef && !(nodes.empty())) {
		if (EVENT_TYPE_PRIVATE(type) || 
		    type == EVENT_TYPE_DELEGATE_NODES) {
		} else {
			HAGGLE_DBG("ERROR: Event type %s does not accept a data object, a node and a node list as data!\n",
				eventNames[type]);
#if HAVE_EXCEPTION
			throw EventException(type, "Event type %s does not accept a data object, a node and a node list as data!");
#endif
		}
	}
}

Event::Event(EventType _type, const DataObjectRefList& _dObjs, double _delay) : 
#ifdef DEBUG_LEAKS
	LeakMonitor(LEAK_TYPE_EVENT),
#endif
	HeapItem(),
	type(_type),
	timeout(absolute_time_double(_delay)), 
	dObjs(_dObjs),
	data(NULL),
	doesHaveData(_dObjs.size() > 0)
{
	if (!EVENT_TYPE(type)) {
		HAGGLE_DBG("ERROR: trying to allocate an invalid event type!\n");
#if HAVE_EXCEPTION
		throw EventException(type, "Unknown event type");
#else
                return;
#endif
	}
	if (dObjRef) {
		if (EVENT_TYPE_PRIVATE(type) || 
		    type == EVENT_TYPE_DATAOBJECT_DELETED) {
		} else {
			HAGGLE_DBG("ERROR: Event type %s does not accept a list of data objects as data!\n",
				eventNames[type]);
#if HAVE_EXCEPTION
			throw EventException(type, "Event type %s does not accept a list of data objects as data!");
#endif
		}
	}
}

Event::Event(EventType _type, void *_data, double _delay) : 
#ifdef DEBUG_LEAKS
	LeakMonitor(LEAK_TYPE_EVENT),
#endif
	HeapItem(),
	type(_type),
	timeout(absolute_time_double(_delay)), 
	data(_data),
	doesHaveData(_data != NULL)
{
	if (!EVENT_TYPE(type)) {
#if HAVE_EXCEPTION
		throw EventException(type, "Unknown event type");
#else
                return;
#endif
        }

	if (data != NULL) {
		if (EVENT_TYPE_PRIVATE(type) || 
		    type == EVENT_TYPE_PREPARE_STARTUP || 
		    type == EVENT_TYPE_STARTUP || 
		    type == EVENT_TYPE_PREPARE_SHUTDOWN || 
		    type == EVENT_TYPE_SHUTDOWN) {
		} else {
			switch(type)
			{
				case EVENT_TYPE_DATAOBJECT_RECEIVED:
				case EVENT_TYPE_DATAOBJECT_VERIFIED:
				case EVENT_TYPE_DATAOBJECT_NEW:
				case EVENT_TYPE_DATAOBJECT_DELETED:
				case EVENT_TYPE_DATAOBJECT_INCOMING:
					HAGGLE_DBG(
						"ERROR: Event type %s does not accept void * as data - "
						"only a data object!\n",
						eventNames[type]);
#if HAVE_EXCEPTION
					throw 
						EventException(
							type, 
							"Event type %s does not accept void * as data - "
							"only a data object!");
#endif
				break;
				
				case EVENT_TYPE_DATAOBJECT_FORWARD:
				case EVENT_TYPE_DATAOBJECT_SEND_SUCCESSFUL:
				case EVENT_TYPE_DATAOBJECT_SEND_FAILURE:
					HAGGLE_DBG(
						"ERROR: Event type %s does not accept void * as data - "
						"only a data object and a node!\n",
						eventNames[type]);
#if HAVE_EXCEPTION
					throw 
						EventException(
							type, 
							"Event type %s does not accept void * as data - "
							"only a data object and a node!");
#endif
				break;
				
				case EVENT_TYPE_TARGET_NODES:
				case EVENT_TYPE_NODE_UPDATED:
					HAGGLE_DBG(
						"ERROR: Event type %s does not accept void * as data - "
						"only a node and a node list!\n",
						eventNames[type]);
#if HAVE_EXCEPTION
					throw 
						EventException(
							type, 
							"Event type %s does not accept void * as data - "
							"only a node and a node list!");
#endif
				break;
				
				case EVENT_TYPE_DATAOBJECT_SEND:
					HAGGLE_DBG(
						"ERROR: Event type %s does not accept void * as data - "
						"only a data object and a node list!\n",
						eventNames[type]);
#if HAVE_EXCEPTION
					throw 
						EventException(
							type, 
							"Event type %s does not accept void * as data - "
							"only a data object and a node list!");
#endif
				break;
				
				case EVENT_TYPE_DELEGATE_NODES:
					HAGGLE_DBG(
						"ERROR: Event type %s does not accept void * as data - "
						"only a data object, a node and a node list!\n",
						eventNames[type]);
#if HAVE_EXCEPTION
					throw 
						EventException(
							type, 
							"Event type %s does not accept void * as data - "
							"only a data object, a node and a node list!");
#endif
				break;
				
				case EVENT_TYPE_LOCAL_INTERFACE_UP:
				case EVENT_TYPE_LOCAL_INTERFACE_DOWN:
				case EVENT_TYPE_NEIGHBOR_INTERFACE_UP:
				case EVENT_TYPE_NEIGHBOR_INTERFACE_DOWN:
					HAGGLE_DBG(
						"ERROR: Event type %s does not accept void * as data - "
						"only an interface!\n",
						eventNames[type]);
#if HAVE_EXCEPTION
					throw 
						EventException(
							type, 
							"Event type %s does not accept void * as data - "
							"only an interface!");
#endif
				break;
				
				case EVENT_TYPE_NODE_CONTACT_NEW:
				case EVENT_TYPE_NODE_CONTACT_END:
					HAGGLE_DBG(
						"ERROR: Event type %s does not accept void * as data - "
						"only a node!\n",
						eventNames[type]);
#if HAVE_EXCEPTION
					throw 
						EventException(
							type, 
							"Event type %s does not accept void * as data - "
							"only a node!");
#endif
				break;
				
				case EVENT_TYPE_RESOURCE_POLICY_NEW:
					HAGGLE_DBG(
						"ERROR: Event type %s does not accept void * as data - "
						"only a resource policy!\n",
						eventNames[type]);
#if HAVE_EXCEPTION
					throw 
						EventException(
							type, 
							"Event type %s does not accept void * as data - "
							"only a resource policy!");
#endif
				break;
				
#ifdef DEBUG
				case EVENT_TYPE_DEBUG_CMD:
					HAGGLE_DBG(
						"ERROR: Event type %s does not accept void * as data - "
						"only a debug command!\n",
						eventNames[type]);
#if HAVE_EXCEPTION
					throw 
						EventException(
							type, 
							"Event type %s does not accept void * as data - "
							"only a debug command!");
#endif
				break;
#endif
				
				default:
				break;
			}
		}
	}
}

Event::Event(const EventCallback < EventHandler > *_callback, void *_data, double _delay) :
#ifdef DEBUG_LEAKS
	LeakMonitor(LEAK_TYPE_EVENT),
#endif 
	HeapItem(),
	type(EVENT_TYPE_CALLBACK), 
	timeout(absolute_time_double(_delay)), 
	callback(_callback), 
	data(_data),
	doesHaveData(_data ? true : false)
{
#if HAVE_EXCEPTION
	if (!callback)
		throw EventException(type, "No callback function in argument");
#endif
}


Event::Event(const EventCallback<EventHandler> *_callback, const DataObjectRef&  _dObjRef, double _delay) :
#ifdef DEBUG_LEAKS
	LeakMonitor(LEAK_TYPE_EVENT),
#endif 
	HeapItem(),
	type(EVENT_TYPE_CALLBACK), 
	timeout(absolute_time_double(_delay)), 
	callback(_callback), 
	dObjRef(_dObjRef),
	data(NULL),
	doesHaveData(_dObjRef ? true : false)
{
#if HAVE_EXCEPTION
	if (!callback)
		throw EventException(type, "No callback function in argument");
#endif
}

Event::Event(const EventCallback<EventHandler> *_callback, const InterfaceRef& _ifaceRef, double _delay) :
#ifdef DEBUG_LEAKS
	LeakMonitor(LEAK_TYPE_EVENT),
#endif 
	HeapItem(),
	type(EVENT_TYPE_CALLBACK), 
	timeout(absolute_time_double(_delay)), 
	callback(_callback), 
	ifaceRef(_ifaceRef),
	data(NULL),
	doesHaveData(_ifaceRef ? true : false)
{
#if HAVE_EXCEPTION
	if (!callback)
		throw EventException(type, "No callback function in argument");
#endif
}

Event::Event(const EventCallback<EventHandler> *_callback, const NodeRef& _nodeRef, double _delay) :
#ifdef DEBUG_LEAKS
	LeakMonitor(LEAK_TYPE_EVENT),
#endif 
	HeapItem(),
	type(EVENT_TYPE_CALLBACK), 
	timeout(absolute_time_double(_delay)),
	callback(_callback), 
	nodeRef(_nodeRef),
	data(NULL),
	doesHaveData(_nodeRef ? true : false)
{
#if HAVE_EXCEPTION
	if (!callback)
		throw EventException(type, "No callback function in argument");
#endif
}

Event::Event(const EventCallback<EventHandler> *_callback, const PolicyRef& _policyRef, double _delay) :
#ifdef DEBUG_LEAKS
	LeakMonitor(LEAK_TYPE_EVENT),
#endif 
	HeapItem(),
	type(EVENT_TYPE_CALLBACK), 
	timeout(absolute_time_double(_delay)), 
	callback(_callback), 
	policyRef(_policyRef),
	data(NULL),
	doesHaveData(_policyRef ? true : false)
{
#if HAVE_EXCEPTION
	if (!callback)
		throw EventException(type, "No callback function in argument");
#endif
}


#ifdef DEBUG
Event::Event(const EventCallback<EventHandler> *_callback, const DebugCmdRef& _dbgCmdRef, double _delay) :
#ifdef DEBUG_LEAKS
	LeakMonitor(LEAK_TYPE_EVENT),
#endif 
	HeapItem(),
	type(EVENT_TYPE_CALLBACK), 
	timeout(absolute_time_double(_delay)), 
	callback(_callback), 
	dbgCmdRef(_dbgCmdRef),
	data(NULL),
	doesHaveData(_dbgCmdRef ? true : false)
{
#if HAVE_EXCEPTION
	if (!callback)
		throw EventException(type, "No callback function in argument");
#endif
}
#endif


Event::~Event()
{
}

string Event::getDescription(void)
{
	char tmp[64];
	// Start with an empty description:
	string description = "";
	// Fallback: no data object => set the id to "-"
	string dObjIdStr = "-";
	// Fallback: no node => set the id to "-"
	string nodeIdStr = "-";
	// Fallback: no interface => set the id to "-"
	string ifaceStr = "-";
	// Fallback: no policy => set the id to "-"
	string policyStr = "-";
	// Fallback: no data => set the id to "-"
	string dataStr = "-";
	// This will be overwritten below:
	string eventTypeStr = "??";
	// Fallback: no event name => set it to:
	string eventNameStr = "[unknown event type]";
	
	// If there is a data object:
	if (dObjRef) {
		// Report this as the data object's id str + it's number:
		snprintf(tmp, 64, "%s-%d", dObjRef->getIdStr(), dObjRef->getNum());
		dObjIdStr = tmp;
	}
	
	// If there is a node:
	if (nodeRef) {
		// Report the node's id str:
		nodeIdStr = nodeRef->getIdStr();
	}
	
	// If there is an interface:
	if (ifaceRef) {
		// Report the interface's id str:
		ifaceStr = ifaceRef->getIdentifierStr();
	}
	
	// If there is a policy:
	if (policyRef) {
		// Report that there is (there is no way of identifying policies):
		policyStr = "+";
	}
	
	// If there is private data:
	if (data) {
		// Report that there is (there is really no way of telling what this 
		// is):
		dataStr = "+";
	}
	
	// Find the type number of this event:
	snprintf(tmp, 64, "%d", getType());
	eventTypeStr = tmp;

	// Find the name of this event:
	eventNameStr = getName();
	
	// Start with the event number:
	description  = eventTypeStr + '\t';
	// Then the DO id:
	description += dObjIdStr + '\t';
	// Then the node id:
	description += nodeIdStr + '\t';
	// Then the interface id:
	description += ifaceStr + '\t';
	// Then the policy id:
	description += policyStr + '\t';
	// Then the data id:
	description += dataStr + '\t';
	// Then the event's name:
	description += eventNameStr;
	
	// Done:
	return description;
}

int Event::registerType(const char *name, EventCallback < EventHandler > *_callback)
{
	char *buf;
	EventType _type;

	if (num_event_types >= MAX_NUM_EVENT_TYPES)
		return -1;

	/* Find first free private event type slot */
	for (_type = EVENT_TYPE_PRIVATE_MIN; _type < EVENT_TYPE_PRIVATE_MAX; _type++) {
		if (privCallbacks[Event::privTypeToCallbackIndex(_type)] == NULL) {

			buf = (char *) malloc(strlen(name) + 1);

			if (!buf) {
				return -1;
			}

			strcpy(buf, name);

			num_event_types++;

			eventNames[_type] = buf;
			privCallbacks[Event::privTypeToCallbackIndex(_type)] = _callback;
			HAGGLE_DBG("Registered private event %d/%d:\'%s\'\n", _type, EVENT_TYPE_MAX, name);
			return _type;
		}
	}

	return -1;
}

int Event::unregisterType(EventType _type)
{
	if (!EVENT_TYPE_PRIVATE(_type)) {
		HAGGLE_ERR("Tried to delete event non-private event type %d\n", _type);
		return -1;
	}

	if (privCallbacks[privTypeToCallbackIndex(_type)] != NULL) {
		HAGGLE_DBG("Deleting event type %d: %s\n", _type, eventNames[_type]);

		delete privCallbacks[privTypeToCallbackIndex(_type)];
		privCallbacks[privTypeToCallbackIndex(_type)] = NULL;
		free(const_cast < char *>(eventNames[_type]));
		eventNames[_type] = NULL;

		return --num_event_types;
	} else {
		HAGGLE_ERR("Tried to delete empty event type %d, double delete?\n", _type);
		return -1;
	}
}
