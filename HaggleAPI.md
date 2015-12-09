# Introduction #

Haggle provides a search-based data dissemination among nodes where content spreads according to how well it ranks against a user's declared interests. Haggle offers a publish/subscribe-like interface to disseminate the data. It allows the application developer to insert data with its associated metadata into the network, and register persistent queries expressing the user's interest.

libhaggle is a C library, wrappers for [Java](http://code.google.com/p/haggle/wiki/HaggleJavaAPI) and C# are provided. An easy way to get familiar with libhaggle is to have a look in the example applications.

# libhaggle #

This page gives an overview of libhaggle and details the low level C API. For details we refer to the API documenation.

For access to the API, one needs to have libhaggle installed on a system and have the headers in the include path. Then include the following line in your application:

```
#include <libhaggle/haggle.h>
```

## Obtaining a handle to Haggle ##
At the beginning of your application you have to obtain a handle to Haggle:

```
// Declare handle
haggle_handle_t hh;

haggle_handle_get("yourApplicationName", &hh);
```

The handle should be released when your application exits:

```
haggle_handle_free(hh);
```


## Publish data to the network ##

Publish data objects through Haggle:

```
haggle_ipc_publish_dataobject(hh, dobj);
```


## Expressing interests in data ##

Data is retrieved from the network by expressing an application interest. The interest itself will also be disseminated by Haggle so that other nodes learn about it. Expressing an interest can be seen as a persistent search query.

```
haggle_ipc_add_application_interests(hh, attribute_list);
```

Matching data objects are received by event notification (see below, LIBHAGGLE\_EVENT\_NEW\_DATAOBJECT).

## Receiving events from Haggle ##

It is possible to get event notifications from Haggle:
```
haggle_ipc_register_event_interest(hh, LIBHAGGLE_EVENT_TYPE, callback_function);

haggle_ipc_register_event_interest_with_arg(hh, LIBHAGGLE_EVENT_TYPE, callback_function, &mydata);
```

The above functions are equivalent apart from the optional `arg` parameter, which allows the application writer to give a pointer to a data structure that will be passed back to the application in the callback function. This is useful in case one uses a single callback function for all events and wants to be passed event specific data.

All events sent by Haggle are described in data objects, and hence every callback function has the same template as shown further down.

The following events are currently supported by libhaggle:

```
// Shutdown event: haggle is shutting down.
LIBHAGGLE_EVENT_SHUTDOWN = 0,
// Neighbor update event.
LIBHAGGLE_EVENT_NEIGHBOR_UPDATE,
// New data object event.
LIBHAGGLE_EVENT_NEW_DATAOBJECT,
// Interest list
LIBHAGGLE_EVENT_INTEREST_LIST,
```

An example of a callback function for receiving data objects (Haggle v0.2):

```
void new_dataobject_event(haggle_dobj_t *dobj, void *arg)
{
	printf("New data object!\n");
	
	haggle_dataobject_free(dobj);
}
```

Note that all data objects received by an application also has to be managed by that application and hence freed when no longer needed. This has changed with Haggle 0.3 and since that version it is now possible to indicate that one would like to keep the data object by returning a value greater than 0. Otherwise the data object will be automatically freed after the handler function. For example,

```
int new_dataobject_event(haggle_dobj_t *dobj, void *arg)
{
	printf("New data object!\n");
	
        if (should_keep_data_object) {
                store_data_object(dobj);
                return 1;
        }
        
        return 0;
}
```

Do not forget to start an event loop in which the application waits for events from Haggle (see below).

## Utility functions ##

In case Haggle is not running, libhaggle allows to start Haggle from an application:

```

unsigned long pid;

int ret = haggle_daemon_pid(&pid);

if (ret == 0) {
	 printf("Haggle not running, trying to spawn daemon... ");
                
        if (haggle_daemon_spawn(NULL) != HAGGLE_NO_ERROR) {
	 	 printf("failed!\n");
	 	 exit(EXIT_FAILURE);
	}
	printf("success!\n");
} else if (pid == 1) {
        printf("Haggle daemon is running with pid=%lu\n", pid);
}
```

## Application event loop ##

An application usually has an event loop in which it waits for events from Haggle. For example, a typical application would wait for newly received data objects that match the application's interest. Event loops can be either synchronous or asynchronous, and libhaggle supports both approaches. In general, a synchronous event loop is an easier approach for simple single threaded applications that can block while nothing is happening. However, for, e.g., GUI applications it may be more appropriate to have an asynchronous approach, which does not block the application. However, in the asynchronous case the callback functions will be called from a different thread than the application's "main" thread, and proper protection has to be ensured for global data structures, etc.

```
haggle_event_loop_run_async(hh);
```