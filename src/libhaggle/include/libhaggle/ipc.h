/* Copyright 2008 Uppsala University
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
#ifndef _LIBHAGGLE_IPC_H_
#define _LIBHAGGLE_IPC_H_

#include "exports.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
	The types of events that can be registered.
*/
enum event_types {
	// Shutdown event: haggle is shutting down.
	LIBHAGGLE_EVENT_HAGGLE_SHUTDOWN = 0,
	// Neighbor update event.
	LIBHAGGLE_EVENT_NEIGHBOR_UPDATE,
	// New data object event.
	LIBHAGGLE_EVENT_NEW_DATAOBJECT,
	// Interest list
	LIBHAGGLE_EVENT_INTEREST_LIST,
	// The number of possible events
	_LIBHAGGLE_NUM_EVENTS,
};

typedef struct haggle_handle *haggle_handle_t;

#if defined(OS_WINDOWS_MOBILE)
#define STDCALL __stdcall
#else
#define STDCALL 
#endif

/**
	A haggle event handler. Used to receive data objects or events 
	(which are also data objects).
	
	A data object given to the application in this manner is owned by the
	receiver. It is the receiving function's task to release the data object.
*/
typedef void (STDCALL *haggle_event_handler_t) (struct dataobject *, void *);


/**

*/
typedef void (STDCALL *haggle_event_loop_start_t) (void *);
typedef void (STDCALL *haggle_event_loop_stop_t) (void *);

/* Errors */
#define	LIBHAGGLE_ERR_BAD_HANDLE    0x01
#define	LIBHAGGLE_ERR_NOT_CONNECTED 0x02

/* Attribute name definitions */
#define HAGGLE_ATTR_CONTROL_NAME "HaggleIPC"  // <-- all messages should have at least this one.
#define HAGGLE_ATTR_APPLICATION_ID_NAME "ApplicationId"
#define HAGGLE_ATTR_APPLICATION_NAME_NAME "ApplicationName"
#define HAGGLE_ATTR_SESSION_ID_NAME "SessionId"
#define HAGGLE_ATTR_DATAOBJECT_ID_NAME "DataObjectId"
#define HAGGLE_ATTR_EVENT_TYPE_NAME "EventType"
#define HAGGLE_ATTR_EVENT_INTEREST_NAME "EventType"
#define HAGGLE_ATTR_HAGGLE_DIRECTORY_NAME "HaggleDirectory"

/* Attribute value definitions */
#define HAGGLE_ATTR_REGISTRATION_REPLY_VALUE "RegistrationReply"
#define HAGGLE_ATTR_REGISTRATION_REPLY_REGISTERED_VALUE "RegistrationReplyRegistered"
#define HAGGLE_ATTR_REGISTRATION_REQUEST_VALUE "RegistrationRequest"
#define HAGGLE_ATTR_REGISTER_EVENT_INTEREST_VALUE "RegisterEventInterest"
#define HAGGLE_ATTR_DEREGISTRATION_NOTICE_VALUE "DeregistrationNotice"
#define HAGGLE_ATTR_ADD_INTEREST_VALUE "AddInterests"
#define HAGGLE_ATTR_REMOVE_INTEREST_VALUE "RemoveInterests"
#define HAGGLE_ATTR_GET_INTERESTS_VALUE "GetInterests"
#define HAGGLE_ATTR_GET_DATAOBJECTS_VALUE "GetDataobjects"
#define HAGGLE_ATTR_DELETE_DATAOBJECT_VALUE "DeleteDataObject"
#define HAGGLE_ATTR_SHUTDOWN_VALUE "Shutdown"

/* Defines whether to expect a reply in response to a sent data object */
#define IO_NO_REPLY                -2
#define IO_REPLY_BLOCK             -1
#define IO_REPLY_NON_BLOCK          0

/* IPC API functions */

/**
	GENERAL NOTES:

	Most IPC functions return a value indicating success or failure. This value only 
	indicates if the operation was successful within "libhaggle" -- it does not say
	anything about the success of the operation in the Haggle daemon.

	Some operations will trigger a response from Haggle in the form of a control 
	data object. In that case, the application will be notified asynchronously through
	its event handlers. Any success of failure of the operations previously initiated
	may be indicated in those messages.
*/


/**
	Returns (if possible) a handle that can be used to communicate with haggle
	given a unique application name.
	
	Fills in the given haggle handle iff successful. Otherwise leaves the handle
	unmodified.

	@returns zero if successful, or an error code.
	
	Some specific error codes:
		HAGGLE_REGISTRATION_ERROR - Unable to establish contact with haggle
		HAGGLE_BUSY_ERROR - Haggle reported that there was already an 
			application with that name.
*/

HAGGLE_API int haggle_handle_get(const char *name, haggle_handle_t *handle);

/**
	Relinquishes a handle used to communicate with haggle.
	
	The handle will be invalid after calling this function.
*/
HAGGLE_API void haggle_handle_free(haggle_handle_t hh);


/**
	This function returns the Process ID (PID) of a running
        Haggle daemon.
	
	@returns the haggle daemon's PID, or 0 if it is not running.
*/
HAGGLE_API unsigned long haggle_daemon_pid();

/**
	This function can be used by an application to spawn a new
        Haggle daemon, if none was already running.
	
        @param daemonpath The path to the Haggle daemon executable, or
        NULL if libhaggle should try standard paths.

	@returns 0 if a haggle daemon is already running, 1 if a daemon 
        was spawned and -1 if an error occured.
*/
HAGGLE_API int haggle_daemon_spawn(const char *daemonpath);

/**
	This function can be used by an application to remove any previous 
	registration by a same-named application from haggle.
	
	PLEASE USE ONLY WHEN ABSOLUTELY CERTAIN THAT THERE IS NO OTHER APPLICATION.
	
        @param name the name of the application to unregister
	@returns an error code.
*/
HAGGLE_API int haggle_unregister(const char *name);


