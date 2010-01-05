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
#include <sqlite3.h>
#include <string.h>
#include <math.h>
#include <libxml/parser.h>
#include <libxml/tree.h> // For dumping to XML

#include "SQLDataStore.h"
#include "DataObject.h"
#include "Attribute.h"
#include "HaggleKernel.h"

#if defined(OS_MACOSX)
// Needed for mkdir
#include <sys/stat.h>
#endif

#include <haggleutils.h>

/*
	Sqlite uses different types for sqlite_int64 depending on platform.
	On UNIX-style platforms the sqlite_int64 is a long long int (%lld), whilst
	on Windows it is a __int64. We need this macro to handle formating when
	converting the sqlite_int64 to a string (with e.g., sprintf).
	Using %lld on Windows will cause problems.
*/
#if defined(OS_WINDOWS)
#define INT64_FORMAT "%I64d"
#else
#define INT64_FORMAT "%lld"
#endif

using namespace haggle;

/*
	Tables for basic data types
*/
#define TABLE_DATAOBJECTS "table_dataobjects"
#define TABLE_INTERFACES "table_interfaces"
#define TABLE_NODES "table_nodes"
#define TABLE_ATTRIBUTES "table_attributes"
#define TABLE_FILTERS "table_filters"

/*
  Tables that map nodes attributes to nodes (or vice versa really).

 */

#define TABLE_MAP_DATAOBJECTS_TO_ATTRIBUTES_VIA_ROWID "table_map_dataobjects_to_attributes_via_rowid"
#define TABLE_MAP_NODES_TO_ATTRIBUTES_VIA_ROWID	"table_map_nodes_to_attributes_via_rowid"
#define TABLE_MAP_FILTERS_TO_ATTRIBUTES_VIA_ROWID "table_map_filters_to_attributes_via_rowid"

/*
	The following views map between dataobject and attributes, nodes and attributes,
	and filters and attributes, respectively.

	|ROWID|dataobject_rowid|attr_rowid|timestamp
	|ROWID|node_rowid|attr_rowid|timestamp

	These views are dynamic in that they are recreated at query time such that they are 
	subsets of the views above in relation to a specific node or data object.
*/
#define VIEW_MAP_DATAOBJECTS_TO_ATTRIBUTES_VIA_ROWID_DYNAMIC "view_map_dataobjects_to_attributes_via_rowid_dynamic"
#define VIEW_MAP_NODES_TO_ATTRIBUTES_VIA_ROWID_DYNAMIC "view_map_nodes_to_attributes_via_rowid_dynamic"


/*
	Count the number of matching attributes between nodes and dataobjects.
	|dataobject_rowid|node_rowid|mcount|dataobject_timestamp
	|dataobject_rowid|node_rowid|mcount|weight|dataobject_timestamp

 */
#define VIEW_MATCH_DATAOBJECTS_AND_NODES "view_match_dataobjects_and_nodes"
#define VIEW_MATCH_NODES_AND_DATAOBJECTS "view_match_nodes_and_dataobjects"
/*
	Match the attributes between nodes and dataobjects and give the ratio calculated
	weight of matching attributes / sum over all node attribute weights (sum_weights)

	|ratio|dataobject_rowid|node_rowid|mcount|dataobject_timestamp

 */
#define VIEW_MATCH_DATAOBJECTS_AND_NODES_AS_RATIO "view_match_dataobjects_and_nodes_as_ratio"
#define VIEW_MATCH_NODES_AND_DATAOBJECTS_AS_RATIO "view_match_nodes_and_dataobjects_as_ratio"

/*
	Same as above but between filters and nodes, and filters and data objects, respectively.
*/
#define VIEW_MATCH_FILTERS_AND_NODES "view_match_filters_and_nodes"
#define VIEW_MATCH_FILTERS_AND_DATAOBJECTS "view_match_filters_and_dataobjects"

#define VIEW_MATCH_FILTERS_AND_NODES_AS_RATIO "view_match_filters_and_nodes_as_ratio"
#define VIEW_MATCH_FILTERS_AND_DATAOBJECTS_AS_RATIO "view_match_filters_and_dataobjects_as_ratio"


/*
	List the attributes of a node or dataobject as name value pairs
	|dataobject_rowid|doid|name|value
	|node_rowid|doid|name|value

*/
#define VIEW_DATAOBJECT_ATTRIBUTES_AS_NAMEVALUE "view_dataobject_attributes_as_namevalue" 
#define VIEW_NODE_ATTRIBUTES_AS_NAMEVALUE "view_node_attributes_as_namevalue" 

/* 
	Views for matching of wildcard attributes
	- filterrelevant attributes: subset of the attribute table with only attributes 
      maped by TABLE_MAP_FILTERS_TO_ATTRIBUTES_VIA_ROWID
	- similar attributes: extends the attribute table with relations between attributes, 
	  indicating the rowid of the similar attribute. 
	  Note: at the moment limited to filterrelevant_attributes
*/
#define VIEW_FILTERRELEVANT_ATTRIBUTES "view_filterrelevant_attributes"
#define VIEW_SIMILAR_ATTRIBUTES "view_similar_attributes"

/*
	Repository Table
	
	persistent storage for Managers and Modules. 
*/
#define TABLE_REPOSITORY "table_repository" 


/* ========================================================= */

/*
	Commands to create tables, triggers, and views.

*/

/* ========================================================= */


/*
	The basic tables for Dataobjects, Nodes, Attributes, Filters, Interfaces
	and related Triggers:
	- update the timestamp on insert
*/

// Create tables for Dataobjects, Nodes, Attributes, Filters, Interfaces
//------------------------------------------
#define SQL_CREATE_TABLE_DATAOBJECTS_CMD "CREATE TABLE IF NOT EXISTS " TABLE_DATAOBJECTS " (ROWID INTEGER PRIMARY KEY AUTOINCREMENT, id BLOB UNIQUE ON CONFLICT ROLLBACK, xmlhdr TEXT, filepath TEXT, filename TEXT, datalen INTEGER, num_attributes INTEGER DEFAULT 0, signaturestatus INTEGER, signee TEXT, createtime INTEGER, receivetime INTEGER, rxtime INTEGER, source_iface_rowid INTEGER, timestamp DATE);"
enum {
	table_dataobjects_rowid	= 0,
	table_dataobjects_id,
	table_dataobjects_xmlhdr,
	table_dataobjects_filepath,
	table_dataobjects_filename,
	table_dataobjects_datalen,
	table_dataobjects_num_attributes,
	table_dataobjects_signature_status,
	table_dataobjects_signee,
	table_dataobjects_createtime, // The creation time in milliseconds (creator's local time)
	table_dataobjects_receivetime, // The receive time in milliseconds (local time)
	table_dataobjects_rxtime, // The transfer time in milliseconds 
	table_dataobjects_source_iface_rowid,
	table_dataobjects_timestamp
};

//------------------------------------------
#define SQL_CREATE_TABLE_NODES_CMD "CREATE TABLE IF NOT EXISTS " TABLE_NODES " (ROWID INTEGER PRIMARY KEY AUTOINCREMENT, type INTEGER, id BLOB UNIQUE ON CONFLICT ROLLBACK, id_str TEXT, name TEXT, bloomfilter BLOB, num_attributes INTEGER DEFAULT 0, sum_weights INTEGER DEFAULT 0, resolution_max_matching_dataobjects INTEGER, resolution_threshold INTEGER, timestamp DATE);"
enum {
	table_nodes_rowid = 0,
	table_nodes_type,
	table_nodes_id,
	table_nodes_id_str,
	table_nodes_name,
	table_nodes_bloomfilter,
	table_nodes_num_attributes,
	table_nodes_sum_weights,
	table_nodes_resolution_max_matching_dataobjects,	// resolution: max number of data object that a node is willing to recieve
	table_nodes_resolution_threshold,					// resolution: relation threshold (only relations with a higher ratio will be considered)
	table_nodes_timestamp
};

//------------------------------------------
#define SQL_CREATE_TABLE_ATTRIBUTES_CMD "CREATE TABLE IF NOT EXISTS " TABLE_ATTRIBUTES " (ROWID INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, value TEXT, UNIQUE (name,value) ON CONFLICT ROLLBACK);"
enum {
	table_attributes_rowid = 0,
	table_attributes_name,
	table_attributes_text
};

//------------------------------------------
#define SQL_CREATE_TABLE_FILTERS_CMD "CREATE TABLE IF NOT EXISTS " TABLE_FILTERS " (ROWID INTEGER PRIMARY KEY, event INTEGER UNIQUE ON CONFLICT ROLLBACK, num_attributes INTEGER DEFAULT 0, timestamp DATE);"
enum {
	table_filters_rowid = 0,
	table_filters_event,
	table_filters_num_attributes,
	table_filters_timestamp
};

//------------------------------------------
#define SQL_CREATE_TABLE_INTERFACES_CMD "CREATE TABLE IF NOT EXISTS " TABLE_INTERFACES " (ROWID INTEGER PRIMARY KEY AUTOINCREMENT, type INTEGER, mac BLOB, mac_str TEXT, node_rowid INTEGER, timestamp DATE, UNIQUE (type,mac) ON CONFLICT ROLLBACK);"
enum {
	table_interfaces_rowid = 0,
	table_interfaces_type,
	table_interfaces_mac,
	table_interfaces_mac_str,
	table_interfaces_node_rowid,
	table_interfaces_timestamp
};

// Triggers to update the timestamp on insert of Dataobjects, Nodes, Interfaces
//------------------------------------------
#define SQL_CREATE_TRIGGER_TABLE_DATAOBJECTS_CMD "CREATE TRIGGER insert_" TABLE_DATAOBJECTS "_timestamp AFTER INSERT ON " TABLE_DATAOBJECTS " BEGIN UPDATE " TABLE_DATAOBJECTS " SET timestamp = STRFTIME('%s', 'NOW') WHERE ROWID = NEW.ROWID; END;"
//------------------------------------------
#define SQL_CREATE_TRIGGER_NODE_TABLE_CDM "CREATE TRIGGER insert_" TABLE_NODES "_timestamp AFTER INSERT ON " TABLE_NODES " BEGIN UPDATE " TABLE_NODES " SET timestamp = STRFTIME('%s', 'NOW') WHERE ROWID = new.ROWID; END;"
//------------------------------------------
#define SQL_CREATE_TRIGGER_FILTER_TABLE_CMD "CREATE TRIGGER insert_" TABLE_FILTERS "_timestamp AFTER INSERT ON " TABLE_FILTERS " BEGIN UPDATE " TABLE_FILTERS " SET timestamp = STRFTIME('%s', 'NOW') WHERE ROWID = NEW.ROWID; END;"
//------------------------------------------
#define SQL_CREATE_TRIGGER_TABLE_INTERFACES_CMD "CREATE TRIGGER insert_" TABLE_INTERFACES "_timestamp AFTER INSERT ON " TABLE_INTERFACES " BEGIN UPDATE " TABLE_INTERFACES " SET timestamp = STRFTIME('%s', 'NOW') WHERE ROWID = NEW.ROWID; END;"

/*
	Link Tables
	Tables to define the linking between Attributes and {Dataobjects,Nodes,Filters}
	and related Triggers: 
	- to remove the linking if a {Dataobject,Node,Filter} is removed
	- to update the timestamp on insert
	- to count the attributes of a {Dataobject,Node,Filter}  (increment on insert, decrement on delete)		
*/

// Dataobject related:
//------------------------------------------
#define SQL_CREATE_DATAOBJECT_TABLE_ATTRIBUTES_CMD "CREATE TABLE " TABLE_MAP_DATAOBJECTS_TO_ATTRIBUTES_VIA_ROWID " (ROWID INTEGER PRIMARY KEY, dataobject_rowid INTEGER, attr_rowid INTEGER, timestamp DATE, UNIQUE (dataobject_rowid,attr_rowid) ON CONFLICT ROLLBACK);"
enum {
	table_map_dataobjects_to_attributes_via_rowid_rowid = 0,
	table_map_dataobjects_to_attributes_via_rowid_dataobject_rowid,
	table_map_dataobjects_to_attributes_via_rowid_attr_rowid,
	table_map_dataobjects_to_attributes_via_rowid_timestamp
};

//------------------------------------------
#define SQL_CREATE_TRIGGER_DEL_DATAOBJECT_CMD "CREATE TRIGGER delete_" TABLE_DATAOBJECTS " AFTER DELETE ON "TABLE_DATAOBJECTS " BEGIN DELETE FROM " TABLE_MAP_DATAOBJECTS_TO_ATTRIBUTES_VIA_ROWID " WHERE dataobject_rowid=old.rowid; END;"
//------------------------------------------
#define SQL_CREATE_TRIGGER_INSERT_DATAOBJECT_ATTRIBUTES_CMD "CREATE TRIGGER insert_" TABLE_MAP_DATAOBJECTS_TO_ATTRIBUTES_VIA_ROWID " AFTER INSERT ON " TABLE_MAP_DATAOBJECTS_TO_ATTRIBUTES_VIA_ROWID " BEGIN UPDATE " TABLE_MAP_DATAOBJECTS_TO_ATTRIBUTES_VIA_ROWID " SET timestamp = STRFTIME('%s', 'NOW') WHERE ROWID = NEW.ROWID; UPDATE " TABLE_DATAOBJECTS " SET num_attributes=num_attributes+1 WHERE rowid = NEW.dataobject_rowid; END;"
//------------------------------------------
#define SQL_CREATE_TRIGGER_DEL_DATAOBJECT_ATTRIBUTES_CMD "CREATE TRIGGER delete_" TABLE_MAP_DATAOBJECTS_TO_ATTRIBUTES_VIA_ROWID " AFTER DELETE ON " TABLE_MAP_DATAOBJECTS_TO_ATTRIBUTES_VIA_ROWID " BEGIN UPDATE " TABLE_DATAOBJECTS " SET num_attributes=num_attributes-1 WHERE rowid = OLD.dataobject_rowid; END;"

// Node related:
//------------------------------------------
#define SQL_CREATE_NODE_TABLE_ATTRIBUTES_CMD "CREATE TABLE " TABLE_MAP_NODES_TO_ATTRIBUTES_VIA_ROWID " (ROWID INTEGER PRIMARY KEY, node_rowid INTEGER, attr_rowid INTEGER, weight INTEGER, timestamp DATE, UNIQUE (node_rowid,attr_rowid) ON CONFLICT ROLLBACK);"
enum {
	table_map_nodes_to_attributes_via_rowid_rowid = 0,
	table_map_nodes_to_attributes_via_rowid_node_rowid,
	table_map_nodes_to_attributes_via_rowid_attr_rowid,
	table_map_nodes_to_attributes_via_rowid_weight,
	table_map_nodes_to_attributes_via_rowid_timestamp
};

//------------------------------------------
#define SQL_CREATE_TRIGGER_DEL_NODE_CMD "CREATE TRIGGER delete_" TABLE_NODES " AFTER DELETE ON " TABLE_NODES " BEGIN DELETE FROM " TABLE_MAP_NODES_TO_ATTRIBUTES_VIA_ROWID " WHERE node_rowid=old.rowid; DELETE FROM " TABLE_INTERFACES " WHERE node_rowid=old.rowid; END;"
//------------------------------------------
#define SQL_CREATE_TRIGGER_INSERT_NODE_ATTRIBUTES_CMD "CREATE TRIGGER insert_" TABLE_MAP_NODES_TO_ATTRIBUTES_VIA_ROWID " AFTER INSERT ON " TABLE_MAP_NODES_TO_ATTRIBUTES_VIA_ROWID " BEGIN UPDATE " TABLE_MAP_NODES_TO_ATTRIBUTES_VIA_ROWID " SET timestamp = STRFTIME('%s', 'NOW') WHERE ROWID = NEW.ROWID; UPDATE " TABLE_NODES " SET num_attributes=num_attributes+1 , sum_weights=sum_weights+NEW.weight WHERE rowid = NEW.node_rowid; END;"
//------------------------------------------
#define SQL_CREATE_TRIGGER_DEL_NODE_ATTRIBUTES_CMD "CREATE TRIGGER delete_" TABLE_MAP_NODES_TO_ATTRIBUTES_VIA_ROWID " AFTER DELETE ON " TABLE_MAP_NODES_TO_ATTRIBUTES_VIA_ROWID " BEGIN UPDATE " TABLE_NODES " SET num_attributes=num_attributes-1, sum_weights=sum_weights-OLD.weight WHERE rowid = OLD.node_rowid; END;"

// Filter related:
//------------------------------------------
#define SQL_CREATE_FILTER_TABLE_ATTRIBUTES_CMD "CREATE TABLE " TABLE_MAP_FILTERS_TO_ATTRIBUTES_VIA_ROWID " (ROWID INTEGER PRIMARY KEY, filter_rowid INTEGER, attr_rowid INTEGER, weight INTEGER, timestamp DATE, UNIQUE (filter_rowid,attr_rowid) ON CONFLICT ROLLBACK);"
enum {
	table_map_filters_to_attributes_via_rowid_rowid = 0,
	table_map_filters_to_attributes_via_rowid_filter_rowid,
	table_map_filters_to_attributes_via_rowid_attr_rowid,
	table_map_filters_to_attributes_via_rowid_weight,
	table_map_filters_to_attributes_via_rowid_timestamp
};

