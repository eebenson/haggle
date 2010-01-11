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

#include "ForwarderRank.h"

ForwarderRank::ForwarderRank(ForwardingManager *m) : 
	Forwarder(m, "RANK")
{
	const char *nodeId = getManager()->getKernel()->getThisNode()->getIdStr();
	myRank = nodeId[0];
	
	// Create a new forwarding data object with the rank as the metric:
	
	/*
	 String tmp;
	stringprintf(tmp, "%ld", myRank);
	createMetricDataObject(tmp);
	
	addMetricDO(myMetricDO);
	 */
	/*
	// Print the data object:
	char *str;
	size_t len;
	md->getRawAlloc(&str, &len);
	HAGGLE_DBG("Created ForwarderRank metric data object:\n%s", str);
	free(str);
	*/
}

ForwarderRank::~ForwarderRank()
{
}

bool ForwarderRank::addRoutingInformation(DataObjectRef& dObj, Metadata *parent)
{
	/*
	if(!isMetricDO(metricDO))
		return;

	ranks[getNodeIdFromMetricDataObject(metricDO)] = 
		strtol(getMetricFromMetricDataObject(metricDO).c_str(), NULL, 10);
*/
	/*
	// Output that we got a new metric do:
	String str;
	
	str = 
		"Got metric DO: " + 
		getNodeIdFromMetricDataObject(metricDO) + " - " + 
		getMetricFromMetricDataObject(metricDO);
	HAGGLE_DBG("%s\n", str.c_str());
	*/
	return false;
}

void ForwarderRank::newNeighbor(const NodeRef &neighbor)
{
}

void ForwarderRank::endNeighbor(const NodeRef &neighbor)
{
}

void ForwarderRank::generateTargetsFor(const NodeRef &neighbor)
{

}

void ForwarderRank::generateDelegatesFor(const DataObjectRef &dObj, const NodeRef &target, const NodeRefList *other_targets)
{
	NodeRefList delegateList;

	for (Map<String, long >::iterator it = ranks.begin(); it != ranks.end(); it++) {
		if (it->second > myRank) {
			NodeRef new_node = new Node(it->first.c_str(), NODE_TYPE_PEER, "Rank delegate node");
			delegateList.add(new_node);
		}
	}

	if (!delegateList.empty())
		getManager()->getKernel()->addEvent(new Event(EVENT_TYPE_DELEGATE_NODES, dObj, target, delegateList));
}
