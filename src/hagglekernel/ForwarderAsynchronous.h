/* Copyright 2009 Uppsala University
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
#ifndef _FORWARDERASYNCHRONOUS_H
#define _FORWARDERASYNCHRONOUS_H

/*
	Forward declarations of all data types declared in this file. This is to
	avoid circular dependencies. If/when a data type is added to this file,
	remember to add it here.
*/
class ForwarderAsynchronous;

#include "Forwarder.h"
#include "DataObject.h"
#include "Node.h"

#include <libcpphaggle/String.h>
#include <libcpphaggle/Mutex.h>
#include <libcpphaggle/GenericQueue.h>
#include <haggleutils.h>

/**
	This enum is used in the actions to tell the run loop what to do.
*/
typedef enum {
	// Add bew metric data to the routing table
	FWD_TASK_NEW_ROUTING_INFO,
	// This neighbor was just seen in the neighborhood
	FWD_TASK_NEW_NEIGHBOR,
	// This neighbor just left the neighborhood
	FWD_TASK_END_NEIGHBOR,
	// Get the nodes that delegateNode is a good delegate forwarder for
	FWD_TASK_GENERATE_TARGETS,
	// Get the nodes that are good delegate forwarders for this node
	FWD_TASK_GENERATE_DELEGATES,
	FWD_TASK_GENERATE_ROUTING_INFO_DATA_OBJECT,
#ifdef DEBUG
	// Print the routing table:
	FWD_TASK_PRINT_RIB,
#endif
	// Get the internal state as XML:
	FWD_TASK_GET_XML_STATE,
	// Terminate the run loop
	FWD_TASK_QUIT
} ForwardingTaskType_t;

/**
	These action elements are used to send data to the run loop, in order to
	make processing asynchronous.
*/
class ForwardingTask {
private:
	ForwardingTaskType_t type;
	DataObjectRef dObj;
	NodeRef	node;
	RepositoryEntryList *rel;
        string xml;
public:
	ForwardingTask(ForwardingTaskType_t _type, DataObjectRef _dObj = NULL, NodeRef _node = NULL) :
		type(_type), dObj(_dObj), node(_node), rel(NULL) {}
	ForwardingTask(ForwardingTaskType_t _type, NodeRef _node) :
		type(_type), dObj(NULL), node(_node), rel(NULL) {}
	DataObjectRef& getDataObject() { return dObj; }
	void setDataObject(DataObjectRef _dObj) { dObj = _dObj; }
	NodeRef& getNode() { return node; }
	RepositoryEntryList *getRepositoryEntryList() { return rel; }
	void setRepositoryEntryList(RepositoryEntryList *_rel) { if (!rel) {rel = _rel;} }
	const ForwardingTaskType_t getType() const { return type; }
        void setXML(const string& _xml) { xml = _xml; }
        const string& getXML() const { return xml; }
	~ForwardingTask() { if (rel) delete rel; }
};

/**
	Asynchronous forwarding module. A forwarding module should inherit from this
	module if it is doing too much processing to be executing in the kernel 
	thread.
*/
class ForwarderAsynchronous : public Forwarder {
	const EventType eventType;
	
	GenericQueue<ForwardingTask *> taskQ;	/**
		Main run loop for the prophet forwarder.
	*/
	bool run(void);
protected:
	/**
		Does the actual work of newRoutingInformation
	*/
	virtual bool newRoutingInformation(const Metadata *m) { return false; }
	
	/**
		Does the actual work of newNeighbor.
	*/
	virtual void _newNeighbor(NodeRef &neighbor) {}
	
	/**
		Does the actual work of endNeighbor.
	*/
	virtual void _endNeighbor(NodeRef &neighbor) {}
	
	/**
		Does the actual work of getTargetsFor.
	*/
	virtual void _generateTargetsFor(NodeRef &neighbor) {}
	
	/**
		Does the actual work of getDelegatesFor.
	*/
	virtual void _generateDelegatesFor(DataObjectRef &dObj, NodeRef &target) {}
		
#ifdef DEBUG
	/**
		Does the actual work or printRoutingTable().
	*/
	virtual void _printRoutingTable(void) {}
#endif
	/**
		Does the actual work or getInternatlStateAsXML().
		
		This function only exists in the haggle-demo branch, and should only
		be there. Changes including this function should not be merged with
		the default development branch.
	*/
	virtual const string _getInternalStateAsXML(void) { return ""; }

public:
	ForwarderAsynchronous(ForwardingManager *m = NULL, const EventType type = -1, const string name = "Asynchronous forwarding module");
	/**
	 Generally, the thread should not be stopped by doing a delete 
	 on the forwarding module object. This is because, once in the destructor,
	 the state of the object might already have been deleted, and then it
	 is too late to save it. Use the quit() function instead, because it will
	 stop the thread and allow it to save its state before the destructor is
	 called. The stop() in the destructor is only a safeguard in case the thread 
	 is for some reason already running
	 */
	~ForwarderAsynchronous();
	
	/**
	  Call quit() when the forwarding module thread should exit. After calling quit, 
	  the forwarding module will save its state to the data store and then exit. 
	 */ 
	void quit();
	
	/** See the parent class function with the same name. */
	void newRoutingInformation(DataObjectRef& dObj);
	/** See the parent class function with the same name. */
	void newNeighbor(NodeRef &neighbor);
	/** See the parent class function with the same name. */
	void endNeighbor(NodeRef &neighbor);
	/** See the parent class function with the same name. */
	void generateTargetsFor(NodeRef &neighbor);
	/** See the parent class function with the same name. */
	void generateDelegatesFor(DataObjectRef &dObj, NodeRef &target);
	/** See the parent class function with the same name. */
	void generateRoutingInformationDataObject(NodeRef &neighbor);
#ifdef DEBUG
	/** See the parent class function with the same name. */
	void printRoutingTable(void);
#endif
	/** See the parent class function with the same name. */
	void getInternalStateAsXML(void);
};

#endif