//------------------------------------------
#define SQL_CREATE_TRIGGER_DEL_FILTER_CMD "CREATE TRIGGER delete_" TABLE_FILTERS " AFTER DELETE ON " TABLE_FILTERS " BEGIN DELETE FROM " TABLE_MAP_FILTERS_TO_ATTRIBUTES_VIA_ROWID " WHERE filter_rowid=old.rowid; END;"
//------------------------------------------
#define SQL_CREATE_TRIGGER_INSERT_FILTER_ATTRIBUTES_CMD "CREATE TRIGGER insert_" TABLE_MAP_FILTERS_TO_ATTRIBUTES_VIA_ROWID " AFTER INSERT ON " TABLE_MAP_FILTERS_TO_ATTRIBUTES_VIA_ROWID " BEGIN UPDATE " TABLE_MAP_FILTERS_TO_ATTRIBUTES_VIA_ROWID " SET timestamp = STRFTIME('%s', 'NOW') WHERE ROWID = NEW.ROWID; UPDATE " TABLE_FILTERS " SET num_attributes=num_attributes+1 WHERE rowid = NEW.filter_rowid; END;"
//------------------------------------------
#define SQL_CREATE_TRIGGER_DEL_FILTER_ATTRIBUTES_CMD "CREATE TRIGGER delete_" TABLE_MAP_FILTERS_TO_ATTRIBUTES_VIA_ROWID " AFTER DELETE ON " TABLE_MAP_FILTERS_TO_ATTRIBUTES_VIA_ROWID " BEGIN UPDATE " TABLE_FILTERS " SET num_attributes=num_attributes-1 WHERE rowid = OLD.filter_rowid; END;"


/* 
	Views to support matching
 
	Matching element x from type X to elemetes y of type Y is done the following way:
	- limit the link table of X to the attributes of x  (VIEW_LIMITED_X_ATTRIBUTES)
	  to keep the matching limited to relevant rows.
	- join the limited link table with the link table of type Y 
	  to get a list of all matching attributes linked to type Y
    - group on y 
	  to get the list with one entry per y that has at least one attribute in common with x
	  and to count the number of matches per y (with count(*))
	- In a second step, the ratio of matched attributes is calculated
	  by dividing the number of matches with the number of attributes per x and y.
 	
	Matching of filters is slightly different:
	- support of wildcards:
	  VIEW_SIMILAR_ATTRIBUTES is a view that links all attributes of a filter with 
	  similar attributes. Similar attributes are either the same attribute, or an 
	  an attribute with the same name but value ATTR_WILDCARD (defined in attributes.h)
 
	Comment: 
	wildcards for matching between Dataobjects and Nodes is not supported anymore
	due to performance issues. This might be investigated again for the future.
	
*/

// limit the dataobject attributes link table 
//------------------------------------------
#define SQL_CREATE_VIEW_LIMITED_DATAOBJECT_ATTRIBUTES_CMD "CREATE VIEW " VIEW_MAP_DATAOBJECTS_TO_ATTRIBUTES_VIA_ROWID_DYNAMIC " AS SELECT * FROM " TABLE_MAP_DATAOBJECTS_TO_ATTRIBUTES_VIA_ROWID ";"
enum {
	view_limited_dataobject_attributes_rowid = 0,
	view_limited_dataobject_attributes_dataobject_rowid,
	view_limited_dataobject_attributes_attr_rowid,
	view_limited_dataobject_attributes_timestamp
};
#define SQL_DROP_VIEW_LIMITED_DATAOBJECT_ATTRIBUTES_CMD "DROP VIEW " VIEW_MAP_DATAOBJECTS_TO_ATTRIBUTES_VIA_ROWID_DYNAMIC ";"
// Note: VIEW_MAP_DATAOBJECTS_TO_ATTRIBUTES_VIA_ROWID_DYNAMIC gets dynamically replaced during matching

// limit the node attributes link table 
//------------------------------------------
#define SQL_CREATE_VIEW_LIMITED_NODE_ATTRIBUTES_CMD "CREATE VIEW " VIEW_MAP_NODES_TO_ATTRIBUTES_VIA_ROWID_DYNAMIC " AS SELECT * FROM " TABLE_MAP_NODES_TO_ATTRIBUTES_VIA_ROWID ";"
enum {
	view_map_nodes_to_attributes_via_rowid_dynamic_rowid = 0,
	view_map_nodes_to_attributes_via_rowid_dynamic_node_rowid,
	view_map_nodes_to_attributes_via_rowid_dynamic_attr_rowid,
	view_map_nodes_to_attributes_via_rowid_dynamic_weight,
	view_map_nodes_to_attributes_via_rowid_dynamic_timestamp
};
#define SQL_DROP_VIEW_LIMITED_NODE_ATTRIBUTES_CMD "DROP VIEW " VIEW_MAP_NODES_TO_ATTRIBUTES_VIA_ROWID_DYNAMIC ";"
// Note: VIEW_MAP_NODES_TO_ATTRIBUTES_VIA_ROWID_DYNAMIC gets dynamically replaced during matching

// Matching Filter > Dataobjects 
//------------------------------------------
#define SQL_CREATE_VIEW_FILTERRELEVANT_ATTRIBUTES_CMD "CREATE VIEW " VIEW_FILTERRELEVANT_ATTRIBUTES " AS SELECT a.* FROM " TABLE_MAP_FILTERS_TO_ATTRIBUTES_VIA_ROWID " as fa LEFT JOIN " TABLE_ATTRIBUTES "  as a ON fa.attr_rowid = a.rowid;"
enum {
	// These should be identical (because of above definition of the view) to 
	// the table_attributes enums
	view_filterrelevant_attributes_rowid = 0,
	view_filterrelevant_attributes_name,
	view_filterrelevant_attributes_value
};

//------------------------------------------
#define SQL_CREATE_VIEW_SIMILAR_ATTRIBUTES_CMD "CREATE VIEW " VIEW_SIMILAR_ATTRIBUTES " AS SELECT a.rowid as a_rowid, b.rowid as b_rowid, b.name as name, b.value as value FROM " VIEW_FILTERRELEVANT_ATTRIBUTES " as a INNER JOIN " TABLE_ATTRIBUTES " as b ON ((a.name=b.name) AND (a.value=b.value OR a.value='" ATTR_WILDCARD "'));"
enum {
	view_similar_attributes_a_rowid = 0,
	view_similar_attributes_b_rowid,
	view_similar_attributes_name,
	view_similar_attributes_value
};

//------------------------------------------
#define SQL_CREATE_VIEW_FILTER_MATCH_DATAOBJECT_CMD "CREATE VIEW " VIEW_MATCH_FILTERS_AND_DATAOBJECTS " AS SELECT f.rowid as filter_rowid, f.event as filter_event, count(*) as fmcount, f.num_attributes as filter_num_attributes, da.dataobject_rowid as dataobject_rowid FROM VIEW_MAP_DATAOBJECTS_TO_ATTRIBUTES_VIA_ROWID_DYNAMIC as da INNER JOIN VIEW_SIMILAR_ATTRIBUTES as a ON da.attr_rowid=a.b_rowid LEFT JOIN TABLE_MAP_FILTERS_TO_ATTRIBUTES_VIA_ROWID as fa ON fa.attr_rowid=a.a_rowid LEFT JOIN TABLE_FILTERS as f ON fa.filter_rowid=f.rowid GROUP by f.rowid, da.dataobject_rowid;"
enum {
	view_match_filters_and_dataobjects_filter_rowid = 0,
	view_match_filters_and_dataobjects_filter_event,
	view_match_filters_and_dataobjects_fmcount,
	view_match_filters_and_dataobjects_filter_num_attributes,
	view_match_filters_and_dataobjects_dataobject_rowid
};

//------------------------------------------
#define SQL_CREATE_VIEW_FILTER_MATCH_DATAOBJECT_RATED_CMD "CREATE VIEW " VIEW_MATCH_FILTERS_AND_DATAOBJECTS_AS_RATIO " AS SELECT filter_rowid, filter_event, 100*fmcount/filter_num_attributes as ratio, dataobject_rowid FROM " VIEW_MATCH_FILTERS_AND_DATAOBJECTS " ORDER BY ratio desc, filter_num_attributes desc;"
enum {
	view_match_filters_and_dataobjects_as_ratio_filter_rowid = 0,
	view_match_filters_and_dataobjects_as_ratio_filter_event,
	view_match_filters_and_dataobjects_as_ratio_ratio,
	view_match_filters_and_dataobjects_as_ratio_dataobject_rowid
};


// Matching Filter > Nodes
//------------------------------------------
#define SQL_CREATE_VIEW_FILTER_MATCH_NODE_CMD "CREATE VIEW " VIEW_MATCH_FILTERS_AND_NODES " AS SELECT f.rowid as filter_rowid, f.event as filter_event, count(*) as fmcount, n.rowid as node_rowid, n.rowid as node_rowid, n.timestamp as node_timestamp from " TABLE_MAP_FILTERS_TO_ATTRIBUTES_VIA_ROWID " as fa LEFT JOIN " VIEW_SIMILAR_ATTRIBUTES " as a ON fa.attr_rowid=a.b_rowid INNER JOIN " TABLE_MAP_NODES_TO_ATTRIBUTES_VIA_ROWID " as na ON na.attr_rowid=a.a_rowid LEFT JOIN " TABLE_NODES " as n ON n.rowid=na.node_rowid LEFT JOIN " TABLE_FILTERS " as f ON fa.filter_rowid=f.rowid group by f.rowid, n.rowid;"
enum {
	view_match_filters_and_nodes_filter_rowid = 0,
	view_match_filters_and_nodes_filter_event,
	view_match_filters_and_nodes_fmcount,
	view_match_filters_and_nodes_node_rowid,
	view_match_filters_and_nodes_node_rowid2,
	view_match_filters_and_nodes_node_timestamp
};

//------------------------------------------
#define SQL_CREATE_VIEW_FILTER_MATCH_NODE_RATED_CMD "CREATE VIEW " VIEW_MATCH_FILTERS_AND_NODES_AS_RATIO " AS SELECT 100*fmcount/f.num_attributes as ratio, fm.* FROM " VIEW_MATCH_FILTERS_AND_NODES " as fm LEFT JOIN " TABLE_FILTERS " as f ON fm.filter_rowid=f.rowid ORDER BY ratio desc, fmcount desc;"
enum {
	view_match_filters_and_nodes_as_ratio_ratio = 0,
	view_match_filters_and_nodes_as_ratio_filter_rowid,
	view_match_filters_and_nodes_as_ratio_filter_event,
	view_match_filters_and_nodes_as_ratio_fmcount,
	view_match_filters_and_nodes_as_ratio_node_rowid,
	view_match_filters_and_nodes_as_ratio_node_rowid2,
	view_match_filters_and_nodes_as_ratio_node_timestamp
};


// Matching Dataobject > Nodes
//------------------------------------------
#define SQL_CREATE_VIEW_DATAOBJECT_NODE_MATCH_CMD "CREATE VIEW " VIEW_MATCH_DATAOBJECTS_AND_NODES " AS SELECT da.dataobject_rowid as dataobject_rowid, na.node_rowid as node_rowid, count(*) as mcount, sum(na.weight) as weight, min(na.weight)=" STRINGIFY(ATTR_WEIGHT_NO_MATCH) " as dataobject_not_match, da.timestamp as dataobject_timestamp FROM " VIEW_MAP_DATAOBJECTS_TO_ATTRIBUTES_VIA_ROWID_DYNAMIC " as da INNER JOIN " TABLE_MAP_NODES_TO_ATTRIBUTES_VIA_ROWID " as na ON na.attr_rowid=da.attr_rowid GROUP by da.dataobject_rowid, na.node_rowid;"
enum {
	view_match_dataobjects_and_nodes_dataobject_rowid = 0,
	view_match_dataobjects_and_nodes_node_rowid,
	view_match_dataobjects_and_nodes_mcount,
	view_match_dataobjects_and_nodes_weight,
	view_match_dataobjects_and_nodes_dataobject_not_match,
	view_match_dataobjects_and_nodes_dataobject_timestamp
};

//------------------------------------------
#define SQL_CREATE_VIEW_DATAOBJECT_NODE_MATCH_CMD_RATED_CMD "CREATE VIEW " VIEW_MATCH_DATAOBJECTS_AND_NODES_AS_RATIO " AS SELECT 100*weight/n.sum_weights as ratio, m.* FROM " VIEW_MATCH_DATAOBJECTS_AND_NODES " as m LEFT JOIN " TABLE_NODES " as n ON m.node_rowid=n.rowid ORDER BY ratio desc, mcount desc;"
// using count: #define SQL_CREATE_VIEW_DATAOBJECT_NODE_MATCH_CMD_RATED_CMD "CREATE VIEW " VIEW_MATCH_DATAOBJECTS_AND_NODES_AS_RATIO " AS SELECT 100*mcount/dacount as dataobject_ratio, 100*mcount/nacount as node_ratio, m.* FROM " VIEW_MATCH_DATAOBJECTS_AND_NODES " as m LEFT JOIN " VIEW_NODE_ATTRIBUTE_COUNT " as cn ON m.node_rowid=cn.node_rowid LEFT JOIN " VIEW_DATAOBJECT_ATTRIBUTE_COUNT " as cd ON m.dataobject_rowid=cd.dataobject_rowid ORDER BY 100*mcount/nacount+100*mcount/dacount desc, mcount desc;"
enum {
	view_match_dataobjects_and_nodes_as_ratio_ratio = 0,
	view_match_dataobjects_and_nodes_as_ratio_dataobject_rowid,
	view_match_dataobjects_and_nodes_as_ratio_node_rowid,
	view_match_dataobjects_and_nodes_as_ratio_mcount,
	view_match_dataobjects_and_nodes_as_ratio_weight,
	view_match_dataobjects_and_nodes_as_ratio_dataobject_not_match,
	view_match_dataobjects_and_nodes_as_ratio_dataobject_timestamp
};

// Matching Node > Dataobjects
//------------------------------------------
#define SQL_CREATE_VIEW_NODE_DATAOBJECT_MATCH_CMD "CREATE VIEW " VIEW_MATCH_NODES_AND_DATAOBJECTS " AS SELECT da.dataobject_rowid as dataobject_rowid, na.node_rowid as node_rowid, count(*) as mcount, sum(na.weight) as weight, min(na.weight)=" STRINGIFY(ATTR_WEIGHT_NO_MATCH) " as dataobject_not_match, da.timestamp as dataobject_timestamp FROM " VIEW_MAP_NODES_TO_ATTRIBUTES_VIA_ROWID_DYNAMIC " as na INNER JOIN " TABLE_MAP_DATAOBJECTS_TO_ATTRIBUTES_VIA_ROWID " as da ON na.attr_rowid=da.attr_rowid GROUP by na.node_rowid, da.dataobject_rowid;"
enum {
	view_match_nodes_and_dataobjects_dataobject_rowid = 0,
	view_match_nodes_and_dataobjects_node_rowid,
	view_match_nodes_and_dataobjects_mcount,
	view_match_nodes_and_dataobjects_weight,
	view_match_nodes_and_dataobjects_dataobject_not_match,
	view_match_nodes_and_dataobjects_dataobject_timestamp
};

//------------------------------------------
#define SQL_CREATE_VIEW_NODE_DATAOBJECT_MATCH_CMD_RATED_CMD "CREATE VIEW " VIEW_MATCH_NODES_AND_DATAOBJECTS_AS_RATIO " AS SELECT 100*weight/n.sum_weights as ratio, m.* FROM " VIEW_MATCH_NODES_AND_DATAOBJECTS " as m LEFT JOIN " TABLE_NODES " as n ON m.node_rowid=n.rowid LEFT JOIN " TABLE_DATAOBJECTS " as d ON m.dataobject_rowid=d.rowid WHERE dataobject_not_match=0 ORDER BY ratio desc, mcount desc, d.timestamp desc;"
// using count: #define SQL_CREATE_VIEW_NODE_DATAOBJECT_MATCH_CMD_RATED_CMD "CREATE VIEW " VIEW_MATCH_NODES_AND_DATAOBJECTS_AS_RATIO " AS SELECT 100*mcount/dacount as dataobject_ratio, 100*mcount/nacount as node_ratio, m.* FROM " VIEW_MATCH_NODES_AND_DATAOBJECTS " as m LEFT JOIN " VIEW_NODE_ATTRIBUTE_COUNT " as cn ON m.node_rowid=cn.node_rowid LEFT JOIN " VIEW_DATAOBJECT_ATTRIBUTE_COUNT " as cd ON m.dataobject_rowid=cd.dataobject_rowid ORDER BY 100*mcount/nacount+100*mcount/dacount desc, mcount desc;"
enum {
	view_match_nodes_and_dataobjects_rated_ratio = 0,
	view_match_nodes_and_dataobjects_rated_dataobject_rowid,
	view_match_nodes_and_dataobjects_rated_node_rowid,
	view_match_nodes_and_dataobjects_rated_mcount,
	view_match_nodes_and_dataobjects_rated_weight,
	view_match_nodes_and_dataobjects_rated_dataobject_not_match,
	view_match_nodes_and_dataobjects_rated_dataobject_timestamp
};

/*
	convenience views to display name value pairs of {Dataobject,Node} attributes
*/
//------------------------------------------
#define SQL_CREATE_VIEW_DATAOBJECT_ATTRIBUTES_AS_NAMEVALUE_CMD "CREATE VIEW " VIEW_DATAOBJECT_ATTRIBUTES_AS_NAMEVALUE " AS select d.rowid as dataobject_rowid, d.id as doid, a.name as name, a.value as value from " TABLE_DATAOBJECTS " as d LEFT JOIN " TABLE_MAP_DATAOBJECTS_TO_ATTRIBUTES_VIA_ROWID " as da ON d.rowid=da.dataobject_rowid LEFT JOIN " TABLE_ATTRIBUTES " as a ON da.attr_rowid=a.rowid;"
enum {
	view_dataobjects_attributes_as_namevalue_dataobject_rowid = 0,
	view_dataobjects_attributes_as_namevalue_doid,
	view_dataobjects_attributes_as_namevalue_name,
	view_dataobjects_attributes_as_namevalue_value
};

