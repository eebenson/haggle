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
#include <time.h>

#include <libcpphaggle/Platform.h>
#include <libcpphaggle/String.h>
#include <haggleutils.h>

#include "EventQueue.h"
#include "NodeManager.h"
#include "DataObject.h"
#include "Node.h"
#include "Event.h"
#include "Attribute.h"
#include "Interface.h"
#include "Filter.h"

using namespace haggle;

#define FILTER_KEYWORD NODE_DESC_ATTR
//#define FILTER_NODEDESCRIPTION FILTER_KEYWORD "=true"
#define FILTER_NODEDESCRIPTION FILTER_KEYWORD "=" ATTR_WILDCARD

NodeManager::NodeManager(HaggleKernel * _haggle) : 
	Manager("NodeManager", _haggle), 
	thumbnail_size(0), thumbnail(NULL), 
	sequence_number(0)
{
#define __CLASS__ NodeManager

	// Register filter for node descriptions
	registerEventTypeForFilter(
		nodeDescriptionEType,
		"NodeManager NodeDescription Filter Event",	
		onReceiveNodeDescription,
		FILTER_NODEDESCRIPTION);

	setEventHandler(EVENT_TYPE_LOCAL_INTERFACE_UP, onLocalInterfaceUp);
	setEventHandler(EVENT_TYPE_LOCAL_INTERFACE_DOWN, onLocalInterfaceDown);
	setEventHandler(EVENT_TYPE_NEIGHBOR_INTERFACE_UP, onNeighborInterfaceUp);
	setEventHandler(EVENT_TYPE_NEIGHBOR_INTERFACE_DOWN, onNeighborInterfaceDown);

	setEventHandler(EVENT_TYPE_NODE_CONTACT_NEW, onNewNodeContact);
	setEventHandler(EVENT_TYPE_NODE_DESCRIPTION_SEND, onSendNodeDescription);
	
	setEventHandler(EVENT_TYPE_DATAOBJECT_SEND_SUCCESSFUL, onSendResult);
	setEventHandler(EVENT_TYPE_DATAOBJECT_SEND_FAILURE, onSendResult);
	
	//setEventHandler(EVENT_TYPE_NEIGHBOR_INTERFACE_DOWN, onNeighborInterfaceDown);

	filterQueryCallback = newEventCallback(onFilterQueryResult);

	onRetrieveNodeCallback = newEventCallback(onRetrieveNode);
	onRetrieveThisNodeCallback = newEventCallback(onRetrieveThisNode);
	onRetrieveNodeDescriptionCallback = newEventCallback(onRetrieveNodeDescription);

	kernel->getDataStore()->retrieveNode(kernel->getThisNode(), onRetrieveThisNodeCallback);
	
	
	/*
		We only search for a thumbnail at haggle startup time, to avoid 
		searching the disk every time we create a new node description.
	*/
	/*
		Search for and (if found) load the avatar image for this node.
	*/
#if defined(OS_ANDROID)
        // This is a ugly hack for Android in order to not store the
        // Avatar.jpg in /usr/bin
	string str = HAGGLE_DEFAULT_STORAGE_PATH;
#else
	string str = HAGGLE_FOLDER_PATH;
#endif
	str += PLATFORM_PATH_DELIMITER;
	str += "Avatar.jpg";
	FILE *fp = fopen(str.c_str(), "r");
        
	if (fp != NULL) {
		fseek(fp, 0, SEEK_END);
		thumbnail_size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		thumbnail = (char *) malloc(thumbnail_size);
	
                if (thumbnail == NULL) {
			thumbnail_size = 0;
		} else {
			if (fread(thumbnail, 1, thumbnail_size, fp) != (size_t) thumbnail_size) {
                                free(thumbnail);
                                thumbnail = NULL;
                        }
		}
		fclose(fp);
	}

	if (thumbnail == NULL){
		HAGGLE_DBG("No avatar image found.\n");
	} else {
		HAGGLE_DBG("Found avatar image. Will attach to all node descriptions\n");
	}
	// add node information to trace
//	Event nodeInfoEvent(onRetrieveThisNodeCallback, kernel->getThisNode());
//	LOG_ADD("%s: %s NodeManager thisNode information\n", Timeval::now().getAsString().c_str(), nodeInfoEvent.getDescription().c_str());
}

NodeManager::~NodeManager()
{
	if (filterQueryCallback)
		delete filterQueryCallback;
	
	if (onRetrieveNodeCallback)
		delete onRetrieveNodeCallback;
	
	if (onRetrieveThisNodeCallback)
		delete onRetrieveThisNodeCallback;
	
	if (onRetrieveNodeDescriptionCallback)
		delete onRetrieveNodeDescriptionCallback;
}

