# Runtime Configuration Options for Haggle #

There are many configuration parameters and options that affect Haggle's behavior, and many of them can be set at runtime using a special "configuration data object" (`config.xml`). This is essentially a data object (according to Haggle's XML format) that contains information about how Haggle should be configured in each manager. Below is an example configuration object with an overview and explanation of what options can be set. A more complete example configuration object can be found under `resources/` in the source code repository.

The configuration data object must be tagged with an attribute having the name "ManagerConfiguration" in order for the object to be recognized by Haggle as a configuration object. The object should be put in the folder where Haggle's data store is located (e.g., $HOME/.Haggle on GNU/Linux, or /data/data/org.haggle.kernel/files on Android).

## Example Configuration Data Object ##
```
<Haggle persistent="no">
        <Attr name="ManagerConfiguration">*</Attr>
        <SecurityManager security_level="MEDIUM"/>
        <DebugManager>
                <DebugTrace enable="true"/>
        </DebugManager>
        <NodeManager>
                <Node matching_threshold="0" max_dataobjects_in_match="10"/>
                <NodeDescriptionRetry retries="3" retry_wait="10.0"/>
        </NodeManager>
        <ProtocolManager>
                <TCPServer port="9697" backlog="30"/>
        </ProtocolManager>
        <DataManager set_createtime_on_bloomfilter_update="true">
                <Aging period="3600" max_age="86400"/>
                <Bloomfilter default_error_rate="0.01" default_capacity="2000"/>
        </DataManager>
        <ConnectivityManager>
          <Bluetooth scan_base_time="120" scan_random_time="60" read_remote_name="false" />
        </ConnectivityManager>
        <ForwardingManager query_on_new_dataobject="false" periodic_dataobject_query_interval="0">
          <Forwarder max_generated_delegates="1" max_generated_targets="1" protocol="Prophet">
            <Prophet strategy="GRTR" P_encounter="0.75" alpha="0.5" beta="0.25" gamma="0.999"/>
          </Forwarder>
        </ForwardingManager>
</Haggle>
```

### Security Manager ###

```
<SecurityManager security_level="MEDIUM"/>
```

This sets the security level used in Haggle. Valid settings are HIGH, MEDIUM, LOW. For an explanation of these settings, see HaggleSecurity.

### Debug Manager ###

```
<DebugTrace enable="true"/>
```

This option sets whether Haggle should write its debug information to a `haggle.log` trace file or not. Set to `true` or `false`.

### Node Manager ###

```
<Node matching_threshold="0" max_dataobjects_in_match="10"/>
```

These options are put in a node's node description when sent to other nodes. A receiving node uses these parameters when performing search resolutions on behalf of the originating node. The parameter`matching_threshold` indicates the minimum rating a data object should have to be considered a match when performing a search resolution. All data objects above this threshold are but in a list and ranked according to their rating. A threshold of 0 (zero) thus includes all objects that have some kind of match (e.g., at least one matching attribute). The `max_dataobjects_in_match` then determines how many of the top ranked objects in this list to return as a result of the search resolution (these are typically the data objects disseminated).

```
<NodeDescriptionRetry retries="3" retry_wait="10.0"/>
```

These options set how many times a node should try to send its node description when gaining a new neighbor. The `retry_wait` sets the waiting time in seconds between each retry. A node description transmission can fail due to temporary disconnection, overload on the receiving node, or other factors.

### Protocol Manager ###

```
<TCPServer port="9697" backlog="30"/>
```

This simply sets the port to listen on for incoming TCP connections and the backlog to use for accepting them.

### Data Manager ###

```
<DataManager set_createtime_on_bloomfilter_update="true">
```

Set to either `true` or `false` depending on whether Haggle should timestamp its node description each time it adds a new object to its bloomfilter. A fresh timestamp on the node description will mark it as "updated" (essentially making it a new object) and force it to be re-disseminated, although the node's interests might not have changed. The reason why this might be desirable is that the node description will contain the updated bloomfilter so that other nodes can learn what data objects the originating node already has stored. This will stop them from unnecessarily pushing content to the originating node. Setting this to `true`, however, also increases the number of node descriptions disseminated.

```
<Aging period="3600" max_age="86400"/>
```

These options affect the garbage collection of old data objects stored on a devices. The `period` sets the number of seconds between each scan through the data store that looks for old data objects to delete. The `max_age` sets the age threshold in seconds that each data object must have to be considered for deletion.

```
<Bloomfilter default_error_rate="0.01" default_capacity="2000"/>
```

These parameters configure the node's bloomfilter. The `default_error_rate` sets the error rate (likelihood of false positives) that the filter should have. The`default_capacity` sets the capacity of the filter, i.e., the number of objects it should be able to hold before the error rate (likelihood of false positives) starts to be seriously affected. Both parameters affect the size of the filter and hence the memory requirements of the device and size of the node description (which contains the bloomfilter).

### Connectivity Manager ###

```
<Bluetooth scan_base_time="120" scan_random_time="60" read_remote_name="false" />
```

These settings configure the scanning scheme used to discover neighbors via Bluetooth. The parameter `scan_base_time` sets the time between each bluetooth scan, while `scan_random_time` sets the maximum random time (e.g., 0-60) to add or subtract from the base time. The final time between scans are calculated as follows:

` time_to_next_scan = scan_base_time - scan_random_time + 2 * rand(scan_random_time) `

The `read_remote_name` parameter simply indicates whether Bluetooth should try to read the remote Bluetooth name of the peer. This doesn't really affect anything except maybe the debug output and the length of scans.

### Forwarding Manager ###

```
<ForwardingManager query_on_new_dataobject="false" periodic_dataobject_query_interval="0">
```

These parameters control how Haggle performs search resolutions/queries, which in turn affect how data objects are disseminated. See the [FAQ](FAQ#How_are_disseminations_triggered?.md) for a description of how dissemination works. The `query_on_new_dataobject` parameter can be set to `true` or `false` and indicates whether Haggle should try to disseminate a newly created data object to its current neighbors (if matching allows). The `periodic_dataobject_query_interval` indicates the number of seconds between performing additional search resolutions for neighbors (i.e., periodically triggering disseminations). Setting this to 0 (zero) disables this feature.

```
<Forwarder max_generated_delegates="1" max_generated_targets="1" protocol="Prophet">
```

The above parameters configure how Haggle performs delegation. Typically, each delegation forwarding algorithm maintains a quality number (e.g., between 0 and 100) that indicates how good a delegate a neighbor is for a particular "target" node. The `max_generated_delegates` parameter sets how many of current neighbors data objects should be delegated, while `max_generated_targets` sets how many distant nodes one should perform resolutions for. The `protocol` parameter sets which forwarding algorithm to use.

```
<Prophet strategy="GRTR" P_encounter="0.75" alpha="0.5" beta="0.25" gamma="0.999"/>
```

These settings are specific to the Prophet forwarding algorithm. These parameters are described in the Prophet draft/RFC.