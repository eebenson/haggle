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
#ifndef _NODEMANAGER_H
#define _NODEMANAGER_H

/*
	Forward declarations of all data types declared in this file. This is to
	avoid circular dependencies. If/when a data type is added to this file,
	remember to add it here.
*/

class NodeManager;

#include "Event.h"
#include "Manager.h"
#include "Filter.h"

typedef List< Pair<NodeRef, DataObjectRef> > NodeExchangeList;

/** */
class NodeManager : public Manager
{
	long thumbnail_size;
	char *thumbnail;
	unsigned long sequence_number;
	NodeExchangeList nodeExchangeList;
        EventCallback<EventHandler> *filterQueryCallback;
	EventCallback<EventHandler> *onRetrieveNodeCallback;
	EventCallback<EventHandler> *onRetrieveThisNodeCallback;
	EventCallback<EventHandler> *onRetrieveNodeDescriptionCallback;
        EventType nodeDescriptionEType;
        int sendNodeDescription(NodeRefList& neighList);
        void onFilterQueryResult(Event *e);
        void onApplicationFilterMatchEvent(Event *e);
        void onSendNodeDescription(Event *e);
        void onReceiveNodeDescription(Event *e);
        void onLocalInterfaceUp(Event *e);
        void onLocalInterfaceDown(Event *e);
        void onNeighborInterfaceUp(Event *e);
        void onNeighborInterfaceDown(Event *e);
        void onNewNodeContact(Event *e);
	void onSendResult(Event *e);
	void onRetrieveNode(Event *e);
	void onRetrieveNodeDescription(Event *e);
	//int onNodeContactEnd(Event *e);
	void onRetrieveThisNode(Event *e);
	void onNodeInformation(Event *e);

#if defined(ENABLE_METADAPARSER)
        bool onParseMetadata(Metadata *md);
#endif
	
	void onPrepareShutdown();
public:
        NodeManager(HaggleKernel *_haggle = haggleKernel);
        ~NodeManager();

class NodeManagerException : public ManagerException
        {
        public:
                NodeManagerException(const int err = 0, const char* data = "Node manager Error") : ManagerException(err, data) {}
        };
};


#endif /* _NODEMANAGER_H */
