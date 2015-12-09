# Introduction #

Haggle provides a Java wrapper for libhaggle for those that want to write applications in Java, or run on the Android platform. Below follows a short introduction to the Java API that only covers a subset of the API.

The [TestApp](http://code.google.com/p/haggle/source/browse/src/libhaggle/jni/java/org/haggle/TestApp.java) application in the source tree is a good start for those that want an example of how to use the Java API. The Java calls that we describe here follows that application (more or less). For an Android example, and more elaborate use of the API, look at the [PhotoShare](http://code.google.com/p/haggle/source/browse/android/PhotoShare/src/org/haggle/PhotoShare/PhotoShare.java) source code.

## Note on Memory Management in Java ##

The Haggle Java API wraps native C functions in libhaggle and therefore many Haggle Java objects, such as data objects, interfaces, nodes, and attributes allocate native memory. This memory won't be released until the Java VM garbage collects the corresponding Java objects. As Java does not track the native memory usage of these objects, it can take some time before objects are garbage collected. This means that a Java application can have a lot of native memory allocated. By generating many data objects it is possible to allocate a large amount of native memory without freeing it.

In order to free native memory, it is possible to call the dispose() method on Haggle Java objects. This, of course, assumes that these objects are no longer used in your code. Another option would be to force the VM to run the garbage collector when memory should be freed.

# Prerequisites #

You need the `haggle.jar` Java library in your classpath to run a Haggle application written in Java. This library ends up in `lib/` or `src/libhaggle/jni` after running `make` in your haggle directory. You also need the native libraries (`libhaggle`, `libhaggle_jni`) installed and in a library path that your system recognizes (the simplest approach is to run `make install`). In [src/libhaggle/jni/README](http://code.google.com/p/haggle/source/browse/src/libhaggle/jni/README) you will find more information on how to run the Java TestApp.

# Writing the Application #

You need first to include the haggle packages:

```
import org.haggle.*
```

Then define a class for your application that implements the EventHandler interface and overrides the event-handler methods of interest. This allows your application to handle Haggle events. In the example below, the application can handle the new data object event:
```
public class MyHaggleApplication implements EventHandler {
        public synchronized void onNewDataObject(DataObject dObj) {
                System.out.println("Got new data object, filepath=" + dObj.getFilePath());
        }
}
```

In your application's start function (called, e.g., from main(), or the applications main-loop), get a handle to a running Haggle instance (i.e., Haggle must be running first):

```
try {
        Handle h = new Handle("MyHaggleApplication");
} catch (Handle.RegistrationFailedException e) {
        System.out.println("Could not get handle: " + e.getMessage());
}
```

Register for the events your application needs, e.g.:

```
 h.registerEventInterest(EVENT_NEIGHBOR_UPDATE, this);
 h.registerEventInterest(EVENT_INTEREST_LIST_UPDATE, this);
 h.registerEventInterest(EVENT_HAGGLE_SHUTDOWN, this);
```

In order for your application to be able to handle these events it must implement the EventHandler interface and override any of the handler functions that you register for. Note that handler functions are usually called in a different thread context than the application's main loop (if any), or GUI thread. Synchronization must be handled accordingly for shared data structures.

Register interests for the application, i.e., which data objects to receive:

```
Attribute attr = new Attribute("foo", "bar", 3);
h.registerInterest(attr);
```

Once the application is ready to run the event-loop, call:

```
 h.eventLoopRunAsync();
```

As this is an asynchronous call, the event-loop will run in a different thread and call the event handlers asynchronously. An alternative is to call the blocking equivalent `h.eventLoopRun()`. This makes sense for applications that do not have their own main-loop. To later stop the event-loop call `h.eventLoopStop()`, or `h.eventLoopIsRunning()` to check whether the event loop is currently running.

To get the current application interests that the application may have previously registered:

```
h.getApplicationInterestsAsync();
```

Note, however, that this requires that the application has also registered for the EVENT\_INTEREST\_LIST\_UPDATE event and have implemented the onInterestListUpdate() function of the EventHandler interface. The application interests will not be received until the event-loop has started.

To generate and publish a data object:
```
DataObject dObj = new DataObject("/path/to/my/file");
dObj.addAttribute("MyObjectName", "MyValue");
h.publishDataObject(dObj);
dObj.dispose(); // Call dispose() to free native memory before GC
```

To delete a data object in Haggle:
```
h.deleteDataObject(dObj);
```

This call deletes the data object from Haggle's permanent storage. It does not delete the Java object.

Shutdown Haggle:
```
h.shutdown();
```

This will generate a shutdown event that your application will receive only if it has registered for that event and implemented the `onShutdown()` function of the EventHandler interface.

When the application wants to close and exit, it should call the Haggle handle's `dispose()` method to deregister from Haggle. There is also a static `unregister()` function which can be used to unregister a previous handle that was never released due to an application crash.
```
h.dispose();
```