//------------------------------------------
#define SQL_CREATE_VIEW_NODE_ATTRIBUTES_AS_NAMEVALUE_CMD "CREATE VIEW " VIEW_NODE_ATTRIBUTES_AS_NAMEVALUE " AS select n.rowid as node_rowid, n.id as nodeid, a.name as name, a.value as value from " TABLE_NODES " as n LEFT JOIN " TABLE_MAP_NODES_TO_ATTRIBUTES_VIA_ROWID " as na ON n.rowid=na.node_rowid LEFT JOIN " TABLE_ATTRIBUTES " as a ON na.attr_rowid=a.rowid;"
enum {
	view_node_attributes_as_namevalue_node_rowid = 0,
	view_node_attributes_as_namevalue_nodeid,
	view_node_attributes_as_namevalue_name,
	view_node_attributes_as_namevalue_value
};

/*
	Repository Table
 
*/
#define SQL_CREATE_TABLE_REPOSITORY_CMD "CREATE TABLE " TABLE_REPOSITORY " (ROWID INTEGER PRIMARY KEY AUTOINCREMENT, authority TEXT, key TEXT, value TEXT);"
enum {
	table_repository_rowid = 0,
	table_repository_authority,
	table_repository_key,
	table_repository_value
};



/*
	indexing
 
	of all columns used to search or join tables. 
*/
//------------------------------------------
#define SQL_INDEX_DATAOBJECTS_CMD "CREATE INDEX index_dataobjects ON " TABLE_DATAOBJECTS " (id);"
//------------------------------------------
#define SQL_INDEX_ATTRIBUTES_CMD "CREATE INDEX index_attributes_name ON " TABLE_ATTRIBUTES " (name); CREATE INDEX index_attributes_value ON " TABLE_ATTRIBUTES " (value);"
//------------------------------------------
#define SQL_INDEX_NODES_CMD "CREATE INDEX index_Nodes ON " TABLE_NODES " (id);"
//------------------------------------------
#define SQL_INDEX_DATAOBJECT_ATTRS_CMD "CREATE INDEX index_dataobjectAttributes_attr ON " TABLE_MAP_DATAOBJECTS_TO_ATTRIBUTES_VIA_ROWID " (attr_rowid); CREATE INDEX index_dataobjectAttributes_dataobject ON " TABLE_MAP_DATAOBJECTS_TO_ATTRIBUTES_VIA_ROWID " (dataobject_rowid);"
//------------------------------------------
#define SQL_INDEX_NODE_ATTRS_CMD "CREATE INDEX index_nodeAttributes_attr ON " TABLE_MAP_NODES_TO_ATTRIBUTES_VIA_ROWID " (attr_rowid); CREATE INDEX index_nodeAttributes_node ON " TABLE_MAP_NODES_TO_ATTRIBUTES_VIA_ROWID " (node_rowid);"


static const char *tbl_cmds[] = {
	SQL_CREATE_TABLE_DATAOBJECTS_CMD,
	SQL_CREATE_TRIGGER_TABLE_DATAOBJECTS_CMD,
	SQL_CREATE_TABLE_INTERFACES_CMD,
	SQL_CREATE_TRIGGER_TABLE_INTERFACES_CMD,
	SQL_CREATE_TABLE_NODES_CMD,
	SQL_CREATE_TRIGGER_NODE_TABLE_CDM,
	SQL_CREATE_TABLE_ATTRIBUTES_CMD,
	SQL_CREATE_TABLE_FILTERS_CMD,
	SQL_CREATE_TRIGGER_FILTER_TABLE_CMD,
	SQL_CREATE_DATAOBJECT_TABLE_ATTRIBUTES_CMD,
	SQL_CREATE_TRIGGER_INSERT_DATAOBJECT_ATTRIBUTES_CMD,
	SQL_CREATE_TRIGGER_DEL_DATAOBJECT_ATTRIBUTES_CMD,
	SQL_CREATE_TRIGGER_DEL_DATAOBJECT_CMD,
	SQL_CREATE_NODE_TABLE_ATTRIBUTES_CMD,
	SQL_CREATE_TRIGGER_INSERT_NODE_ATTRIBUTES_CMD,
	SQL_CREATE_TRIGGER_DEL_NODE_ATTRIBUTES_CMD,
	SQL_CREATE_TRIGGER_DEL_NODE_CMD,
	SQL_CREATE_FILTER_TABLE_ATTRIBUTES_CMD,
	SQL_CREATE_TRIGGER_INSERT_FILTER_ATTRIBUTES_CMD,
	SQL_CREATE_TRIGGER_DEL_FILTER_ATTRIBUTES_CMD,
	SQL_CREATE_TRIGGER_DEL_FILTER_CMD,
	SQL_CREATE_VIEW_LIMITED_DATAOBJECT_ATTRIBUTES_CMD,
	SQL_CREATE_VIEW_FILTERRELEVANT_ATTRIBUTES_CMD,
	SQL_CREATE_VIEW_SIMILAR_ATTRIBUTES_CMD,
	SQL_CREATE_VIEW_FILTER_MATCH_DATAOBJECT_CMD,
	SQL_CREATE_VIEW_FILTER_MATCH_DATAOBJECT_RATED_CMD,
	SQL_CREATE_VIEW_FILTER_MATCH_NODE_CMD,
	SQL_CREATE_VIEW_FILTER_MATCH_NODE_RATED_CMD,
	SQL_CREATE_VIEW_DATAOBJECT_ATTRIBUTES_AS_NAMEVALUE_CMD,
	SQL_CREATE_VIEW_NODE_ATTRIBUTES_AS_NAMEVALUE_CMD,
	SQL_CREATE_VIEW_DATAOBJECT_NODE_MATCH_CMD,
	SQL_CREATE_VIEW_LIMITED_NODE_ATTRIBUTES_CMD,
	SQL_CREATE_VIEW_NODE_DATAOBJECT_MATCH_CMD,
	SQL_CREATE_VIEW_DATAOBJECT_NODE_MATCH_CMD_RATED_CMD,
	SQL_CREATE_VIEW_NODE_DATAOBJECT_MATCH_CMD_RATED_CMD,
	SQL_INDEX_DATAOBJECTS_CMD,
	SQL_INDEX_ATTRIBUTES_CMD,
	SQL_INDEX_NODES_CMD,
	SQL_INDEX_DATAOBJECT_ATTRS_CMD,
	SQL_INDEX_NODE_ATTRS_CMD,
	SQL_CREATE_TABLE_REPOSITORY_CMD,
	NULL
};

#define SQL_MAX_CMD_SIZE 8000 // This is set more or less by 

#define SQL_DELETE_FILTERS "DELETE FROM " TABLE_FILTERS ";"

static char sqlcmd[SQL_MAX_CMD_SIZE];

static inline string SQL_INSERT_DATAOBJECT_CMD(const char *xml, const sqlite_int64 ifaceRowId, const DataObjectRef dObj)
{
	string cmd;
	
	/*
		WARNING:
		
		This code, by inserting the xml header verbatim into the actual SQL 
		query suffers from a risk of SQL injection. See the haggle trac page,
		ticket #139.
	*/
	int ret = stringprintf(cmd, "INSERT INTO %s (id,xmlhdr,filepath,filename,datalen,signaturestatus,signee,createtime,receivetime,rxtime,source_iface_rowid) VALUES(?,\'%s\',\'%s\',\'%s\',%lu,%lu,\'%s\'," INT64_FORMAT "," INT64_FORMAT ",%lu," INT64_FORMAT ");", 
                           TABLE_DATAOBJECTS, xml, dObj->getFilePath().c_str(), dObj->getFileName().c_str(), 
                           (dObj->getDynamicDataLen() ? -1 : (unsigned long) dObj->getDataLen()), 
			       dObj->getSignatureStatus(), dObj->getSignee().c_str(),
                           dObj->getCreateTime().getTimeAsMilliSeconds(), dObj->getReceiveTime().getTimeAsMilliSeconds(), 
                           dObj->getRxTime(), ifaceRowId);

        if (ret < 0) {
                HAGGLE_ERR("SQL_INSERT_DATAOBJECT_CMD: stringprintf failed\n");
        } else if (ret == SQL_MAX_CMD_SIZE) {
                HAGGLE_ERR("SQL_INSERT_DATAOBJECT_CMD: command too large\n");
                
        }
	return cmd;
}
static inline char *SQL_DELETE_DATAOBJECT_CMD()
{
	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "DELETE FROM %s WHERE id = ?;", TABLE_DATAOBJECTS);
	
	return sqlcmd;
}

static inline char *SQL_AGE_DATAOBJECT_CMD(const Timeval& minimumAge)
{
	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "SELECT * FROM " TABLE_DATAOBJECTS " WHERE rowid NOT IN (SELECT dataobject_rowid FROM " VIEW_MATCH_FILTERS_AND_DATAOBJECTS_AS_RATIO " ) AND timestamp < strftime('%s', 'now','-%ld seconds');", "%s", minimumAge.getSeconds());
	
	return sqlcmd;
}


#define SQL_FIND_DATAOBJECT_CMD "SELECT * FROM " TABLE_DATAOBJECTS " WHERE id=?;"

static inline char *SQL_INSERT_DATAOBJECT_ATTR_CMD(const sqlite_int64 dataobject_rowid, const sqlite_int64 attr_rowid)
{
	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "INSERT INTO " TABLE_MAP_DATAOBJECTS_TO_ATTRIBUTES_VIA_ROWID " (dataobject_rowid,attr_rowid) VALUES (" INT64_FORMAT "," INT64_FORMAT ");", dataobject_rowid, attr_rowid);

	return sqlcmd;
}

// -- ATTRIBUTE
static inline char *SQL_INSERT_ATTR_CMD(const Attribute * attr)
{
	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "INSERT INTO " TABLE_ATTRIBUTES " (name,value) VALUES(\'%s\',\'%s\');", attr->getName().c_str(), attr->getValue().c_str());

	return sqlcmd;
}

static inline char *SQL_FIND_ATTR_CMD(const Attribute * attr)
{
	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "SELECT ROWID FROM " TABLE_ATTRIBUTES " WHERE (name=\'%s\' AND value=\'%s\');", attr->getName().c_str(), attr->getValue().c_str());

	return sqlcmd;
}

static inline char *SQL_ATTRS_FROM_NODE_ROWID_CMD(const sqlite_int64 node_rowid)
{
	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "SELECT * FROM " TABLE_MAP_NODES_TO_ATTRIBUTES_VIA_ROWID " WHERE node_rowid=" INT64_FORMAT ";", node_rowid);

	return sqlcmd;
}

static inline char *SQL_ATTRS_FROM_DATAOBJECT_ROWID_CMD(const sqlite_int64 dataobject_rowid)
{
	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "SELECT * FROM " TABLE_MAP_DATAOBJECTS_TO_ATTRIBUTES_VIA_ROWID " WHERE dataobject_rowid=" INT64_FORMAT ";", dataobject_rowid);

	return sqlcmd;
}
static inline char *SQL_ATTR_FROM_ROWID_CMD(const sqlite_int64 attr_rowid, const sqlite_int64 node_rowid)
{
	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "SELECT a.rowid, a.name, a.value, w.weight FROM " TABLE_ATTRIBUTES " as a LEFT JOIN " TABLE_MAP_NODES_TO_ATTRIBUTES_VIA_ROWID " as w ON a.rowid=w.attr_rowid WHERE a.rowid=" INT64_FORMAT " AND w.node_rowid=" INT64_FORMAT ";", attr_rowid, node_rowid);

	return sqlcmd;
}
enum {
	sql_attr_from_rowid_cmd_rowid	=	0,
	sql_attr_from_rowid_cmd_name,
	sql_attr_from_rowid_cmd_value,
	sql_attr_from_rowid_cmd_weight
};

static inline char *SQL_DATAOBJECT_FROM_ROWID_CMD(const sqlite_int64 do_rowid)
{
	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "SELECT * FROM %s WHERE rowid=" INT64_FORMAT ";", TABLE_DATAOBJECTS, do_rowid);

	return sqlcmd;
}

// -- INTERFACE
static inline char *SQL_INSERT_IFACE_CMD(const int type, const char *mac_str, const sqlite_int64 node_rowid)
{
	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "INSERT INTO %s (type,mac,mac_str,node_rowid) VALUES(%d,?,\'%s\'," INT64_FORMAT ");", TABLE_INTERFACES,  type, mac_str, node_rowid);

	return sqlcmd;
}

static inline char *SQL_IFACES_FROM_NODE_ROWID_CMD(const sqlite_int64 node_rowid)
{
	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "SELECT * FROM %s WHERE node_rowid=" INT64_FORMAT ";", TABLE_INTERFACES, node_rowid);

	return sqlcmd;
}

static inline char *SQL_IFACE_FROM_ROWID_CMD(const sqlite_int64 iface_rowid)
{
	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "SELECT * FROM %s WHERE rowid=" INT64_FORMAT ";", TABLE_INTERFACES, iface_rowid);

	return sqlcmd;
}

#define SQL_FIND_IFACE_CMD  "SELECT * FROM " TABLE_INTERFACES " WHERE (mac=?);"


// -- NODE

static inline char *SQL_INSERT_NODE_CMD(const int type, const char *idStr, const char *name, const unsigned int maxmatchingdos, const unsigned int threshold)
{
	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "INSERT INTO " TABLE_NODES " (type,id,id_str,name,bloomfilter,resolution_max_matching_dataobjects,resolution_threshold) VALUES (%d,?,\'%s\',\'%s\',?,%d,%d);", type, idStr, name, maxmatchingdos, threshold);

	return sqlcmd;
}
static inline char *SQL_DELETE_NODE_CMD()
{
	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "DELETE FROM %s WHERE id = ?;", TABLE_NODES);
	
	return sqlcmd;
}
static inline char *SQL_INSERT_NODE_ATTR_CMD(const sqlite_int64 node_rowid, const sqlite_int64 attr_rowid, const sqlite_int64 weight)
{
	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "INSERT INTO " TABLE_MAP_NODES_TO_ATTRIBUTES_VIA_ROWID " (node_rowid,attr_rowid,weight) VALUES (" INT64_FORMAT "," INT64_FORMAT "," INT64_FORMAT ");", 
			 node_rowid, attr_rowid, weight);

	return sqlcmd;
}
static inline char *SQL_NODE_FROM_ROWID_CMD(const sqlite_int64 node_rowid)
{
	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "SELECT * from %s WHERE rowid=" INT64_FORMAT ";", TABLE_NODES, node_rowid);

	return sqlcmd;
}

static inline char *SQL_NODE_BY_TYPE_CMD(NodeType_t type)
{
	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "SELECT rowid from %s WHERE type=%d;", TABLE_NODES, type);

	return sqlcmd;
}
enum {
	sql_node_by_type_cmd_rowid	=	0
};

#define SQL_NODE_FROM_ID_CMD "SELECT * FROM " TABLE_NODES " WHERE id=?;"


// -- FILTER
static inline char *SQL_INSERT_FILTER_CMD(const long eventType)
{
	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "INSERT INTO " TABLE_FILTERS " (event) VALUES (%ld);", eventType);

	return sqlcmd;
}
static inline char *SQL_DELETE_FILTER_CMD(const long eventType)
{
	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "DELETE FROM " TABLE_FILTERS " WHERE event = %ld;", eventType);

	return sqlcmd;
}
static inline char *SQL_INSERT_FILTER_ATTR_CMD(const sqlite_int64 filter_rowid, const sqlite_int64 attr_rowid, const sqlite_int64 attr_weight)
{
	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "INSERT INTO " TABLE_MAP_FILTERS_TO_ATTRIBUTES_VIA_ROWID " (filter_rowid,attr_rowid,weight) VALUES (" INT64_FORMAT "," INT64_FORMAT "," INT64_FORMAT ");", filter_rowid, attr_rowid, attr_weight);

	return sqlcmd;
}

#define SQL_FILTER_MATCH_DATAOBJECT_ALL_CMD "SELECT * FROM " VIEW_MATCH_FILTERS_AND_DATAOBJECTS_AS_RATIO " WHERE ratio>0;"
#define SQL_FILTER_MATCH_NODE_ALL_CMD "SELECT * FROM " VIEW_MATCH_FILTERS_AND_NODES_AS_RATIO " WHERE filter_id=? and ratio>0;"

static inline char *SQL_FILTER_MATCH_ALL_CMD(long filter_event)
{
	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "SELECT * FROM " VIEW_MATCH_FILTERS_AND_DATAOBJECTS_AS_RATIO " WHERE filter_event=%ld and ratio>0;", filter_event);

	return sqlcmd;
}

static inline char *SQL_FILTER_MATCH_DATAOBJECT_CMD(const sqlite_int64 filter_rowid)
{
	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "SELECT * FROM " VIEW_MATCH_FILTERS_AND_DATAOBJECTS_AS_RATIO " WHERE filter_rowid=" INT64_FORMAT " AND ratio>0 ORDER BY ratio, dataobject_rowid;", filter_rowid);

	return sqlcmd;
}

static inline char *SQL_FILTER_MATCH_NODE_CMD(const sqlite_int64 filter_rowid)
{
	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "SELECT * FROM " VIEW_MATCH_FILTERS_AND_NODES_AS_RATIO " WHERE filter_rowid=" INT64_FORMAT " AND ratio>0;", filter_rowid);

	return sqlcmd;
}

static inline char *SQL_DEL_FILTER_CMD(const sqlite_int64 filter_rowid)
{
	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "DELETE FROM %s WHERE rowid=" INT64_FORMAT ";", TABLE_FILTERS, filter_rowid);

	return sqlcmd;
}

#define SQL_BEGIN_TRANSACTION_CMD "BEGIN TRANSACTION;"
#define SQL_END_TRANSACTION_CMD "END TRANSACTION;"



/* ========================================================= */
/* Commands to create objects from datastore                 */
/* ========================================================= */

// static Attribute *getAttrFromRowId(sqlite3 *db, const sqlite_int64 attr_rowid, const char* weight_table);