void NodeManager::onPrepareShutdown()
{
	// Remove the node description filter from the data store:
	unregisterEventTypeForFilter(nodeDescriptionEType);
	// Save the "this node" node in the data store so it can be retrieved when 
	// we next start up.
	kernel->getDataStore()->insertNode(kernel->getThisNode());
	
	// We're done:
	signalIsReadyForShutdown();
}

#if defined(ENABLE_METADAPARSER)
bool NodeManager::onParseMetadata(Metadata *md)
{         
        HAGGLE_DBG("NodeManager onParseMetadata()\n");

        // FIXME: should check 'Node' section of metadata
        return true;
}
#endif

void NodeManager::onRetrieveThisNode(Event *e)
{
	if (!e || !e->hasData())
		return;
	
	NodeRef node = e->getNode();
	
	// If we found a "this node" in the data store:
	if (node) {
		// try to update the node store with that node:
		if (kernel->getNodeStore()->update(node))
			// Success! update the hagglekernel's reference, too.
			kernel->setThisNode(node);
	}
	// FIXME: set these to better values.
	// FIXME: respond to (?) and set this accordingly.
	kernel->getThisNode()->setMatchingThreshold(0);
	// FIXME: respond to the resource manager, and set this value accordingly.
	kernel->getThisNode()->setMaxDataObjectsInMatch(10);
	// Update create time to mark the freshness of the thisNode node description
	kernel->getThisNode()->setCreateTime();
}

int NodeManager::sendNodeDescription(NodeRef neigh)
{
	DataObjectRef dObj = kernel->getThisNode()->getDataObject();

	if (neigh->getBloomfilter()->has(dObj)) {
		HAGGLE_DBG("Neighbor %s already has our most recent node description\n", neigh->getName().c_str());
		return 0;
	}

	if (thumbnail != NULL)
		dObj->setThumbnail(thumbnail, thumbnail_size);
	
	HAGGLE_DBG("Sending node description to \'%s\'\n", neigh->getName().c_str());
	kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_SEND, dObj, neigh));
	
	// Remember that we tried to send our node description to this node:
	nodeExchangeList.push_back(Pair<NodeRef, DataObjectRef>(neigh,dObj));
	
	return 1;
}

void NodeManager::onSendResult(Event * e)
{
	NodeRef &node = e->getNode();
	NodeRef neigh = kernel->getNodeStore()->retrieve(node, false);
	DataObjectRef &dObj = e->getDataObject();
	
	// Go through all our data regarding current node exchanges:
	for (NodeExchangeList::iterator it = nodeExchangeList.begin();
             it != nodeExchangeList.end(); it++) {
		// Is this the one?
		if ((*it).first == node && (*it).second == dObj) {
			// Yes.
			
			// Was the exchange successful?
			if (e->getType() == EVENT_TYPE_DATAOBJECT_SEND_SUCCESSFUL) {
				// Yes. Set the flag.
				if (neigh)
					neigh->setExchangedNodeDescription(true);
				else
					node->setExchangedNodeDescription(true);
			} else if (e->getType() == EVENT_TYPE_DATAOBJECT_SEND_FAILURE) {
				// No. Unset the flag.
				if (neigh)
					neigh->setExchangedNodeDescription(false);
				else
					node->setExchangedNodeDescription(false);
				// FIXME: retry?
			}
			// Remove this entry from the list:
			nodeExchangeList.erase(it);
			
			// Done, no need to look further.
			return;
		}
	}
}

void NodeManager::onFilterQueryResult(Event * e)
{
}

void NodeManager::onLocalInterfaceUp(Event * e)
{
	kernel->getThisNode()->addInterface(e->getInterface());
}

void NodeManager::onLocalInterfaceDown(Event *e)
{
	kernel->getThisNode()->removeInterface(e->getInterface());
}

void NodeManager::onNeighborInterfaceUp(Event *e)
{
	NodeRef neigh = kernel->getNodeStore()->retrieve(e->getInterface(), true);

	if (!neigh) {
		// No one had that interface?

		// Create new node
		// It will have uninitilized state
		neigh = NodeRef(new Node(NODE_TYPE_UNDEF), "NodeFromInterfaceUp");

		// Add this interface to it
		neigh->addInterface(e->getInterface());

		// merge if node exists in datastore (asynchronous call), we force it to return
		// as we only generate a node up event when we know we have the best information
		// for the node.
		kernel->getDataStore()->retrieveNode(neigh, onRetrieveNodeCallback, true);
	} else {
		neigh->setInterfaceUp(e->getInterface());
	}
}

