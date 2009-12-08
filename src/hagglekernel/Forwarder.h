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
#ifndef _FORWARDER_H
#define _FORWARDER_H

/*
	Forward declarations of all data types declared in this file. This is to
	avoid circular dependencies. If/when a data type is added to this file,
	remember to add it here.
*/
class Forwarder;

#include "ManagerModule.h"
#include "ForwardingManager.h"
#include "RepositoryEntry.h"

/**
	Forwarding module base class.
	
	The forwarding manager will not actively call ->start() on its forwarding 
	object, so it is up to the forwarding module itself to start its thread if 
	it wants to run as a thread.
*/
class Forwarder : public ManagerModule<ForwardingManager> {
public:
	Forwarder(ForwardingManager *m = NULL, 
		const string name = "Unknown forwarding module") :
		ManagerModule<ForwardingManager>(m, name) {}
	~Forwarder() {}
	
	// Only useful for asynchronous modules
	virtual void quit() {}

	DataObjectRef createRoutingInformationDataObject();
	
	virtual bool addRoutingInformation(DataObjectRef& dObj, Metadata *m) { return false; }
	/**
		This function determines if the given data object contains routing information
		for this forwarding module. 
	 
		Returns: true iff the data object routing information created by this forwarding module.
	*/
	virtual bool hasRoutingInformation(const DataObjectRef& dObj) const;
	
	/**
		This function returns a string with the node id for the node which 
		created the routing information
		
		Returns: if routing information is valid, a string containing a node id. 
		Otherwise NULL.
	*/
	virtual const string getNodeIdFromRoutingInformation(const DataObjectRef& dObj) const;
	
	/**
		This function returns a string with the metric for the node which 
		created the given metric data object.
		
		Returns: if routing information is valid, a string containing a 
		metric. Otherwise NULL.
	*/
	virtual const Metadata *getRoutingInformation(const DataObjectRef& dObj) const;
	
	/*
		The following functions are called by the forwarding manager as part of
		event processing in the kernel. They are therefore called from the 
		kernel thread, and multiprocessing issues need to be taken into account.
		
		The reason for these functions to all be declared virtual ... {} is so 
		that specific forwarding modules can override only those functions they
		actually need to do their job. This means that functions can be declared
		here (and called by the forwarding manager) that only one forwarding 
		algorithm actually uses.
	*/
	
	/**
		Called when a data object has come in that has a "Routing" attribute.
		
		Also called for each such data object that is in the data store on 
		startup.
		
		Since the format of the data in such a data object is unknown to the 
		forwarding manager, it is up to the forwarder to make sure the data is
		in the correct format.
		
		Also, the given metric data object may have been sent before, due to 
		limitations in the forwarding manager.
	*/
	virtual void newRoutingInformation(DataObjectRef& dObj) {}	
	/**
		Called when a neighbor node is discovered.
	*/
	virtual void newNeighbor(NodeRef &neighbor) {}


	/**
		Called when a node just ended being a neighbor.
	*/
	virtual void endNeighbor(NodeRef &neighbor) {}
	
	/**
		Generates an event (EVENT_TYPE_DELEGATE_NODES) providing all the nodes 
		that are good delegate forwarders for the given node.
		
		This function is given a target to which to send a data object, and 
		answers the question: To which delegate forwarders can I send the given
		data object, so that it will reach the given target?
		
		If no nodes are found, no event should be created.
	*/
	virtual void generateDelegatesFor(DataObjectRef &dObj, NodeRef &target) {}
	
	/**
		Generates an event (EVENT_TYPE_TARGET_NODES) providing all the target 
		nodes that the given node is a good delegate forwarder for.
		
		This function is given a current neighbor, and answers the question: 
		For which nodes is the given node a good delegate forwarder?
		
		If no nodes are found, no event should be created.
	*/
	virtual void generateTargetsFor(NodeRef &neighbor) {}
	
	virtual void generateRoutingInformationDataObject(NodeRef& neighbor) {}
	
	virtual size_t getSaveState(RepositoryEntryList& rel) { return 0; }
	virtual bool setSaveState(RepositoryEntryRef& e) { return false; }
	
#ifdef DEBUG
	/**
		Prints the current routing table, without any enclosing text to show
		where it starts or stops.
	*/
	virtual void printRoutingTable(void) {}
#endif
};

#endif