DataObject *SQLDataStore::createDataObject(sqlite3_stmt * stmt)
{
	DataObject *dObj = NULL;

#if HAVE_EXCEPTION
	try 
#endif
        {
		dObj = new DataObject((char *) sqlite3_column_text(stmt, table_dataobjects_xmlhdr), 
					sqlite3_column_bytes(stmt, table_dataobjects_xmlhdr), NULL);

	} 

#if HAVE_EXCEPTION
        catch(DataObject::DataObjectException & e) {
		HAGGLE_DBG(
			"Could not create data object : %s\n", e.getErrorMsg());
		HAGGLE_DBG(
			"Bad xml:\n%s\n", 
			sqlite3_column_text(stmt, table_dataobjects_id));
		return NULL;
	}
#endif
	dObj->setOwnsFile(false);
	dObj->setFilePath((const char *) sqlite3_column_text(stmt, table_dataobjects_filepath));
	dObj->setFileName((const char *) sqlite3_column_text(stmt, table_dataobjects_filename));
	dObj->setSignatureStatus((DataObjectSignatureStatus_t)sqlite3_column_int64(stmt, table_dataobjects_signature_status));
	dObj->setSignee(String(((const char *) sqlite3_column_text(stmt, table_dataobjects_signee))));
	sqlite_int64 millisecs = sqlite3_column_int64(stmt, table_dataobjects_receivetime);
	
	dObj->setReceiveTime(Timeval((long)(millisecs / 1000), (long)((millisecs - (millisecs / 1000)*1000) * 1000)));

	long size = (long) sqlite3_column_int64(stmt, table_dataobjects_datalen);
	
	if (size == -1)
		dObj->setDynamicDataLen(true);
	else
		dObj->setDataLen((size_t) size);

	dObj->setRxTime((unsigned long) sqlite3_column_int64(stmt, table_dataobjects_rxtime));
	// Todo: add source interface
	//dObj->setSourceInterface(getInterfaceFromRowId(db, sqlite3_column_int64(stmt, table_dataobjects_source_iface_rowid)));
	return dObj;
}

NodeRef SQLDataStore::createNode(sqlite3_stmt * in_stmt)
{
	int ret;
	char *sql_cmd;
	sqlite3_stmt *stmt;
	const char *tail;
	sqlite_int64 node_rowid;
	NodeRef node = NULL;
	int num_match = 0;

	if (!in_stmt)
		return node;

	// First try to retrieve the node from the node store
	node = kernel->getNodeStore()->retrieve((char *)sqlite3_column_blob(in_stmt, table_nodes_id));

	if (!node) {
#if HAVE_EXCEPTION
		try 
#endif
		{
			node = new Node((NodeType_t)sqlite3_column_int(in_stmt, table_nodes_type), 
				(char *)sqlite3_column_blob(in_stmt, table_nodes_id), 
				(char *)sqlite3_column_text(in_stmt, table_nodes_name));
		} 
#if HAVE_EXCEPTION
		catch (Node::NodeException &e) {
			HAGGLE_ERR("Could not create node from data store information: %s\n", e.getErrorMsg());
			return node;
		}
#endif
	}
	// Set matching limit and threshold:
	node->setMaxDataObjectsInMatch((unsigned int)sqlite3_column_int(in_stmt, table_nodes_resolution_max_matching_dataobjects));
	node->setMatchingThreshold((unsigned int)sqlite3_column_int(in_stmt, table_nodes_resolution_threshold));
	// set bloomfilter
	node->getBloomfilter()->setRaw((char *)sqlite3_column_blob(in_stmt, table_nodes_bloomfilter));

	node_rowid = sqlite3_column_int64(in_stmt, table_nodes_rowid);

	sql_cmd = SQL_ATTRS_FROM_NODE_ROWID_CMD(node_rowid);

	ret = sqlite3_prepare_v2(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);

	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", sql_cmd);
		return node;
	}

	num_match = 0;

	while ((ret = sqlite3_step(stmt)) != SQLITE_DONE) {
		if (ret == SQLITE_ROW) {
			sqlite_int64 attr_rowid = sqlite3_column_int64(stmt, table_map_nodes_to_attributes_via_rowid_attr_rowid);
			Attribute *attr = getAttrFromRowId(attr_rowid, node_rowid);

			if (!attr) {
				node = NULL;
				HAGGLE_DBG("Get attr failed\n");
				goto out;
			}
			
			node->addAttribute(*attr);
			delete attr;
		}

		if (ret == SQLITE_ERROR) {
			HAGGLE_DBG("Could not get Attribute Error:%s\n", sqlite3_errmsg(db));
			node = NULL;
			goto out;
		}
	}

	sqlite3_finalize(stmt);

	ret = sqlite3_prepare_v2(db, SQL_IFACES_FROM_NODE_ROWID_CMD(node_rowid), (int)
				 strlen(SQL_IFACES_FROM_NODE_ROWID_CMD(node_rowid)), &stmt, &tail);

	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", SQL_IFACES_FROM_NODE_ROWID_CMD);
		return node;
	}

	num_match = 0;

	while ((ret = sqlite3_step(stmt)) != SQLITE_DONE) {
		if (ret == SQLITE_ROW) {
			const char *identifier = (const char *)sqlite3_column_blob(stmt, table_interfaces_mac);
			InterfaceType_t type = (InterfaceType_t) sqlite3_column_int(stmt, table_interfaces_type);

			
			// Try to find the interface from the interface store:
			InterfaceRef iface = kernel->getInterfaceStore()->retrieve(type, identifier);
			
			if (!iface) {
				iface = new Interface(type, identifier);

				if (!iface) {
					node = NULL;
					HAGGLE_DBG("Get iface failed\n");
					goto out;
				}
			}
						
			node->addInterface(iface);
		}

		if (ret == SQLITE_ERROR) {
			HAGGLE_DBG("Could not get Interface Error:%s\n", sqlite3_errmsg(db));
			node = NULL;
			goto out;
		}
	}

      out:
	sqlite3_finalize(stmt);

	return node;
}

Attribute *SQLDataStore::getAttrFromRowId(const sqlite_int64 attr_rowid, const sqlite_int64 node_rowid)
{
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;
	char *sql_cmd;
	Attribute *attr = NULL;
	int num_match = 0;

	sql_cmd = SQL_ATTR_FROM_ROWID_CMD(attr_rowid, node_rowid);

	ret = sqlite3_prepare(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);

	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", sql_cmd);
		return NULL;
	}

	while ((ret = sqlite3_step(stmt)) != SQLITE_DONE) {
		if (ret == SQLITE_ROW) {
			num_match++;

			if (num_match == 1) {
				const char *name = (const char *)
				    sqlite3_column_text(stmt, sql_attr_from_rowid_cmd_name);
				const char *value = (const char *)
				    sqlite3_column_text(stmt, sql_attr_from_rowid_cmd_value);
				const unsigned long weight = (const unsigned long)
					sqlite3_column_int(stmt, sql_attr_from_rowid_cmd_weight);
				attr = new Attribute(name, value, weight);
			} else {
				HAGGLE_DBG("More than one Attribute with rowid=" INT64_FORMAT "\n", attr_rowid);
				goto out;
			}
		} else if (ret == SQLITE_ERROR) {
			HAGGLE_DBG("Attribute get Error:%s\n", sqlite3_errmsg(db));
			goto out;
		}
	}
      out:
	sqlite3_finalize(stmt);

	return attr;
}

DataObject *SQLDataStore::getDataObjectFromRowId(const sqlite_int64 dataObjectRowId)
{
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;
	char *sql_cmd;
	int num_match = 0;
	DataObject *dObj = NULL;

	sql_cmd = SQL_DATAOBJECT_FROM_ROWID_CMD(dataObjectRowId);

	ret = sqlite3_prepare(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);

	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", sql_cmd);
		return NULL;
	}

	while ((ret = sqlite3_step(stmt)) != SQLITE_DONE) {
		if (ret == SQLITE_ROW) {
			num_match++;

			if (num_match == 1) {
				dObj = createDataObject(stmt);
			} else {
				HAGGLE_DBG("More than on DataObject with rowid=" INT64_FORMAT "\n", dataObjectRowId);
				goto out;
			}
		} else if (ret == SQLITE_ERROR) {
			HAGGLE_DBG("DataObject get Error:%s\n", sqlite3_errmsg(db));
			goto out;
		}
	}
      out:
	sqlite3_finalize(stmt);

	return dObj;
}

NodeRef SQLDataStore::getNodeFromRowId(const sqlite_int64 nodeRowId)
{
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;
	char *sql_cmd;
	NodeRef node = NULL;
	int num_match = 0;

	sql_cmd = SQL_NODE_FROM_ROWID_CMD(nodeRowId);

	ret = sqlite3_prepare(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);

	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", sql_cmd);
		return NULL;
	}

	while ((ret = sqlite3_step(stmt)) != SQLITE_DONE) {
		if (ret == SQLITE_ROW) {
			num_match++;

			if (num_match == 1) {
				node = createNode(stmt);
			} else {
				HAGGLE_DBG("More than on Node with key=" INT64_FORMAT "\n", nodeRowId);
				node = NULL;
				goto out_err;
			}
		} else if (ret == SQLITE_ERROR) {
			HAGGLE_DBG("Node get Error:%s\n", sqlite3_errmsg(db));
			goto out_err;
		}
	}

      out_err:
	sqlite3_finalize(stmt);

	return node;
}

#if 0 // not used at the moment, disabled to remove compiler warning
Interface *SQLDataStore::getInterfaceFromRowId(const sqlite_int64 ifaceRowId)
{
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;
	char *sql_cmd;
	Interface *iface = NULL;
	int num_match = 0;

	sql_cmd = SQL_IFACE_FROM_ROWID_CMD(ifaceRowId);

	ret = sqlite3_prepare(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);

	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", sql_cmd);
		return NULL;
	}

	while ((ret = sqlite3_step(stmt)) != SQLITE_DONE) {
		if (ret == SQLITE_ROW) {
			num_match++;

			if (num_match == 1) {
				const char *identifier = (const char *)sqlite3_column_blob(stmt, table_interfaces_mac);
				InterfaceType_t type = (InterfaceType_t) sqlite3_column_int(stmt, table_interfaces_type);

				iface = new Interface(type, identifier);

			} else {
				HAGGLE_DBG("More than on Node with key=" INT64_FORMAT "\n", ifaceRowId);
				delete iface;
				iface = NULL;
				goto out_err;
			}
		} else if (ret == SQLITE_ERROR) {
			HAGGLE_DBG("Node get Error:%s\n", sqlite3_errmsg(db));
			goto out_err;
		}
	}

      out_err:
	sqlite3_finalize(stmt);

	return iface;
}
#endif

// FIXME: This function seems to be unused. Delete?
int SQLDataStore::findAndAddDataObjectTargets(DataObjectRef& dObj, const sqlite_int64 dataObjectRowId, const long ratio)
{
/*
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;
	char *sql_cmd = sqlcmd;
	//Node *node = NULL;
	int num_match = 0;

	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "SELECT * FROM %s WHERE ratio >= %ld AND dataobject_rowid == " INT64_FORMAT ";", VIEW_MATCH_DATAOBJECTS_AND_NODES_AS_RATIO, ratio, dataObjectRowId);

	ret = sqlite3_prepare(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);

	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", sql_cmd);
		goto out_err;
	}

	while ((ret = sqlite3_step(stmt)) != SQLITE_DONE) {
		if (ret == SQLITE_ROW) {
			sqlite_int64 node_rowid = sqlite3_column_int64(stmt, view_match_dataobjects_and_nodes_as_ratio_node_rowid);

			HAGGLE_DBG("Node rowid=" INT64_FORMAT "\n", node_rowid);

			NodeRef node = NodeRef(getNodeFromRowId(node_rowid), "NodeTargetFromDataStore");

			if (node) {
				num_match++;
				dObj->addTarget(node);
			} else {
				HAGGLE_DBG("Could not get node from key\n");
			}
		} else if (ret == SQLITE_ERROR) {
			HAGGLE_DBG("findTargets Error:%s\n", sqlite3_errmsg(db));
			goto out_err;
		}
	}

	sqlite3_finalize(stmt);

	return num_match;
      out_err:
	sqlite3_finalize(stmt);
*/
	return -1;
}

/* ========================================================= */
/* limit/reset views on dataobject and node attributes       */
/*                                                           */
/* the views are limited to speed up matching queries        */
/* ========================================================= */

int SQLDataStore::setViewLimitedDataobjectAttributes(sqlite_int64 dataobject_rowid) {
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;
	char *sql_cmd = sqlcmd;
	
	// -- drop existing view
	sql_cmd = (char *) SQL_DROP_VIEW_LIMITED_DATAOBJECT_ATTRIBUTES_CMD;
	ret = sqlite3_prepare(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);
	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", sql_cmd);
		HAGGLE_DBG("%s\n", sqlite3_errmsg(db));
		// view probably missing, so we do not quit
	} else {
		ret = sqlite3_step(stmt);
	}
	sqlite3_finalize(stmt);
	
	// -- replace with a) limted to specific dataobject, or b) all (default)
	sql_cmd = sqlcmd;
	if (dataobject_rowid) {
		snprintf(sql_cmd, SQL_MAX_CMD_SIZE, SQL_CREATE_VIEW_LIMITED_DATAOBJECT_ATTRIBUTES_CMD " as da WHERE da.dataobject_rowid=" INT64_FORMAT ";", dataobject_rowid);
		// remove ';' from SQL_CREATE_VIEW_LIMITED_DATAOBJECT_ATTRIBUTES_CMD
		sql_cmd[strlen(SQL_CREATE_VIEW_LIMITED_DATAOBJECT_ATTRIBUTES_CMD)-1] = ' ';
	} else {
		sql_cmd = (char *) SQL_CREATE_VIEW_LIMITED_DATAOBJECT_ATTRIBUTES_CMD;
	}
	ret = sqlite3_prepare(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);
	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", sql_cmd);
		HAGGLE_DBG("%s\n", sqlite3_errmsg(db));
		return -1;
	}
	ret = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	
	return 1;
}


int SQLDataStore::setViewLimitedNodeAttributes(sqlite_int64 node_rowid) {
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;
	char *sql_cmd = sqlcmd;
	// This is unused:
	//char *tmp = NULL;
	
	// -- drop existing view
	sql_cmd = (char *) SQL_DROP_VIEW_LIMITED_NODE_ATTRIBUTES_CMD;
	ret = sqlite3_prepare(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);
	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", sql_cmd);
		HAGGLE_DBG("%s\n", sqlite3_errmsg(db));
	} else {
		ret = sqlite3_step(stmt);
	}
	sqlite3_finalize(stmt);
	
	// -- replace with a) limted to specific node, or b) all (default)
	sql_cmd = sqlcmd;
	if (node_rowid) {
		snprintf(sql_cmd, SQL_MAX_CMD_SIZE, SQL_CREATE_VIEW_LIMITED_NODE_ATTRIBUTES_CMD " as na WHERE na.node_rowid=" INT64_FORMAT ";", node_rowid);
		// remove ';' from SQL_CREATE_VIEW_LIMITED_NODE_ATTRIBUTES_CMD
		sql_cmd[strlen(SQL_CREATE_VIEW_LIMITED_NODE_ATTRIBUTES_CMD)-1] = ' ';
	} else {
		sql_cmd = (char *) SQL_CREATE_VIEW_LIMITED_NODE_ATTRIBUTES_CMD;
	}
	ret = sqlite3_prepare(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);
	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", sql_cmd);
		HAGGLE_DBG("%s\n", sqlite3_errmsg(db));
		return -1;
	}
	ret = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	
	return 1;
}


/* ========================================================= */
/* SQLDataStore                                              */
/* ========================================================= */