/* 
	callback on retrieve node from Datastore
 
	call in NodeManager::onNeighborInterfaceUp 
	to retrieve a node with matching interfaces to an undefined node
*/
void NodeManager::onRetrieveNode(Event *e)
{
	if (!e || !e->hasData())
		return;
	
	NodeRef node = e->getNode();

	// See if this node is already an active neighbor but in an uninitialized state
	if (kernel->getNodeStore()->update(node)) {
		HAGGLE_DBG("Node was updated in neighbor list\n", node->getIdStr());
	} else {
		HAGGLE_DBG("Node %s not previously neighbor... Adding to neighbor list\n", node->getIdStr());
		kernel->getNodeStore()->add(node);
	}
	
	kernel->addEvent(new Event(EVENT_TYPE_NODE_CONTACT_NEW, node));
}

void NodeManager::onNeighborInterfaceDown(Event *e)
{
	// Let the NodeStore know:
	NodeRef node = kernel->getNodeStore()->retrieve(e->getInterface(), false);
	
	if (node) {
		node->setInterfaceDown(e->getInterface());
		
		if (!node->isAvailable()) {
			kernel->getNodeStore()->remove(node);

			/*
				We need to update the node information in the data store 
				since the bloomfilter might have been updated during the 
				neighbor's co-location.
			*/
			kernel->getDataStore()->insertNode(node);

			// Report the node as down
			kernel->addEvent(new Event(EVENT_TYPE_NODE_CONTACT_END, node));
		}
	}
}

void NodeManager::onNewNodeContact(Event *e)
{
	if (!e)
		return;

	NodeRef neigh = e->getNode();

	switch (neigh->getType()) {
	case NODE_TYPE_UNDEF:
		HAGGLE_DBG("%s - New node contact. Have not yet received node description!\n", getName());
		break;
	case NODE_TYPE_PEER:
		HAGGLE_DBG("%s - New node contact %s\n", getName(), neigh->getIdStr());
		break;
	case NODE_TYPE_GATEWAY:
		HAGGLE_DBG("%s - New gateway contact %s\n", getName(), neigh->getIdStr());
		break;
	default:
		break;
	}

	sendNodeDescription(neigh);
}

// Push our node description to all neighbors
void NodeManager::onSendNodeDescription(Event *e)
{
	NodeRefList neighList;

	unsigned long num = kernel->getNodeStore()->retrieveNeighbors(neighList);

	if (num <= 0) {
		HAGGLE_DBG("No neighbors - not sending node description\n");
		return;
	}

	// We ignore "this node", i.e., ourselves
	HAGGLE_DBG("Pushing node description to %d neighbors\n", neighList.size());

	for (NodeRefList::iterator it = neighList.begin(); it != neighList.end(); it++) {
		NodeRef neigh = *it;
		
		sendNodeDescription(neigh);
	}
}


void NodeManager::onReceiveNodeDescription(Event *e)
{
	if (!e || !e->hasData())
		return;

	DataObjectRef dObj = e->getDataObject();

	NodeRef node = NodeRef(new Node(NODE_TYPE_PEER, dObj), "NodeFromNodeDescription");
	
	if (!node) {
		HAGGLE_DBG("Could not create node from metadata!\n");
		return;
	}
	
	HAGGLE_DBG("Node description data object %s, refcount=%d\n", dObj.getName(), dObj.refcount());
	HAGGLE_DBG("Node description from node with id=%s\n", node->getIdStr());
	
	if (node == kernel->getThisNode()) {
		HAGGLE_ERR("Node description is my own. Ignoring and deleting from data store\n");
		// Remove the data object from the data store:
		kernel->getDataStore()->deleteDataObject(dObj);
		return;
	}
	
	// Make sure at least the interface of the remote node is set to up
	// this 
	if (dObj->getRemoteInterface()) {
		// Mark the interface as up in the node.
		node->setInterfaceUp(dObj->getRemoteInterface());
		
		if (node->hasInterface(dObj->getRemoteInterface())) {			
		} else {
			// Node description was received from a third party
		}
	} else {
		HAGGLE_DBG("Node description data object has no remote interface\n");
	}
	
	// The received node description may be older than one that we already have stored. Therefore, we
	// need to retrieve any stored node descriptions before we accept this one.
	char filterString[255];
	sprintf(filterString, "%s=%s", NODE_DESC_ATTR, node->getIdStr());
	
	kernel->getDataStore()->doFilterQuery(new Filter(filterString, 0), onRetrieveNodeDescriptionCallback);
}

