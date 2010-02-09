package org.haggle;
import org.haggle.LaunchCallback;

public class Handle {
        private long nativeHandle = 0; // pointer to Haggle handle C struct
        private boolean disposed = false;
        private String name;
        	
        private native int getHandle(String name);
        private native void nativeFree(); // Must be called when handle is not used any more
	
        public static native void unregister(String name);

        public native int getSessionId();
        public native int shutdown();
	public native int registerEventInterest(int type, EventHandler handler);
	public native int publishDataObject(DataObject dObj);
        public native int registerInterest(Attribute attr);
        public native int registerInterests(Attribute[] attrs);
        public native int unregisterInterest(Attribute attr);
        public native int unregisterInterests(Attribute[] attrs);
        public native int getApplicationInterestsAsync();
        public native int getDataObjectsAsync();
        public native int deleteDataObjectById(char[] id);
        public native int deleteDataObject(DataObject dObj);

        // Should probably make the eventLoop functions throw some exceptions
	public native boolean eventLoopRunAsync(); 
	public native boolean eventLoopRun();
	public native boolean eventLoopStop();
        public native boolean eventLoopIsRunning();

        public static int HAGGLE_ERROR = -100;
        public static int HAGGLE_NO_ERROR = 0;

        // Useful for launcing Haggle from an application
        public static int HAGGLE_DAEMON_ERROR = HAGGLE_ERROR;
        public static int HAGGLE_DAEMON_NOT_RUNNING = HAGGLE_NO_ERROR;
        public static int HAGGLE_DAEMON_RUNNING = 1;
        public static int HAGGLE_DAEMON_CRASHED = 2;
        
        public static native long getDaemonPid();
        public static native int getDaemonStatus();
        public static native boolean spawnDaemon();
        public static native boolean spawnDaemon(String path);
        public static native boolean spawnDaemon(LaunchCallback c);
        public static native boolean spawnDaemon(String path, LaunchCallback c);
        
	// Non-native methods follow here
	public Handle(String name) throws RegistrationFailedException
        {
                int ret = getHandle(name);

                if (ret != 0)
                        throw new RegistrationFailedException("Registration failed with value " + ret);

                this.name = name;
        }
        public class RegistrationFailedException extends Exception {
                RegistrationFailedException(String msg)
                {
                        super(msg);
                }
        }
        public synchronized void dispose()
        {
                if (!disposed) {
                        disposed = true;
                        nativeFree();
                }
        }
        protected void finalize() throws Throwable
        {
                dispose();
                super.finalize();
        }
        static {
                System.loadLibrary("haggle_jni");
        }
}