SQLDataStore::SQLDataStore(const bool recreate, const string filepath, const string name) : 
	DataStore(name)
{
	int ret = 0;
	sqlite3_stmt *stmt;
	const char *tail;
	int num_tables = 0;

	if (filepath.empty()) {
		printf("Bad data store filepath %s\n", filepath.c_str());
#if HAVE_EXCEPTION
		throw SQLException(0, "Bad database filepath\n");
#else
                return;
#endif
	}
	
	FILE *fp = NULL;
	string file = filepath + PLATFORM_PATH_DELIMITER + DEFAULT_DATASTORE_FILENAME;
	// Try to open the path
	fp = fopen(file.c_str(), "rb");

	if (!fp) {
		// The directory path in which the data base resides
		string path = file.substr(0, file.find_last_of(PLATFORM_PATH_DELIMITER));
		
		// Create path. 
		if (!create_path(path.c_str())) {
			HAGGLE_ERR("Could not create directory path \'%s\'\n", path.c_str());
#if HAVE_EXCEPTION
			throw SQLException(0, "Could not create directory path\n");
#else
                        return;
#endif
		}
	} else {
		fclose(fp);
	}

	if (recreate) {
#if defined(OS_WINDOWS)
		wchar_t *wfilepath = strtowstr(file.c_str());

		if (DeleteFile(wfilepath) != 0) {
			printf("Deleted existing database file: %s\n", file.c_str());
		} else {
			printf("Failed to delete database file: %s\n", file.c_str());
		}
		free(wfilepath);
#else
		if (unlink(file.c_str()) == 0) {
			printf("Deleted existing database file: %s\n", file.c_str());
		}
#endif
	}

	ret = sqlite3_open(file.c_str(), &db);

	if (ret != SQLITE_OK) {
		fprintf(stderr, "Can't open database file %s: %s\n", file.c_str(), sqlite3_errmsg(db));
		sqlite3_close(db);
#if HAVE_EXCEPTION
		throw SQLException(ret, "Can't open database\n");
#else
                return;
#endif
	}
	
#ifdef OS_WINDOWS_MOBILE
	//sqlite3_config(SQLITE_CONFIG_HEAP, membuffer, MEMBUF_SIZE, 8);
	//sqlite3_soft_heap_limit(202500);
#endif
	// First check if the tables already exist
	ret = sqlite3_prepare(db, "SELECT name FROM sqlite_master where name='" TABLE_DATAOBJECTS "';\0", -1, &stmt, &tail);

	if (ret != SQLITE_OK) {
		fprintf(stderr, "SQLite command compilation failed: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
#if HAVE_EXCEPTION
		throw SQLException(ret, "Can't open table\n");
#else
                return;
#endif
	}

	while (1) {
		ret = sqlite3_step(stmt);

		if (ret == SQLITE_ERROR) {
			fprintf(stderr, "Could not create table error: %s\n", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			sqlite3_close(db);
#if HAVE_EXCEPTION
			throw SQLException(ret, "Could not create table.\n");
#else
                        return;
#endif
		} else if (ret == SQLITE_ROW) {
			num_tables++;
                } else if (ret == SQLITE_DONE) {
			break;
                }
	}
	if (num_tables > 0) {
		printf("Database and tables already exist...\n");
		sqlite3_finalize(stmt);
		cleanupDataStore();
		return;
	}
	sqlite3_finalize(stmt);
	// Ok, no tables exist, we need to create them
	createTables();
}


SQLDataStore::~SQLDataStore()
{
	sqlite3_close(db);
}

int SQLDataStore::createTables()
{
	int ret;
	int i = 0;
	
	while (tbl_cmds[i]) {
		//printf("%d: %s\n", i, tbl_cmds[i]);
		ret = sqlQuery(tbl_cmds[i]);

		if (ret == SQLITE_ERROR) {
			fprintf(stderr, "Could not create table error: %s\n", sqlite3_errmsg(db));
			sqlite3_close(db);
#if HAVE_EXCEPTION
			throw SQLException(ret, "Could not create table.\n");
#else
                        return -1;
#endif
		}
		i++;
	}
	return 1;
}

int SQLDataStore::cleanupDataStore()
{
	int ret;

	// removing Filters from database
	ret = sqlQuery(SQL_DELETE_FILTERS);

	if (ret == SQLITE_ERROR) {
		HAGGLE_DBG("Could not delete Filters Error:%s\n", sqlite3_errmsg(db));
	}

	return 1;
}


int SQLDataStore::sqlQuery(const char *sql_cmd)
{
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;

	ret = sqlite3_prepare_v2(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);

	if (ret != SQLITE_OK) {
		fprintf(stderr, "SQLite command compilation failed! %s\n", sql_cmd);
		return ret;
	}

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
	};
	
	if (ret == SQLITE_ERROR) {
		HAGGLE_ERR("error: %s\n", sqlite3_errmsg(db));
	}

	sqlite3_finalize(stmt);

	return ret;
}




/* ========================================================= */
/* get rowid for different objects                           */
/* ========================================================= */

sqlite_int64 SQLDataStore::getDataObjectRowId(const DataObjectRef& dObj)
{
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;
	sqlite_int64 rowid = -1;

	if (!dObj)
		return -1;

	ret = sqlite3_prepare_v2(db, SQL_FIND_DATAOBJECT_CMD, (int) strlen(SQL_FIND_DATAOBJECT_CMD), &stmt, &tail);


	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", SQL_FIND_DATAOBJECT_CMD);
		return -1;
	}

	ret = sqlite3_bind_blob(stmt, 1, dObj->getId(), DATAOBJECT_ID_LEN, SQLITE_TRANSIENT);

	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite could not bind blob!\n");
		sqlite3_finalize(stmt);
		return -1;
	}

	ret = sqlite3_step(stmt);

	if (ret == SQLITE_ROW) {
		rowid = sqlite3_column_int64(stmt, table_dataobjects_rowid);
	}

	if (ret == SQLITE_ERROR) {
		HAGGLE_DBG("Could not insert DO Error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}

	sqlite3_finalize(stmt);

	return rowid;
}

sqlite_int64 SQLDataStore::getDataObjectRowId(const DataObjectId_t& id)
{
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;
	sqlite_int64 rowid = -1;

	if (id == NULL)
		return -1;

	ret = sqlite3_prepare_v2(db, SQL_FIND_DATAOBJECT_CMD, (int) strlen(SQL_FIND_DATAOBJECT_CMD), &stmt, &tail);


	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", SQL_FIND_DATAOBJECT_CMD);
		return -1;
	}

	ret = sqlite3_bind_blob(stmt, 1, id, DATAOBJECT_ID_LEN, SQLITE_TRANSIENT);

	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite could not bind blob!\n");
		sqlite3_finalize(stmt);
		return -1;
	}

	ret = sqlite3_step(stmt);

	if (ret == SQLITE_ROW) {
		rowid = sqlite3_column_int64(stmt, table_dataobjects_rowid);
	}

	if (ret == SQLITE_ERROR) {
		HAGGLE_DBG("Could not insert DO Error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}

	sqlite3_finalize(stmt);

	return rowid;
}

sqlite_int64 SQLDataStore::getAttributeRowId(const Attribute *attr)
{
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;
	char *sql_cmd;
	sqlite_int64 rowid = -1;

	if (!attr)
		return -1;

	sql_cmd = SQL_FIND_ATTR_CMD(attr);
	ret = sqlite3_prepare_v2(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);

	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", sql_cmd);
		return -1;
	}

	ret = sqlite3_step(stmt);

	if (ret == SQLITE_ROW) {
		rowid = sqlite3_column_int64(stmt, table_attributes_rowid);
	}

	if (ret == SQLITE_ERROR) {
		HAGGLE_DBG("Could not find Attribute: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}

	sqlite3_finalize(stmt);

	return rowid;
}


sqlite_int64 SQLDataStore::getInterfaceRowId(const InterfaceRef& iface)
{
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;
	sqlite_int64 ifaceRowId = -1;

	if (!iface)
		return -1;

	ret = sqlite3_prepare_v2(db, SQL_FIND_IFACE_CMD, (int) strlen(SQL_FIND_IFACE_CMD), &stmt, &tail);

	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", SQL_FIND_IFACE_CMD);
		return -1;
	}

	ret = sqlite3_bind_blob(stmt, 1, iface->getRawIdentifier(), iface->getIdentifierLen(), SQLITE_TRANSIENT);

	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite could not bind blob!\n");
		sqlite3_finalize(stmt);
		return -1;
	}

	ret = sqlite3_step(stmt);

	if (ret == SQLITE_ROW) {
		ifaceRowId = sqlite3_column_int64(stmt, table_interfaces_rowid);
	}

	if (ret == SQLITE_ERROR) {
		HAGGLE_DBG("Could not insert DO Error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}

	sqlite3_finalize(stmt);

	return ifaceRowId;
}



sqlite_int64 SQLDataStore::getNodeRowId(const NodeRef& node)
{
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;
	sqlite_int64 nodeRowId = -1;

	if (node->getType() != NODE_TYPE_UNDEF) {
		// lookup by id
		ret = sqlite3_prepare_v2(db, SQL_NODE_FROM_ID_CMD, (int) strlen(SQL_NODE_FROM_ID_CMD), &stmt, &tail);

		if (ret != SQLITE_OK) {
			HAGGLE_DBG("SQLite command compilation failed! %s\n", SQL_NODE_FROM_ID_CMD);
			return -1;
		}

		ret = sqlite3_bind_blob(stmt, 1, node->getId(), NODE_ID_LEN, SQLITE_TRANSIENT);

		if (ret != SQLITE_OK) {
			HAGGLE_DBG("SQLite could not bind blob!\n");
			sqlite3_finalize(stmt);
			return -1;
		}

		ret = sqlite3_step(stmt);

		if (ret == SQLITE_ROW) {
			nodeRowId = sqlite3_column_int64(stmt, table_nodes_rowid);
		}

		if (ret == SQLITE_ERROR) {
			HAGGLE_DBG("Could not insert DO Error: %s\n", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			return -1;
		}

		sqlite3_finalize(stmt);
	} else {
		// lookup by common interfaces
		
		snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "SELECT node_rowid FROM %s", TABLE_INTERFACES);

		const InterfaceRefList *ifaces = node->getInterfaces();
		
		int cnt = 0;
		for (InterfaceRefList::const_iterator it = ifaces->begin(); it != ifaces->end(); it++) {
			InterfaceRef ifaceRef = (*it);
			cnt++;
			
			if (cnt == 1) 
				sprintf(&(sqlcmd[strlen(sqlcmd)]), " WHERE (type = %d AND mac_str='%s')", ifaceRef->getType(), ifaceRef->getIdentifierStr());
			else
				sprintf(&(sqlcmd[strlen(sqlcmd)]), " OR (type = %d AND mac_str='%s')", ifaceRef->getType(), ifaceRef->getIdentifierStr());
		}
		
		strcat(sqlcmd, ";");

		ret = sqlite3_prepare_v2(db, sqlcmd, (int) strlen(sqlcmd), &stmt, &tail);
			
		if (ret != SQLITE_OK) {
			HAGGLE_DBG("SQLite command compilation failed! %s\n", sqlcmd);
			return -1;
		}
			
		ret = sqlite3_step(stmt);
		
		if (ret == SQLITE_ROW) {
			// No name for this column: See select statement in this function:
			nodeRowId = sqlite3_column_int64(stmt, 0);
		}
			
		if (ret == SQLITE_ERROR) {
			HAGGLE_DBG("Could not retrieve node from database: %s\n", sqlite3_errmsg(db));
			return -1;
		}

		sqlite3_finalize(stmt);

	}

	return nodeRowId;
}


/* ========================================================= */
/* Filter matching                                           */
/* ========================================================= */

// ----- Dataobject > Filters

int SQLDataStore::evaluateFilters(const DataObjectRef& dObj, sqlite_int64 dataobject_rowid)
{
	int ret, n = 0;
	sqlite3_stmt *stmt;
	char *sql_cmd = sqlcmd;
	const char *tail;
	sqlite_int64 filter_rowid = -1;
	int eventType = -1;
	DataObjectRefList dObjs;

	if (!dObj)
		return -1;

	HAGGLE_DBG("Evaluating filters\n");

	if (dataobject_rowid == 0) {
		dataobject_rowid = getDataObjectRowId(dObj);
	}
	
	/* limit VIEW_MAP_DATAOBJECTS_TO_ATTRIBUTES_VIA_ROWID_DYNAMIC to dataobject in question */
	setViewLimitedDataobjectAttributes(dataobject_rowid);

	/* matching filters */
	sql_cmd = (char *) SQL_FILTER_MATCH_DATAOBJECT_ALL_CMD;
	ret = sqlite3_prepare(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);

	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", SQL_FILTER_MATCH_DATAOBJECT_ALL_CMD);
		return -1;
	}
	
	// Add the data object to the result list
	dObjs.add(dObj);

	/* Loop through the results, i.e., all the filters that match */
	while ((ret = sqlite3_step(stmt)) != SQLITE_DONE) {
		if (ret == SQLITE_ROW) {
			
			filter_rowid = sqlite3_column_int64(stmt, view_match_filters_and_dataobjects_as_ratio_filter_rowid);
			eventType = sqlite3_column_int(stmt, view_match_filters_and_dataobjects_as_ratio_filter_event);

			HAGGLE_DBG("Filter " INT64_FORMAT " with event type %d matches!\n", filter_rowid, eventType);
			n++;

                        kernel->addEvent(new Event(eventType, dObjs));
		} else if (ret == SQLITE_ERROR) {
			HAGGLE_DBG("Could not insert DO Error: %s\n", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			return -1;
		}
	}

	sqlite3_finalize(stmt);

	return n;
}


// ----- Filter > Dataobjects

int SQLDataStore::evaluateDataObjects(long eventType)
{
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;
	sqlite_int64 do_rowid = -1;
	char *sql_cmd = sqlcmd;
	DataObjectRefList dObjs;

	HAGGLE_DBG("Evaluating filter\n");

	/* reset dynamic link table */
	setViewLimitedDataobjectAttributes();	

	sql_cmd = SQL_FILTER_MATCH_ALL_CMD(eventType);
	
	ret = sqlite3_prepare_v2(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);

	if (ret != SQLITE_OK) {
		HAGGLE_DBG("Match filter command compilation failed\n");
		return -1;
	}

	while ((ret = sqlite3_step(stmt)) != SQLITE_DONE) {
		if (ret == SQLITE_ROW) {
			
			do_rowid = sqlite3_column_int64(stmt, view_match_filters_and_dataobjects_as_ratio_dataobject_rowid);

			HAGGLE_DBG("Data object with rowid " INT64_FORMAT " matches!\n", do_rowid);
			
			dObjs.push_back(DataObjectRef(getDataObjectFromRowId(do_rowid)));
                        
			// FIXME: Set a limit on how many data objects to match when registering
			// a filter. If there are many data objects, the matching will take too long time
			// and Haggle will become unresponsive.
			// Therefore, we set a limit to 10 data objects here. In the future we should
			// make sure the limit is a user configurable variable and that the data
			// objects returned are the highest ranking ones in descending order.
			
			if (dObjs.size() == 10)
				break;

		} else if (ret == SQLITE_ERROR) {
			HAGGLE_DBG("Could not insert DO Error: %s\n", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			return -1;
		}
	}

	sqlite3_finalize(stmt);
	
	if (dObjs.size())
		kernel->addEvent(new Event(eventType, dObjs));

	return dObjs.size();
}


/* ========================================================= */
/* inserting and deleting of different objects               */
/*                                                           */
/* note: insert is actually an update                        */
/*	     (existing objects get replaced)                     */
/* ========================================================= */

int SQLDataStore::_deleteFilter(long eventtype)
{
	int ret;
	char *sql_cmd;
	sqlite3_stmt *stmt;
	const char *tail;

	sql_cmd = SQL_DELETE_FILTER_CMD(eventtype);

	ret = sqlite3_prepare_v2(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);

	if (ret != SQLITE_OK) {
		HAGGLE_DBG("Delete filter command compilation failed\n");
		return -1;
	}

	ret = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (ret == SQLITE_ERROR) {
		HAGGLE_DBG("Could not delete filter : %s\n", sqlite3_errmsg(db));
		return -1;
	}
	return 0;
}

int SQLDataStore::_insertFilter(Filter *f, bool matchFilter, const EventCallback<EventHandler> *callback)
{
	int ret;
	char *sql_cmd;
	sqlite3_stmt *stmt;
	const char *tail;
	sqlite_int64 filter_rowid;
	sqlite_int64 attr_rowid;
	const Attributes *attrs;

	if (!f)
		return -1;

	sql_cmd = SQL_INSERT_FILTER_CMD(f->getEventType());

	ret = sqlite3_prepare_v2(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);

	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", sql_cmd);
		return -1;
	}

	ret = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (ret == SQLITE_CONSTRAINT) {
		HAGGLE_DBG("Filter exists, updating...\n");
		
		ret = _deleteFilter(f->getEventType());

		if (ret < 0) {
			HAGGLE_ERR("Could not delete filter\n");
			return -1;
		}
		// Call this function again
		ret = _insertFilter(f, matchFilter, callback);

		return ret;
	} else if (ret == SQLITE_ERROR) {
		HAGGLE_DBG("Could not insert Filter : %s\n", sqlite3_errmsg(db));
		return -1;
	}

	filter_rowid = sqlite3_last_insert_rowid(db);

	// Insert Attributes
	attrs = f->getAttributes();

	for (Attributes::const_iterator it = attrs->begin(); it != attrs->end(); it++) {
		const Attribute& a = (*it).second;

		ret = sqlQuery(SQL_INSERT_ATTR_CMD(&a));

		if (ret == SQLITE_ERROR) {
			HAGGLE_DBG("SQLite insert of attribute failed!\n");
			return -1;
		}

		if (ret == SQLITE_CONSTRAINT) {
			attr_rowid = getAttributeRowId(&a);
		} else {
			attr_rowid = sqlite3_last_insert_rowid(db);
		}

		ret = sqlQuery(SQL_INSERT_FILTER_ATTR_CMD(filter_rowid, attr_rowid, a.getWeight()));

		if (ret == SQLITE_ERROR) {
			HAGGLE_DBG("SQLite insert of filter-attribute link failed!\n");
			return -1;
		}
	}

	if (callback)
		kernel->addEvent(new Event(callback, f));
	
	// Find all data objects that match this filter, and report them back:
	if (matchFilter)
		evaluateDataObjects(f->getEventType());
	
	return (int) filter_rowid;
}


int SQLDataStore::_deleteNode(NodeRef& node)
{
	int ret;
	char *sql_cmd;
	sqlite3_stmt *stmt;
	const char *tail;
	
	sql_cmd = SQL_DELETE_NODE_CMD();
	
	ret = sqlite3_prepare_v2(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);
	
	if (ret != SQLITE_OK) {
		HAGGLE_DBG("Delete node command compilation failed : %s\n", sqlite3_errmsg(db));
		return -1;
	}
	
	ret = sqlite3_bind_blob(stmt, 1, node->getId(), NODE_ID_LEN, SQLITE_TRANSIENT);
   
	if (ret != SQLITE_OK) {
	   HAGGLE_DBG("SQLite could not bind blob!\n");
	   sqlite3_finalize(stmt);
	   return -1;
	}
				   
	ret = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	
	if (ret == SQLITE_ERROR) {
		HAGGLE_DBG("Could not delete node : %s\n", sqlite3_errmsg(db));
		return -1;
	}
	return 0;
}


int SQLDataStore::_insertNode(NodeRef& node, const EventCallback<EventHandler> *callback)
{
	int ret;
	char *sql_cmd;
	sqlite3_stmt *stmt;
	const char *tail;
	sqlite_int64 node_rowid;
	//sqlite_int64 dataobject_rowid;
	sqlite_int64 attr_rowid;
	const Attributes *attrs;
	const InterfaceRefList *ifaces;

	if (!node->getDataObject())
		return -1;

	node.lock();

	// Do not insert nodes with undefined state/type
	if (node->getType() == NODE_TYPE_UNDEF) {
		HAGGLE_DBG("DataStore: Node type undefined. Ignoring INSERT\n");
		node.unlock();
		return -1;
	}
	HAGGLE_DBG("DataStore insert Node (%d)\n", node->getAttributes()->size());

//	sqlQuery(SQL_BEGIN_TRANSACTION_CMD);

	sql_cmd = SQL_INSERT_NODE_CMD((const int) node->getType(), node->getIdStr(), 
		node->getName().c_str(), node->getMaxDataObjectsInMatch(), node->getMatchingThreshold());

	HAGGLE_DBG("SQLcmd: %s\n", sql_cmd);

	ret = sqlite3_prepare_v2(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);

	if (ret != SQLITE_OK) {
		HAGGLE_DBG("Error: %s\n", sqlite3_errmsg(db));
		goto out_insertNode_err;
	}

	ret = sqlite3_bind_blob(stmt, 1, node->getId(), NODE_ID_LEN, SQLITE_TRANSIENT);

	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite could not bind blob!\n");
		sqlite3_finalize(stmt);
		goto out_insertNode_err;
	}

	ret = sqlite3_bind_blob(stmt, 2, node->getBloomfilter()->getRaw(), node->getBloomfilter()->getRawLen() , SQLITE_TRANSIENT);

	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite could not bind blob!\n");
		sqlite3_finalize(stmt);
		goto out_insertNode_err;
	}

	ret = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (ret == SQLITE_CONSTRAINT) {
		HAGGLE_DBG("Node already in datastore\n");
		
		ret = _deleteNode(node);
		
		if (ret < 0) {
			HAGGLE_ERR("Could not delete node\n");
			goto out_insertNode_err;
		}

		// Call this function again
//		sqlQuery(SQL_END_TRANSACTION_CMD);
		int ret = _insertNode(node, callback);
		node.unlock();
		return ret;
	} else if (ret == SQLITE_ERROR) {
		HAGGLE_DBG("Could not insert Node Error: %s\n", sqlite3_errmsg(db));
		goto out_insertNode_err;
	} else {
		node_rowid = sqlite3_last_insert_rowid(db);
	}

	HAGGLE_DBG("Node rowid=" INT64_FORMAT "\n", node_rowid);

	// Insert Attributes
	
	// Must use the node pointer here since the nodeRef is now locked.
	attrs = node->getAttributes();

	for (Attributes::const_iterator it = attrs->begin(); it != attrs->end(); it++) {
		const Attribute& a = (*it).second;
		HAGGLE_DBG("Inserting attribute %s=%s\n", a.getName().c_str(), a.getValue().c_str());

		sql_cmd = SQL_INSERT_ATTR_CMD(&a);

		ret = sqlQuery(sql_cmd);

		if (ret == SQLITE_ERROR) {
			HAGGLE_DBG("SQLite insert of attribute failed !\n");
			goto out_insertNode_err;
		}

		if (ret == SQLITE_CONSTRAINT) {
			attr_rowid = getAttributeRowId(&a);
		} else {
			attr_rowid = sqlite3_last_insert_rowid(db);
		}

		sql_cmd = SQL_INSERT_NODE_ATTR_CMD(node_rowid, attr_rowid, a.getWeight());

		ret = sqlQuery(sql_cmd);

		if (ret == SQLITE_ERROR) {
			HAGGLE_DBG("SQLite insert of node-attribute link failed!\n");
			goto out_insertNode_err;
		}
	}

	// Insert node interfaces
	ifaces = node->getInterfaces();

	HAGGLE_DBG("Node has %d interfaces\n", ifaces->size());

	for (InterfaceRefList::const_iterator it = ifaces->begin(); it != ifaces->end(); it++) {
		InterfaceRef ifaceRef = (*it);

		ifaceRef.lock();
		
		sql_cmd = SQL_INSERT_IFACE_CMD((sqlite_int64) ifaceRef->getType(), ifaceRef->getIdentifierStr(), node_rowid);
		HAGGLE_DBG("Insert interface SQLcmd: %s\n", sql_cmd);

		ret = sqlite3_prepare_v2(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);

		if (ret != SQLITE_OK) {
			HAGGLE_DBG("SQLite command compilation failed! %s\n", sql_cmd);
			ifaceRef.unlock();
			goto out_insertNode_err;
		}

		ret = sqlite3_bind_blob(stmt, 1, ifaceRef->getRawIdentifier(), ifaceRef->getIdentifierLen(), SQLITE_TRANSIENT);

		if (ret != SQLITE_OK) {
			HAGGLE_DBG("SQLite could not bind interface identifier blob!\n");
			sqlite3_finalize(stmt);
			ifaceRef.unlock();
			goto out_insertNode_err;
		}

		ret = sqlite3_step(stmt);
		sqlite3_finalize(stmt);

		if (ret == SQLITE_CONSTRAINT) {
			HAGGLE_DBG("Interface already in datastore\n");
		} else if (ret == SQLITE_ERROR) {
			HAGGLE_DBG("Could not insert Interface Error:%s\n", sqlite3_errmsg(db));
			ifaceRef.unlock();
			goto out_insertNode_err;
		}
		ifaceRef.unlock();
	}

//	sqlQuery(SQL_END_TRANSACTION_CMD);	
	node.unlock();
	
	if (callback)
		kernel->addEvent(new Event(callback, node));
	
	return 1;
	
out_insertNode_err:
//	sqlQuery(SQL_END_TRANSACTION_CMD);
	node.unlock();
	return -1;
	
}