/* 
	callback to clean-up outdated nodedescriptions in the DataStore

	call in NodeManager::onReceiveNodeDescription through filterQuery
*/
void NodeManager::onRetrieveNodeDescription(Event *e)
{
	if (!e || !e->hasData())
		return;
	
	DataStoreQueryResult *qr = static_cast < DataStoreQueryResult * >(e->getData());

	DataObjectRef dObj_tmp;
	DataObjectRef dObj = qr->detachFirstDataObject();
	Timeval receiveTime = dObj->getReceiveTime();
	/*
	HAGGLE_DBG("Node description id=%s\n", dObj->getIdStr());

	HAGGLE_DBG("Node description createTime %lf receiveTime %lf\n",
		   dObj->getCreateTime().getTimeAsSecondsDouble(),
		   receiveTime.getTimeAsSecondsDouble());
	*/

	while ((dObj_tmp = qr->detachFirstDataObject())) {

		HAGGLE_DBG("Node description createTime %lf receiveTime %lf\n",
			   dObj_tmp->getCreateTime().getTimeAsSecondsDouble(),
			   dObj_tmp->getReceiveTime().getTimeAsSecondsDouble());
		/*   
		HAGGLE_DBG("Time difference between node descriptions: receiveTime: %lf, createTime: %lf\n",
			   receiveTime.getTimeAsSecondsDouble() -
			   dObj_tmp->getReceiveTime().getTimeAsSecondsDouble(),
			   dObj->getCreateTime().getTimeAsSecondsDouble() -
			   dObj_tmp->getCreateTime().getTimeAsSecondsDouble());
		*/
		if (dObj_tmp->getReceiveTime() > dObj->getReceiveTime()) {
			receiveTime = dObj_tmp->getReceiveTime();
		}
		if (dObj_tmp->getCreateTime() > dObj->getCreateTime()) {
			// This node description was newer than the last "newest":
			
			HAGGLE_DBG("Found newer node description, deleting id=%s with createTime %lf\n",
				   dObj->getIdStr(), dObj->getCreateTime().getTimeAsSecondsDouble());
			// Delete the old "newest" data object:
			kernel->getDataStore()->deleteDataObject(dObj);
			// FIXME: we should really remove the data object from the 
			// "this" node's bloomfilter here.
			
			dObj = dObj_tmp;
		} else {
			// This node description was not newer than the last "newest":
			/*
			HAGGLE_DBG("Found older node description, deleting id=%s with createTime %lf\n",
				   dObj_tmp->getIdStr(), dObj_tmp->getCreateTime().getTimeAsSecondsDouble());
			*/
			// Delete it:
			kernel->getDataStore()->deleteDataObject(dObj_tmp);
			// FIXME: we should really remove the data object from the 
			// "this" node's bloomfilter here.
		}
	}

	/*
		If the greatest receive time is not equal to the one in the
		latest created node description, then we received an old node description 
		(i.e., we had a more recent one in the data store).
	*/
	if (receiveTime != dObj->getReceiveTime()) {
		HAGGLE_DBG("Received node description is not the latest, ignoring... latest: %s - dObj: %s\n", 
			receiveTime.getAsString().c_str(), dObj->getReceiveTime().getAsString().c_str());

		// The most recently received node description must have been received from a third party,
		// so we ignore it.
		delete qr;
		return;
	} 
	HAGGLE_DBG("Received fresh node description -- creating node: createTime %s receiveTime %s\n", 
		   dObj->getCreateTime().getAsString().c_str(), receiveTime.getAsString().c_str());

	NodeRef node = NodeRef(new Node(NODE_TYPE_PEER, dObj));

	// insert node into DataStore
	kernel->getDataStore()->insertNode(node);
	
	NodeRefList nl;
	
	// See if this node is already an active neighbor but in an uninitialized state
	if (kernel->getNodeStore()->update(node, &nl)) {
		HAGGLE_DBG("Node was updated in neighbor list\n", node->getIdStr());
		kernel->addEvent(new Event(EVENT_TYPE_NODE_UPDATED, node, nl));
	} else {
		HAGGLE_DBG("Node %s not previously neighbor...\n", node->getIdStr());
		
		// Sync the node's interfaces with those in the interface store. This
		// makes sure all active interfaces are marked as 'up'.
		node.lock();
		const InterfaceRefList *ifl = node->getInterfaces();
		
		for (InterfaceRefList::const_iterator it = ifl->begin(); it != ifl->end(); it++) {
			if (kernel->getInterfaceStore()->stored(*it)) {
				node->setInterfaceUp(*it);
			}
		}
		node.unlock();
		
		if (node->isAvailable()) {
			// Add node to node store
			HAGGLE_DBG("Adding new neighbor %s to node store\n", node->getIdStr());
			
			kernel->getNodeStore()->add(node);
		
			// Tell anyone that may wish to know:
			// FIXME: Is this really necessary here since NODE_CONTACT_NEW is
			// also generated by onRetrieveNode whenever a new neigbhor interface goes
			// up?
			kernel->addEvent(new Event(EVENT_TYPE_NODE_CONTACT_NEW, node));
		} else {
			HAGGLE_DBG("Node %s had no active interfaces, not adding to store\n", node->getIdStr());
		}
	}
	
	delete qr;
}