/**
	Get the session Id associated with the handle. 
	A valid handle has a positive session id.
*/
HAGGLE_API int haggle_handle_get_session_id(haggle_handle_t hh);

/**
	Publishes a data object into haggle.
	
	This function does not take possession of the data object.
	
	@returns an error code.
*/
HAGGLE_API int haggle_ipc_publish_dataobject(haggle_handle_t hh, struct dataobject *dobj);

/**
	Register interest in an event type. See event types above for the different
	event types that can be registered.
	
	If this data object matches any interest of this application, a copy of the
	data object will be given to the registered event handler.
	
	@returns an error code.
*/
HAGGLE_API int STDCALL haggle_ipc_register_event_interest(haggle_handle_t hh, const int eventId, haggle_event_handler_t handler);
HAGGLE_API int STDCALL haggle_ipc_register_event_interest_with_arg(haggle_handle_t hh, const int eventId, haggle_event_handler_t handler, void *arg);

/**
	Register interest in a particular attribute name/value combination.
	
	If there is an event handler registered to receive data objects, that event
	handler will be given any new data objects received by haggle that match
	any interest this application has received.
	
	@returns an error code.
*/
HAGGLE_API int haggle_ipc_add_application_interest(haggle_handle_t hh, const char *name, const char *value);

/**
	Register interest in a particular attribute name/value combination with weight.
	
	If there is an event handler registered to receive data objects, that event
	handler will be given any new data objects received by haggle that match
	any interest this application has received.
	
	@returns an error code.
*/
HAGGLE_API int haggle_ipc_add_application_interest_weighted(haggle_handle_t hh, const char *name, const char *value, const unsigned long weight);

/**
	Register several interests in the form of an attribute list.

	@returns an error code.

	Note: the attribute list will not be freed or modified. 
	It still has to be freed by the caller.
*/

HAGGLE_API int haggle_ipc_add_application_interests(haggle_handle_t hh, const struct attributelist *al);


/**
	Deregister interest in a particular attribute name/value combination.
	
	@returns an error code.
*/
HAGGLE_API int haggle_ipc_remove_application_interest(haggle_handle_t hh, const char *name, const char *value);

/**
	Deregister several interests in the form of a list of attributes
	
	@returns an error code.

	Note: the attribute list will not be freed or modified. 
	It still has to be freed by the caller.
*/
HAGGLE_API int haggle_ipc_remove_application_interests(haggle_handle_t hh, const struct attributelist *al);

/**
	Get the currently registered application interests for this application.
	The application interests will be asynchrounously returned in a response
	data object, which the applications receive in one of its event handlers.
	The response data object must be handled by the application itself.

	@returns positive value on success, negative (error code) on failure.
*/
HAGGLE_API int haggle_ipc_get_application_interests_async(haggle_handle_t hh);

/**
	Get the data objects that match the currently registered application 
	interests for this application. The application interests will be 
	asynchrounously returned as usual.

	@returns positive value on success, negative (error code) on failure.
*/
HAGGLE_API int haggle_ipc_get_data_objects_async(haggle_handle_t hh);

/**
	Delete a data object that is managed by Haggle. After this call,
	Haggle will no longer store this data object, but it may still exist
	in the application (i.e., the data object given is not deleted in 
	the application).

	@returns a positive value on success, negative (error code) on failure.
	A success of this function does not necessarily mean that the data object
	was deleted from Haggle. It only means that this asynchronous call was
	successful.

	NOTE: This function is not yet implemented.
*/
HAGGLE_API int haggle_ipc_delete_data_object_by_id(haggle_handle_t hh, const unsigned char id[20]);
HAGGLE_API int haggle_ipc_delete_data_object(haggle_handle_t hh, const struct dataobject *dobj);

/**
	Send shutdown event to Haggle daemon.

	@returns an error code.
*/
HAGGLE_API int haggle_ipc_shutdown(haggle_handle_t hh);

/* Event loop */
/**
	Checks if the event loop is running.
	@returns 1 if the event loop is running, 0 if it is not, or an error code on error.
*/

HAGGLE_API int haggle_event_loop_is_running(haggle_handle_t hh);
/**
	Starts the haggle event loop. This function runs synchronously, and as such
	will not  return until the haggle_event_loop_stop function is called.
	
	If an event loop is already running for the given haggle handle...?
	
	@returns an error code.
*/

HAGGLE_API int haggle_event_loop_run(haggle_handle_t hh);
/**
	Starts running the haggle event loop asynchronously. The haggle event loop
	can be stopped by a call to haggle_event_loop_stop.
	
	If an event loop is already running for the given haggle handle...?
	
	@returns an error code.
*/
HAGGLE_API int haggle_event_loop_run_async(haggle_handle_t hh);
/**
	Stops a running haggle event loop.
	
	@returns an error code.
*/
HAGGLE_API int haggle_event_loop_stop(haggle_handle_t hh);

HAGGLE_API int haggle_event_loop_run_async(haggle_handle_t hh);

/**
	Registers callback functions to be called by the event loop
        when it starts and stops, respectively. The callbacks are
        executed in the event loop's thread context, which might be a
        different one from the thread if the event loop is started
        asynchronously.
	
        @param hh the haggle handle
        @param start the startup callback function, or NULL if none.
        @param stop the stop callback function, or NULL if none.
        @param arg an argument to be passed with the callbacks.
	@returns an error code.
*/
HAGGLE_API int haggle_event_loop_register_callbacks(haggle_handle_t hh, haggle_event_loop_start_t start, haggle_event_loop_stop_t stop, void *arg);
#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif

#endif /* _LIBHAGGLE_IPC_H */