int SQLDataStore::_deleteDataObject(const DataObjectId_t &id, bool shouldReportRemoval)
{
	int ret;
	char *sql_cmd;
	sqlite3_stmt *stmt;
	const char *tail;
	char idStr[MAX_DATAOBJECT_ID_STR_LEN];
	int len = 0;

	// Generate a readable string of the Id
	for (int i = 0; i < DATAOBJECT_ID_LEN; i++) {
		len += sprintf(idStr + len, "%02x", id[i] & 0xff);
	}
	
	if (shouldReportRemoval) {
		DataObjectRef dObj = getDataObjectFromRowId(getDataObjectRowId(id));
		// FIXME: shouldn't the data object be given back ownership of it's 
		// file? (If it has one.) So that the file is removed from disk along 
		// with the data object.
		if (dObj) {
			kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_DELETED, dObj));
		} else {
			HAGGLE_ERR(
				"Tried to report removal of a data object that "
				"isn't in the data store. (id=%s)\n",
				idStr);
			// there should not be a data object to delete, so done.
			return -1;
		}
	}
	
	sql_cmd = SQL_DELETE_DATAOBJECT_CMD();
	
	ret = sqlite3_prepare_v2(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);
	
	if (ret != SQLITE_OK) {
		HAGGLE_DBG("Delete dataobject command compilation failed : %s\n", sqlite3_errmsg(db));
		return -1;
	}
	
	ret = sqlite3_bind_blob(stmt, 1, id, DATAOBJECT_ID_LEN, SQLITE_TRANSIENT);
	
	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite could not bind blob!\n");
		sqlite3_finalize(stmt);
		return -1;
	}
	
	ret = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	
	if (ret == SQLITE_ERROR) {
		HAGGLE_DBG("Could not delete dataobject : %s\n", sqlite3_errmsg(db));
		return -1;
	} else {
		if (ret == SQLITE_ROW) {
			HAGGLE_DBG("SQLITE_ROW Deleted data object %s\n", idStr);
		} else if (ret == SQLITE_DONE) {
			HAGGLE_DBG("SQLITE_DONE Deleted data object %s\n", idStr);
		} else {
			HAGGLE_DBG("Delete data object %s - NO MATCH?\n", idStr);
		}
	}
	return 0;
}

int SQLDataStore::_deleteDataObject(DataObjectRef& dObj, bool shouldReportRemoval)
{
	// FIXME: shouldn't the data object be given back ownership of it's file?
	// (If it has one.) So that the file is removed from disk along with the
	// data object.
	
	if (_deleteDataObject(dObj->getId(), false) == 0 && shouldReportRemoval)
		kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_DELETED, dObj));
	
	return 0;
}

int SQLDataStore::_ageDataObjects(const Timeval& minimumAge, const EventCallback<EventHandler> *callback)
{
	int ret;
	char *sql_cmd;
	sqlite3_stmt *stmt;
	const char *tail;
	DataObjectRefList dObjs;
	
	// -- drop dataobject view
	sql_cmd = (char *) SQL_DROP_VIEW_LIMITED_DATAOBJECT_ATTRIBUTES_CMD;
	ret = sqlite3_prepare(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);

	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", sql_cmd);
		HAGGLE_DBG("%s\n", sqlite3_errmsg(db));
		// view probably missing, so we do not quit
	} else {
		ret = sqlite3_step(stmt);
	}
	sqlite3_finalize(stmt);
	
	// -- reset dataobject view
	sql_cmd = (char *) SQL_CREATE_VIEW_LIMITED_DATAOBJECT_ATTRIBUTES_CMD;
	ret = sqlite3_prepare(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);
	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", sql_cmd);
		HAGGLE_DBG("%s\n", sqlite3_errmsg(db));
		// view probably missing, so we do not quit
	} else {
		ret = sqlite3_step(stmt);
	}
	sqlite3_finalize(stmt);
	
	// -- delete dataobjects not related to any filter (no interest) and being created more than minimumAge seconds ago. 
	sql_cmd = SQL_AGE_DATAOBJECT_CMD(minimumAge);
	
	ret = sqlite3_prepare_v2(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);

	if (ret != SQLITE_OK) {
		HAGGLE_DBG("Dataobject aging command compilation failed : %s\n", sqlite3_errmsg(db));
		ret = -1;
		goto out;
	}
	
	while (dObjs.size() < DATASTORE_MAX_DATAOBJECTS_AGED_AT_ONCE && (ret = sqlite3_step(stmt)) != SQLITE_DONE) {
		if (ret == SQLITE_ROW) {
			dObjs.push_back(createDataObject(stmt));
		} else if (ret == SQLITE_ERROR) {
			HAGGLE_DBG("Could not age data object - Error: %s\n", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			ret = -1;
			goto out;
		}
	}
	
	sqlite3_finalize(stmt);
	
	for (DataObjectRefList::iterator it = dObjs.begin(); it != dObjs.end(); it++) {
		_deleteDataObject(*it, false);	// delete and report as event
	}
	kernel->addEvent(new Event(EVENT_TYPE_DATAOBJECT_DELETED, dObjs));
	
	if (ret == SQLITE_ERROR) {
		HAGGLE_DBG("Could not age data objects : %s\n", sqlite3_errmsg(db));
		ret = -1;
	}
		
out:
	if (callback)
		kernel->addEvent(new Event(callback, dObjs));

	return ret;
}

int SQLDataStore::_insertDataObject(DataObjectRef& dObj, const EventCallback<EventHandler> *callback)
{
	int ret;
	size_t metadatalen;
	char *metadata;
	char *sql_cmd;
	string sql_cmd_str;
	sqlite3_stmt *stmt;
	const char *tail;
	sqlite_int64 dataobject_rowid;
	sqlite_int64 attr_rowid;
	sqlite_int64 ifaceRowId = -1;
	const Attributes *attrs;

	dObj.lock();

	if (!dObj) {
		dObj.unlock();
		return -1;
	}

	HAGGLE_DBG("DataStore insert DataObject with num_attributes=%d\n", dObj->getAttributes()->size());

	if (!dObj->getRawMetadataAlloc(&metadata, &metadatalen)) {
		HAGGLE_ERR("Could not get raw metadata from DO\n");
		dObj.unlock();
		return -1;
	}

	if (dObj->getRemoteInterface())
		ifaceRowId = getInterfaceRowId(dObj->getRemoteInterface());


	sql_cmd_str = SQL_INSERT_DATAOBJECT_CMD(metadata, ifaceRowId, dObj);
	sql_cmd = (char *) sql_cmd_str.c_str();

	//HAGGLE_DBG("SQLcmd: %s\n", sql_cmd);
	ret = sqlite3_prepare_v2(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);

	if (ret != SQLITE_OK) {
		HAGGLE_ERR("SQLite command compilation failed! %s\n", sql_cmd);
		goto out_insertDataObject_err;
	}

	ret = sqlite3_bind_blob(stmt, 1, dObj->getId(), DATAOBJECT_ID_LEN, SQLITE_TRANSIENT);

	if (ret != SQLITE_OK) {
		HAGGLE_ERR("SQLite could not bind blob!\n");
		sqlite3_finalize(stmt);
		goto out_insertDataObject_err;
	}

	ret = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (ret == SQLITE_CONSTRAINT) {
		HAGGLE_ERR("DataObject already in datastore\n");
                dObj->setDuplicate();
		goto out_insertDataObject_duplicate;
	}
	
	dObj->setOwnsFile(false);
	
	if (ret == SQLITE_ERROR) {
		HAGGLE_ERR("Could not insert DO Error: %s\n", sqlite3_errmsg(db));
		goto out_insertDataObject_err;
	} else if (ret != SQLITE_DONE) {
                HAGGLE_ERR("Insert data object did not return SQLITE_DONE\n");
        }

	dataobject_rowid = sqlite3_last_insert_rowid(db);

	// Insert Attributes
	attrs = dObj->getAttributes();

	for (Attributes::const_iterator it = attrs->begin(); it != attrs->end(); it++) {
		const Attribute& a = (*it).second;

		sql_cmd = SQL_INSERT_ATTR_CMD(&a);

		ret = sqlQuery(sql_cmd);

		if (ret == SQLITE_ERROR) {
			HAGGLE_ERR("SQLite insert of attribute failed!\n");
			goto out_insertDataObject_err;
		}

		
		if (ret == SQLITE_CONSTRAINT) {
			attr_rowid = getAttributeRowId(&a);
		} else {
			attr_rowid = sqlite3_last_insert_rowid(db);
		}

		sql_cmd = SQL_INSERT_DATAOBJECT_ATTR_CMD(dataobject_rowid, attr_rowid);

		ret = sqlQuery(sql_cmd);

		if (ret == SQLITE_ERROR) {
			HAGGLE_ERR("SQLite insert of dataobject-attribute link failed!\n");
			goto out_insertDataObject_err;
		}

	}

	dObj.unlock();
	free(metadata);

	// Evaluate Filters
	evaluateFilters(dObj, dataobject_rowid);

	// Remove non-persistent data object from database
	if (!dObj->isPersistent()) {
		// comment: we do that check here after actually having inserted the data 
		// object into the database to allow for standard duplicate check and 
		// standard filter evaluation on non-persistent data objects. 
		_deleteDataObject(dObj, false);
	}
		
	if (callback)
		kernel->addEvent(new Event(callback, dObj));
		
	return 0;

out_insertDataObject_duplicate:
	dObj.unlock();
	free(metadata);

        // Notify the data manager of this duplicate data object
        if (callback)
		kernel->addEvent(new Event(callback, dObj));
        
        return 0;
out_insertDataObject_err:
	dObj.unlock();
	free(metadata);
	return -1;
}



/* ========================================================= */
/* Asynchronous calls to retrieve objects                    */
/* ========================================================= */

int SQLDataStore::_retrieveNode(NodeRef& refNode, const EventCallback<EventHandler> *callback, bool forceCallback)
{
	NodeRef node;
	
	HAGGLE_DBG("DataStore retrieve Node \n");
	
	sqlite_int64 node_rowid = getNodeRowId(refNode);
	
	node = getNodeFromRowId(node_rowid);
	
	if (!node) {
		if (forceCallback) {
			kernel->addEvent(new Event(callback, refNode));
			return 1;
		} else{
			return -1;
		}
	}
	
	/*
	 FIXME: This is done to allow an application's new UDP port number to be
	 moved to it's old node. This should really be somehow done in the
	 application manager, or have some other way of triggering it, rather
	 than relying on forceCallback to be true only when it's the application
	 manager that caused this function to be called.
	 */
	if (forceCallback) {
		refNode.lock();
		const InterfaceRefList *lst = refNode->getInterfaces();
		
		for (InterfaceRefList::const_iterator it = lst->begin(); it != lst->end(); it++) {
			node->addInterface(*it);
		}
		refNode.unlock();
	}
	
	
	kernel->addEvent(new Event(callback, node));
	return 1;
}

int SQLDataStore::_retrieveNodeByType(NodeType_t type, const EventCallback<EventHandler> *callback)
{
	NodeRefList *nodes = NULL;
	int ret;
	char *sql_cmd;
	sqlite3_stmt *stmt;
	const char *tail;
	
	sql_cmd = SQL_NODE_BY_TYPE_CMD(type);
	
	ret = sqlite3_prepare(db, sql_cmd, (int)strlen(sql_cmd), &stmt, &tail);
	
	if (ret != SQLITE_OK) {
		HAGGLE_ERR("Node by type command compilation failed : %s\n", sqlite3_errmsg(db));
		return -1;
	}
	
	do {
		ret = sqlite3_step(stmt);

		if (ret == SQLITE_ROW) {
			NodeRef node = getNodeFromRowId(sqlite3_column_int64(stmt, sql_node_by_type_cmd_rowid));
			if (node) {
				if (nodes == NULL) {
					nodes = new NodeRefList();
				}
				nodes->push_front(node);
			}
		}
	} while (ret != SQLITE_DONE);
	
	sqlite3_finalize(stmt);
	
	kernel->addEvent(new Event(callback, nodes));

	return 1;
}



/* ========================================================= */
/* Asynchronous queries                                      */
/* ========================================================= */

// ----- Filter > Dataobjects

int SQLDataStore::_doFilterQuery(DataStoreFilterQuery *q)
{
	DataStoreQueryResult *qr;
	unsigned int num_match = 0;
	sqlite3_stmt *stmt;
	const char *tail;
	char *sql_cmd = sqlcmd;
	int ret;
	sqlite_int64 filter_rowid = 0;
	sqlite_int64 dataobject_rowid = 0;
	
	HAGGLE_DBG("DataStore Filter Query\n");
	
	if (!q) {
		return -1;
	}	
	
	qr = new DataStoreQueryResult();

	if (!qr) {
		HAGGLE_DBG("Could not allocate query result object\n");
		return -1;
	}
	
	// insert filter into database (remove after query)
	filter_rowid = _insertFilter((Filter*)q->getFilter());
	
	/* reset view dataobject>attribute */
	setViewLimitedDataobjectAttributes();

	/* query */
	sql_cmd = SQL_FILTER_MATCH_DATAOBJECT_CMD(filter_rowid);
	ret = sqlite3_prepare(db, sql_cmd, (int)strlen(sql_cmd), &stmt, &tail);
	
	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", sql_cmd);
		ret = -1;
		goto filterQuery_cleanup;
	}
	
	/* loop through results and create dataobjects */
	while ((ret = sqlite3_step(stmt)) != SQLITE_DONE) {
		if (num_match == 0) {
			qr->setQuerySqlEndTime();
		}
		
		if (ret == SQLITE_ROW) {
			num_match++;
			
			dataobject_rowid = sqlite3_column_int(stmt, view_match_filters_and_dataobjects_as_ratio_dataobject_rowid);
			
			HAGGLE_DBG("Dataobject with rowid %d matches!\n", dataobject_rowid);
			
			DataObjectRef dObj = DataObjectRef(getDataObjectFromRowId(dataobject_rowid), "DataObjectInQueryResult");
			
			if (dObj) {
				qr->addDataObject(dObj);
			} else {
				HAGGLE_DBG("Could not get data object from rowid\n");
			}
		} else if (ret == SQLITE_ERROR) {
			HAGGLE_DBG("Could not insert DO Error: %s\n", sqlite3_errmsg(db));
			break;
		}
	}
	
	sqlite3_finalize(stmt);

	if (num_match) {
		kernel->addEvent(new Event(q->getCallback(), qr));
	} else {
		delete qr;
	}

	ret = num_match;
	
filterQuery_cleanup:	
	// remove filter from database
	sprintf(sql_cmd, "DELETE FROM %s WHERE rowid = " INT64_FORMAT "", TABLE_FILTERS, filter_rowid);
	sqlite3_prepare(db, sql_cmd, (int)strlen(sql_cmd), &stmt, &tail);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	
	return ret;
}


// ----- Node > Dataobjects

int SQLDataStore::_doDataObjectQueryStep2(NodeRef &node, NodeRef alsoThisBF, DataStoreQueryResult *qr, int max_matches, unsigned int threshold, unsigned int attrMatch)
{
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;
	char *sql_cmd = sqlcmd;
	int num_match = 0;
	
	sqlite_int64 node_rowid = getNodeRowId(node);

	if (node_rowid == -1 ){
		HAGGLE_DBG("No rowid for node %s\n", node->getName().c_str());
		return 0;
	}

	/* limit VIEW_MAP_NODES_TO_ATTRIBUTES_VIA_ROWID_DYNAMIC to dataobject in question */
	setViewLimitedNodeAttributes(node_rowid);
	 
	/* matching */
	snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "SELECT * FROM " VIEW_MATCH_NODES_AND_DATAOBJECTS_AS_RATIO " WHERE ratio >= %u AND mcount >= %u;", threshold, attrMatch);

	ret = sqlite3_prepare(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);

	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", sql_cmd);
		return 0;
	}

	/* looping through the results and allocating dataobjects */
	while ((ret = sqlite3_step(stmt)) != SQLITE_DONE) {

		if (ret == SQLITE_ROW) {
			sqlite_int64 dObjRowId = sqlite3_column_int64(stmt, view_match_nodes_and_dataobjects_rated_dataobject_rowid);

			DataObjectRef dObj = DataObjectRef(getDataObjectFromRowId(dObjRowId), "DataObjectInQueryResult");

			if (dObj) {
				if (!(node->getBloomfilter()->has(dObj) || (alsoThisBF && alsoThisBF->getBloomfilter()->has(dObj)))) {
					//HAGGLE_DBG("Data object rowid=" INT64_FORMAT "\n", dObjRowId);
					qr->addDataObject(dObj);
					num_match++;
					if (max_matches != 0 && (num_match >= max_matches)) {
						break;
					}
				}
			} else {
				HAGGLE_DBG("Could not get data object from rowid\n");
			}
		} else if (ret == SQLITE_ERROR) {
			HAGGLE_DBG("data object query Error:%s\n", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			return num_match;
		}
	}

	sqlite3_finalize(stmt);
	
	return num_match;
}

int SQLDataStore::_doDataObjectQuery(DataStoreDataObjectQuery *q)
{
	unsigned int num_match = 0;
	DataStoreQueryResult *qr;
	NodeRef node = q->getNode();

	HAGGLE_DBG("DataStore DataObject Query for node=%s\n", node->getIdStr());

	qr = new DataStoreQueryResult();

	if (!qr) {
		HAGGLE_DBG("Could not allocate query result object\n");
		return -1;
	}

	qr->addNode(node);
	qr->setQuerySqlStartTime();
	qr->setQueryInitTime(q->getQueryInitTime());

	num_match = _doDataObjectQueryStep2(node, NULL, qr, node->getMaxDataObjectsInMatch(), node->getMatchingThreshold(), q->getAttrMatch());

	if (num_match == 0) {
		qr->setQuerySqlEndTime();
	}
	qr->setQueryResultTime();

#if defined(BENCHMARK)
	kernel->addEvent(new Event(q->getCallback(), qr));
#else
	if (num_match) {
		kernel->addEvent(new Event(q->getCallback(), qr));
	} else {
		delete qr;
	}
#endif

	HAGGLE_DBG("%u data objects matched query\n", num_match);

	return num_match;
}

/*
	This function is basically the same as _doDataObjectQuery, except that it
	also goes through a list of secondary nodes.
*/
int SQLDataStore::_doDataObjectForNodesQuery(DataStoreDataObjectForNodesQuery *q)
{
	unsigned int num_match = 0;
#if defined(DEBUG)
	unsigned int total_match = 0;
#endif
	long num_left;
	bool has_maximum = false;
	DataStoreQueryResult *qr;
	NodeRef node = q->getNode();
	NodeRef delegateNode = node;
	unsigned int threshold = 0;

	HAGGLE_DBG("DataStore DataObject (for multiple nodes) Query for node=%s\n", 
		node->getIdStr());

	qr = new DataStoreQueryResult();

	if (!qr) {
		HAGGLE_DBG("Could not allocate query result object\n");
		return -1;
	}

	qr->addNode(node);
	qr->setQuerySqlStartTime();
	qr->setQueryInitTime(q->getQueryInitTime());
	
	num_left = node->getMaxDataObjectsInMatch();

	if(num_left > 0)
		has_maximum = true;

	threshold = node->getMatchingThreshold();
	node = q->getNextNode();
	
	while (node != (Node *)NULL && !(has_maximum && (num_left <= 0))) {

		num_match = _doDataObjectQueryStep2(node, delegateNode, qr, num_left, threshold, q->getAttrMatch());
                
		if (has_maximum) {
			num_left -= num_match;
		}
#if defined(DEBUG)
		total_match += num_match;
#endif
		node = q->getNextNode();
	}

	qr->setQuerySqlEndTime();
	qr->setQueryResultTime();

#if defined(BENCHMARK)
	kernel->addEvent(new Event(q->getCallback(), qr));
#else
	if (num_match) {
		kernel->addEvent(new Event(q->getCallback(), qr));
	} else {
		delete qr;
	}
#endif

	HAGGLE_DBG("%u data objects matched query\n", total_match);

	return num_match;
}

// ----- Dataobject > Nodes

int SQLDataStore::_doNodeQuery(DataStoreNodeQuery *q)
{
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;
	char *sql_cmd = sqlcmd;
	unsigned int num_match = 0;
	DataStoreQueryResult *qr;
	DataObjectRef dObj = q->getDataObject();
	
	HAGGLE_DBG("DataStore Node Query for dataobject=%s\n", dObj->getIdStr());
	
	qr = new DataStoreQueryResult();
	
	if (!qr) {
		HAGGLE_DBG("Could not allocate query result object\n");
		return -1;
	}
	
	qr->addDataObject(dObj);
	qr->setQuerySqlStartTime();
	qr->setQueryInitTime(q->getQueryInitTime());
	
	sqlite_int64 dataobject_rowid = getDataObjectRowId(dObj);
	
	/* limit the dataobject attribute links */
	setViewLimitedDataobjectAttributes(dataobject_rowid);
	
	/* the actual query */
	if (q->getMaxResp() > 0) {
		snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "SELECT * FROM %s WHERE ratio >= %u AND mcount >= %u AND dataobject_not_match=0 limit %u;", VIEW_MATCH_DATAOBJECTS_AND_NODES_AS_RATIO, q->getRatio(), q->getAttrMatch(), q->getMaxResp());
	} else {
		snprintf(sqlcmd, SQL_MAX_CMD_SIZE, "SELECT * FROM %s WHERE ratio >= %u AND mcount >= %u AND dataobject_not_match=0;", VIEW_MATCH_DATAOBJECTS_AND_NODES_AS_RATIO, q->getRatio(), q->getAttrMatch());
	}
	
	ret = sqlite3_prepare(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);
	
	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", sql_cmd);
		goto out_err;
	}

	/* looping through the results and allocating nodes */
	while ((ret = sqlite3_step(stmt)) != SQLITE_DONE) {
		if (num_match == 0) {
			qr->setQuerySqlEndTime();
		}
		
		if (ret == SQLITE_ROW) {
			sqlite_int64 nodeRowId = sqlite3_column_int64(stmt, view_match_dataobjects_and_nodes_as_ratio_node_rowid);
			
			//HAGGLE_DBG("node rowid=%ld\n", nodeRowId);
			
			NodeRef node = getNodeFromRowId(nodeRowId);
			
			/*
			 Only consider peers and gateways as targets.
			 Application nodes receive data objects via their
			 filters....
			*/
			if (node && (node->getType() == NODE_TYPE_PEER || node->getType() == NODE_TYPE_GATEWAY)) {
				qr->addNode(node);
				num_match++;
			}
		} else if (ret == SQLITE_ERROR) {
			HAGGLE_DBG("node query Error:%s\n", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			goto out_err;
		}
	}
	
	sqlite3_finalize(stmt);
	
	if (num_match == 0) {
		qr->setQuerySqlEndTime();
	}
	qr->setQueryResultTime();
	
#if !defined(BENCHMARK)
	if (num_match) {
		kernel->addEvent(new Event(q->getCallback(), qr));
	} else {
		delete qr;
	}
#else
	kernel->addEvent(new Event(q->getCallback(), qr));
#endif
	
	HAGGLE_DBG("%u data objects matched query\n", num_match);
	
	return num_match;
out_err:
	
	HAGGLE_DBG("Data object query error, abort!\n");
	
	if (qr)
		delete qr;
	
	return -1;
}



/* ========================================================= */
/* Repository Methods                                        */
/* ========================================================= */

int SQLDataStore::_insertRepository(DataStoreRepositoryQuery *q)
{
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;
	char *sql_cmd = sqlcmd;
	const RepositoryEntryRef query = q->getQuery();
	
	// Prepare a select statemen to see if we should update or insert
	snprintf(sql_cmd, SQL_MAX_CMD_SIZE, "SELECT * FROM " TABLE_REPOSITORY " WHERE authority='%s' AND key='%s';", 
		 query->getAuthority(), 
		 query->getKey());
	
	ret = sqlite3_prepare_v2(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);
	
	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", sql_cmd);
		return -1;
	}
	
	ret = sqlite3_step(stmt);
	
	sqlite3_finalize(stmt);
	
	if (ret == SQLITE_ROW) {
		if (query->getId() > 0) {
			snprintf(sql_cmd, SQL_MAX_CMD_SIZE, "UPDATE " TABLE_REPOSITORY " SET authority='%s', key='%s', value='%s' WHERE rowid='%u' AND authority='%s';", 
				 query->getAuthority(), 
				 query->getKey(), 
				 query->getValue(),
				 query->getId(),
				 query->getAuthority());
		} else {
			snprintf(sql_cmd, SQL_MAX_CMD_SIZE, "UPDATE " TABLE_REPOSITORY " SET authority='%s', key='%s', value='%s' WHERE authority='%s' AND key='%s';", 
				 query->getAuthority(), 
				 query->getKey(), 
				 query->getValue(),
				 query->getAuthority(),
				 query->getKey());
		}
	} else if (ret == SQLITE_DONE) {
		snprintf(sql_cmd, SQL_MAX_CMD_SIZE, "INSERT INTO " TABLE_REPOSITORY " (authority, key, value) VALUES ('%s', '%s', '%s');", 
			 query->getAuthority(), 
			 query->getKey(), 
			 query->getValue());		
	} else {
		// Did something bad occur?
		return -1;
	}
	
	ret = sqlite3_prepare_v2(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);
	
	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", sql_cmd);
		return -1;
	}
	
	ret = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	
	return 1;
}

int SQLDataStore::_readRepository(DataStoreRepositoryQuery *q, const EventCallback<EventHandler> *callback)
{
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;
	char *sql_cmd = sqlcmd;
	
	const RepositoryEntryRef query = q->getQuery();
	DataStoreQueryResult *qr = new DataStoreQueryResult();
	
	if (!qr) {
		HAGGLE_ERR("Could not allocate query result object\n");
		return -1;
	}
	
	if (!query->getAuthority()) {
		HAGGLE_ERR("Error: No authority in repository entry\n");
		return -1;
	}
	
	if (query->getKey() && query->getId() > 0) {
		snprintf(sql_cmd, SQL_MAX_CMD_SIZE, "SELECT * FROM " TABLE_REPOSITORY " WHERE authority='%s' AND key LIKE '%s' AND id=%u;", 
				query->getAuthority(), 
				query->getKey(),
				query->getId());	
	} else if (query->getKey()) {
		snprintf(sql_cmd, SQL_MAX_CMD_SIZE, "SELECT * FROM " TABLE_REPOSITORY " WHERE authority='%s' AND key LIKE '%s';", 
				query->getAuthority(), 
				query->getKey());	
	} else if (query->getId() > 0) {
		snprintf(sql_cmd, SQL_MAX_CMD_SIZE, "SELECT * FROM " TABLE_REPOSITORY " WHERE authority='%s' AND id='%u';", 
				query->getAuthority(), 
				query->getId());	
	} else {
		snprintf(sql_cmd, SQL_MAX_CMD_SIZE, "SELECT * FROM " TABLE_REPOSITORY " WHERE authority='%s';", 
				query->getAuthority());	
	}
	
	ret = sqlite3_prepare_v2(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);
	
	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", sql_cmd);
		return -1;
	}
	
	
	RepositoryEntryRef re;
	
	while ((ret = sqlite3_step(stmt)) != SQLITE_DONE) {
		if (ret == SQLITE_ROW) {
			
			unsigned int id = (unsigned int)sqlite3_column_int64(stmt, table_repository_rowid);
			const char* authority = (const char*)sqlite3_column_text(stmt, table_repository_authority);
			const char* key = (const char*)sqlite3_column_text(stmt, table_repository_key);
			const char* value = (const char*)sqlite3_column_text(stmt, table_repository_value);
			
			re = RepositoryEntryRef(new RepositoryEntry(authority, key, value, id));
			qr->addRepositoryEntry(re);
		} else if (ret == SQLITE_ERROR) {
			HAGGLE_DBG("query Error:%s\n", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			return -1;
		}
	}
	sqlite3_finalize(stmt);
		
	kernel->addEvent(new Event(q->getCallback(), qr));
	
	return 1;
}

int SQLDataStore::_deleteRepository(DataStoreRepositoryQuery *q)
{
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;
	char *sql_cmd = sqlcmd;
	
	const RepositoryEntryRef query = q->getQuery();
	
	if (query->getId() > 0) {
		snprintf(sql_cmd, SQL_MAX_CMD_SIZE, "DELETE FROM " TABLE_REPOSITORY " WHERE authority='%s' AND key='%s' AND rowid = %u;", 
				query->getAuthority(), 
				query->getKey(),
				query->getId());	
	} else {
		snprintf(sql_cmd, SQL_MAX_CMD_SIZE, "DELETE FROM " TABLE_REPOSITORY " WHERE authority='%s' AND key='%s';", 
				query->getAuthority(), 
				query->getKey());	
	}	
	
	ret = sqlite3_prepare_v2(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);
	
	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", sql_cmd);
		return -1;
	}
	
	ret = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	
	return 1;
}

/* ========================================================= */
/* Functions to dump Datastore                               */
/* ========================================================= */

#include <libxml/tree.h>

static int dumpColumn(xmlNodePtr tableNode, sqlite3_stmt *stmt)
{
	if (!tableNode)
		return -1;

	const unsigned char* rowid = sqlite3_column_text(stmt, 0);

	xmlNodePtr rowNode = xmlNewNode(NULL, BAD_CAST "entry"); 

	if (!rowNode)
		goto xml_alloc_fail;

	if (!xmlNewProp(rowNode, BAD_CAST "rowid", (const xmlChar*) rowid)) {
		xmlFreeNode(rowNode);
		goto xml_alloc_fail;
	}

	for (int c = 1; c < sqlite3_column_count(stmt); c++) {
		if (sqlite3_column_type(stmt, c) != SQLITE_BLOB) {
			// Does the column name begin with XML?
			if (strncmp(sqlite3_column_name(stmt, c), "xml", 3) != 0) {
				// No. Standard case, do things normally:
				if (xmlNewTextChild(rowNode, NULL, 
					BAD_CAST sqlite3_column_name(stmt, c),
					BAD_CAST sqlite3_column_text(stmt, c)) == NULL)
					goto xml_alloc_fail;
			} else {
				/*
					Yes. 
					
					Here we handle table columns that begin with "xml" as 
					containing valid XML code. In this case we make sure to 
					print it into the new XML document as raw text, without any
					processing of special characters.
					
					The code below is unneccesarily complex, but I couldn't find
					any other way to do what the code below does.
				*/
				const char *text = (const char *) sqlite3_column_text(stmt, c);
				xmlDocPtr doc = xmlParseMemory(text, strlen(text));

				if (!doc) {
					HAGGLE_ERR("Could not parse xml document\n");
					goto xml_alloc_fail;
				}
				xmlNodePtr node = xmlNewChild(rowNode, NULL, 
						BAD_CAST sqlite3_column_name(stmt, c), NULL);

				if (!node) {
					xmlFreeDoc(doc);
					goto xml_alloc_fail;
				}
				if (!xmlAddChild(node, xmlCopyNode(xmlDocGetRootElement(doc), 1))) {
					xmlFreeDoc(doc);
					goto xml_alloc_fail;
				}
				xmlFreeDoc(doc);
			}
		} else if (!strcmp(sqlite3_column_name(stmt,c), "id")) {
			char* str = (char*)malloc(2 * sqlite3_column_bytes(stmt, c) + 1);
			buf2str((const char *)sqlite3_column_blob(stmt, c), str, sqlite3_column_bytes(stmt, c));
			
			if (!xmlNewTextChild(rowNode, NULL, BAD_CAST sqlite3_column_name(stmt, c), BAD_CAST str)) {
				free(str);
				goto xml_alloc_fail;
			}
			free(str);
		}
	}

	if (!xmlAddChild(tableNode, rowNode))
		goto xml_alloc_fail;

        return 0;

xml_alloc_fail:
	HAGGLE_ERR("XML allocation failure\n");
	return -1;
}

static int dumpTable(xmlNodePtr root_node, sqlite3 *db, const char* name) 
{
	sqlite3_stmt* stmt;
	const char* tail;
	int ret = 0;
	char* sql_cmd = &sqlcmd[0];

	if (!root_node || !db || !name)
		return -1;

	sprintf(sql_cmd, "SELECT * FROM %s;", name);
	ret = sqlite3_prepare_v2(db, sql_cmd, (int)strlen(sql_cmd), &stmt, &tail);
	
	if (ret != SQLITE_OK) {
		HAGGLE_DBG("SQLite command compilation failed! %s\n", sql_cmd);
		HAGGLE_DBG("%s\n", sqlite3_errmsg(db));
		return -1;
	}
	
	xmlNodePtr node = xmlNewNode(NULL, BAD_CAST name);
	
	if (!node) {
		goto xml_alloc_fail;
	}

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		dumpColumn(node, stmt);
	}

	if (ret != SQLITE_DONE) {
		HAGGLE_DBG("SQLite statement evaluation failed! %s\n", sql_cmd);
		HAGGLE_DBG("%s\n", sqlite3_errmsg(db));
                return -1;
	}
	
	if (!xmlAddChild(root_node, node)) {
		xmlFreeNode(node);
		goto xml_alloc_fail;
	}

        return 0;
	
xml_alloc_fail:
	HAGGLE_ERR("XML allocation failure when dumping table %s\n", name);
	return -1;
}

xmlDocPtr SQLDataStore::dumpToXML()
{
	xmlDocPtr doc = NULL;
	xmlNodePtr root_node = NULL;

	HAGGLE_DBG("Dumping data base to XML\n");

	doc = xmlNewDoc(BAD_CAST "1.0");

	if (!doc) {
		HAGGLE_ERR("Could not allocate new XML document\n");
		return NULL;
	}
	root_node = xmlNewNode(NULL, BAD_CAST "HaggleDump");

	if (!root_node) {
		goto xml_alloc_fail;
	}
	
	if (!xmlDocSetRootElement(doc, root_node))
		goto xml_alloc_fail;

	if (!dumpTable(root_node, db, TABLE_ATTRIBUTES))
		goto xml_alloc_fail;

	if (!dumpTable(root_node, db, TABLE_DATAOBJECTS))
		goto xml_alloc_fail;

	if (!dumpTable(root_node, db, TABLE_NODES))
		goto xml_alloc_fail;

	if (!dumpTable(root_node, db, TABLE_FILTERS))
		goto xml_alloc_fail;

	if (!dumpTable(root_node, db, TABLE_MAP_DATAOBJECTS_TO_ATTRIBUTES_VIA_ROWID))
		goto xml_alloc_fail;	

	if (!dumpTable(root_node, db, TABLE_MAP_NODES_TO_ATTRIBUTES_VIA_ROWID))
		goto xml_alloc_fail;

	if (!dumpTable(root_node, db, TABLE_MAP_FILTERS_TO_ATTRIBUTES_VIA_ROWID))
		goto xml_alloc_fail;

//	setViewLimitedDataobjectAttributes();
//	dumpTable(root_node, db, VIEW_MATCH_DATAOBJECTS_AND_NODES_AS_RATIO);
//	dumpTable(root_node, db, VIEW_MATCH_FILTERS_AND_DATAOBJECTS_AS_RATIO);

	HAGGLE_DBG("Dump done\n");

        return doc;

xml_alloc_fail:
	HAGGLE_ERR("XML allocation failure when dumping data store\n");
	xmlFreeDoc(doc);
	return NULL;
}

int SQLDataStore::_dump(const EventCallback<EventHandler> *callback)
{
        xmlDocPtr doc;
        char *dump;
        int xmlLen;
        
        if (!callback) {
                HAGGLE_ERR("Invalid callback\n");
                return -1;
        }
        doc = dumpToXML();
        
        if (!doc) {
                HAGGLE_ERR("ERROR: Dump to XML failed\n");
                return -2;
        }
	xmlDocDumpFormatMemory(doc, (xmlChar **)&dump, &xmlLen, 1);

        xmlFreeDoc(doc);

	if (xmlLen < 0) {
                HAGGLE_ERR("ERROR: xmlLen is less than zero...\n");
                return -3;
        }

        kernel->addEvent(new Event(callback, new DataStoreDump(dump, xmlLen)));
	
	return xmlLen;
}

int SQLDataStore::_dumpToFile(const char *filename)
{
        xmlDocPtr doc = dumpToXML();

        if (!doc)
                return -1;

	xmlSaveFormatFileEnc(filename, doc, "UTF-8", 1);
        xmlFreeDoc(doc);

	return 0;
}


#ifdef DEBUG_SQLDATASTORE

/* ========================================================= */
/* Print tables                                              */
/* ========================================================= */

static void table_dataobjects_print(sqlite3 *db)
{
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;
	const char *sql_cmd = "SELECT * FROM " TABLE_DATAOBJECTS ";";
	
	ret = sqlite3_prepare_v2(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);
	
	if (ret != SQLITE_OK) {
		fprintf(stderr, "SQLite command compilation failed! %s\n", sql_cmd);
		return;
	}
	
	printf("%-5s %-10s %-8s %-10s %-10s %s\n", 
		   "rowid", "datalen", "num attr", "rxtime", "timestamp", "filepath");
	
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		/*
		 char idstr[MAX_DATAOBJECT_ID_STR_LEN];
		 const char *id = (const char *)sqlite3_column_blob(stmt, table_dataobjects_id);
		 int len = 0;
		 
		 memset(idstr, '\0', MAX_DATAOBJECT_ID_STR_LEN);
		 
		 for (int i = 0; i < DATAOBJECT_ID_LEN; i++) {
		 len += sprintf(idstr + len, "%02x", id[i] & 0xff);
		 }
		 */
		sqlite_int64 rowid = sqlite3_column_int64(stmt, table_dataobjects_rowid);
		sqlite_int64 datalen = sqlite3_column_int64(stmt, table_dataobjects_datalen);
		sqlite_int64 numattr = sqlite3_column_int64(stmt, table_dataobjects_num_attributes);
		sqlite_int64 rxtime = sqlite3_column_int64(stmt, table_dataobjects_rxtime);
		sqlite_int64 timestamp = sqlite3_column_int64(stmt, table_dataobjects_timestamp);
		const char *filepath = (const char *)sqlite3_column_text(stmt, table_dataobjects_filepath);
		printf("%-5" INT64_FORMAT " %-10" INT64_FORMAT " %-8" INT64_FORMAT " %-10" INT64_FORMAT " %-10" INT64_FORMAT " %s\n", rowid, datalen, numattr, rxtime, timestamp, filepath);

                /*
		cout << left << setw(6) << rowid 
		<< setw(11) << datalen
		<< setw(9) << numattr
		<< setw(11) << rxtime
		<< setw(11) << timestamp
		<< filepath << endl;
                */
	};
	
	sqlite3_finalize(stmt);
}

static void view_dataobject_attributes_as_namevalue_print(sqlite3 *db)
{
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;
	const char *sql_cmd = "SELECT * FROM " VIEW_DATAOBJECT_ATTRIBUTES_AS_NAMEVALUE ";";
	
	ret = sqlite3_prepare_v2(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);
	
	if (ret != SQLITE_OK) {
		fprintf(stderr, "SQLite command compilation failed! %s\n", sql_cmd);
		return;
	}
	
	printf("%-10s %-20s %-20s\n", 
		   "do_rowid", "name", "value");
	
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		
		sqlite_int64 dorowid = sqlite3_column_int64(stmt, view_dataobjects_attributes_as_namevalue_dataobject_rowid);
		const char *name = (const char *)sqlite3_column_text(stmt, view_dataobjects_attributes_as_namevalue_name);
		const char *value = (const char *)sqlite3_column_text(stmt, view_dataobjects_attributes_as_namevalue_value);
		
	printf("%-10" INT64_FORMAT " %-20s %-20s\n",
               dorowid, name, value);
        /*
		cout << left << setw(11) << dorowid
		<< setw(21)<< name
		<< value << endl;
        */
	};
	
	sqlite3_finalize(stmt);
}

static void view_node_attributes_as_namevalue_print(sqlite3 *db)
{
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;
	const char *sql_cmd = "SELECT * FROM " VIEW_NODE_ATTRIBUTES_AS_NAMEVALUE ";";
	
	ret = sqlite3_prepare_v2(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);
	
	if (ret != SQLITE_OK) {
		fprintf(stderr, "SQLite command compilation failed! %s\n", sql_cmd);
		return;
	}
	
	printf("%-10s %-20s %-20s\n", 
		   "node_rowid", "name", "value");
	
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		
		sqlite_int64 noderowid = sqlite3_column_int64(stmt, view_node_attributes_as_namevalue_node_rowid);
		const char *name = (const char *)sqlite3_column_text(stmt, view_node_attributes_as_namevalue_name);
		const char *value = (const char *)sqlite3_column_text(stmt, view_node_attributes_as_namevalue_value);
		
                printf("%-10" INT64_FORMAT " %-20s %-20s\n" noderowid, name, value);
                /*
                  cout << left << setw(11) << noderowid
                  << setw(21) << name
                  << value << endl;
                */
	};
	
	sqlite3_finalize(stmt);
}


static void table_nodes_print(sqlite3 *db)
{
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;
	const char *sql_cmd = "SELECT * FROM " TABLE_NODES ";";
	
	ret = sqlite3_prepare_v2(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);
	
	if (ret != SQLITE_OK) {
		fprintf(stderr, "SQLite command compilation failed! %s\n", sql_cmd);
		return;
	}
	
	printf("%-5s %-15s %-10s %-10s\n", 
		   "rowid", "type", "num attr", "timestamp");
	
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		
		sqlite_int64 rowid = sqlite3_column_int64(stmt, table_nodes_rowid);
		const char *typestr = Node::typeToStr((NodeType_t)sqlite3_column_int64(stmt, table_nodes_type));
		sqlite_int64 numattr = sqlite3_column_int64(stmt, table_nodes_num_attributes);
		sqlite_int64 timestamp = sqlite3_column_int64(stmt, table_nodes_timestamp);
		//const char *idstr = (const char *)sqlite3_column_text(stmt, table_nodes_id_str);
		
                printf("%-5" INT64_FORMAT " %-15s %-10" INT64_FORMAT " %-10" INT64_FORMAT "\n", 
                       rowid, typestr, numattr);
                /*
                  cout << left << setw(6) << rowid 
                  << setw(16) << typestr 
                  << setw(11) << numattr 
                  << timestamp << endl;
                */
	};
	
	sqlite3_finalize(stmt);
}

static void table_attributes_print(sqlite3 *db)
{
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;
	const char *sql_cmd = "SELECT * FROM " TABLE_ATTRIBUTES ";";
	
	ret = sqlite3_prepare_v2(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);
	
	if (ret != SQLITE_OK) {
		fprintf(stderr, "SQLite command compilation failed! %s\n", sql_cmd);
		return;
	}
	
	printf("%-5s %-20s %-20s %-20s\n", 
		   "rowid", "name", "value", "weight");
	
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		sqlite_int64 rowid = sqlite3_column_int64(stmt, table_attributes_rowid);
		const char *name = (const char *)sqlite3_column_text(stmt, table_attributes_name);
		const char *value = (const char *)sqlite3_column_text(stmt, table_attributes_text);
		// WARNING: FIXME: THIS COLUMN DOESN'T EXIST!
		const unsigned long weight = (const unsigned long)sqlite3_column_int(stmt, 3);
		
                printf("%-5" INT64_FORMAT " %-20s %-20s %-20lu\n",
                       rowid, name, value, weight);
        };
	
	sqlite3_finalize(stmt);
}

static void table_filters_print(sqlite3 *db)
{
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;
	const char *sql_cmd = "SELECT * FROM " TABLE_FILTERS ";";
	
	ret = sqlite3_prepare_v2(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);
	
	if (ret != SQLITE_OK) {
		fprintf(stderr, "SQLite command compilation failed! %s\n", sql_cmd);
		return;
	}
	
	printf("%-5s %-10s %-8s %s\n", 
		   "rowid", "event type", "num attr", "timestamp");
	
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		
		sqlite_int64 rowid = sqlite3_column_int64(stmt, table_filters_rowid);
		sqlite_int64 eventtype = sqlite3_column_int64(stmt, table_filters_event);
		sqlite_int64 numattr = sqlite3_column_int64(stmt, table_filters_num_attributes);
		sqlite_int64 timestamp = sqlite3_column_int64(stmt, table_filters_timestamp);
		
                printf("%-5" INT64_FORMAT " %-10" INT64_FORMAT " %-8" INT64_FORMAT " %8" INT64_FORMAT "\n", rowid, eventtype, numattr, timestamp);

                /*
                  cout << left << setw(6) << rowid 
                  << setw(11) << eventtype 
                  << setw(9) << numattr 
                  << timestamp << endl;
                */
	};
	
	sqlite3_finalize(stmt);
}


static void table_map_filters_to_attributes_via_rowid_print(sqlite3 *db)
{
	int ret;
	sqlite3_stmt *stmt;
	const char *tail;
	const char *sql_cmd = "SELECT * FROM " TABLE_MAP_FILTERS_TO_ATTRIBUTES_VIA_ROWID ";";
	
	ret = sqlite3_prepare_v2(db, sql_cmd, (int) strlen(sql_cmd), &stmt, &tail);
	
	if (ret != SQLITE_OK) {
		fprintf(stderr, "SQLite command compilation failed! %s\n", sql_cmd);
		return;
	}
	
	printf("%-5s %-15s %-15s\n", 
		   "rowid", "filter_rowid", "attr_rowid");
	
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		
		sqlite_int64 rowid = sqlite3_column_int64(stmt, table_map_filters_to_attributes_via_rowid_rowid);
		sqlite_int64 filterrowid = sqlite3_column_int64(stmt, table_map_filters_to_attributes_via_rowid_filter_rowid);
		sqlite_int64 attrrowid = sqlite3_column_int64(stmt, table_map_filters_to_attributes_via_rowid_attr_rowid);
		
                printf("%-5" INT64_FORMAT " %-15" INT64_FORMAT " %-15" INT64_FORMAT "\n", 
                       rowid, filterrowid, attrrowid);
                /*
                  cout << left << setw(6) << rowid 
                  << setw(16) << filterrowid 
                  << attrrowid << endl;
                */
	};
	
	sqlite3_finalize(stmt);
}


void SQLDataStore::_print()
{
	printf("================ DataStore ================\n");
	
	printf("###########################################\n");
	printf("* %s:\n", TABLE_DATAOBJECTS);
	printf("-------------------------------------------\n");
	table_dataobjects_print(db);
	printf("-------------------------------------------\n");
	
	printf("###########################################\n");
	printf("* %s:\n", VIEW_DATAOBJECT_ATTRIBUTES_AS_NAMEVALUE);
	printf("-------------------------------------------\n");
	view_dataobject_attributes_as_namevalue_print(db);
	printf("-------------------------------------------\n");
	
	printf("###########################################\n");
	printf("* %s:\n", TABLE_NODES);
	printf("-------------------------------------------\n");
	table_nodes_print(db);
	printf("-------------------------------------------\n");
	
	printf("###########################################\n");
	printf("* %s:\n", VIEW_NODE_ATTRIBUTES_AS_NAMEVALUE);
	printf("-------------------------------------------\n");
	view_node_attributes_as_namevalue_print(db);
	printf("-------------------------------------------\n");
	
	printf("###########################################\n");
	printf("* %s:\n", TABLE_ATTRIBUTES);
	printf("-------------------------------------------\n");
	table_attributes_print(db);
	printf("-------------------------------------------\n");
	
	printf("###########################################\n");
	printf("*%s:\n", TABLE_FILTERS);
	printf("-------------------------------------------\n");
	table_filters_print(db);
	printf("-------------------------------------------\n");
	
	printf("###########################################\n");
	printf("* %s:\n", TABLE_MAP_FILTERS_TO_ATTRIBUTES_VIA_ROWID);
	printf("-------------------------------------------\n");
	table_map_filters_to_attributes_via_rowid_print(db);
	printf("-------------------------------------------\n");
	
	printf("============== DataStore End ===============\n");
}

#endif /* DEBUG_SQLDATASTORE */


