package org.haggle;

import java.lang.System;
import java.lang.Thread;
import org.haggle.Handle;
import org.haggle.EventHandler;
import org.haggle.DataObject;
import org.haggle.LaunchCallback;

public class TestApp extends Thread implements EventHandler {	
        private Handle h = null;
        private String name;

	public synchronized void onNewDataObject(DataObject dObj) {
                System.out.println("Got new data object, filepath=" + dObj.getFilePath());
        }
	public synchronized void onNeighborUpdate(Node[] neighbors) {
                System.out.println("Got neighbor update event");

                for (int i = 0; i < neighbors.length; i++) {
                        System.out.println("Neighbor: " + neighbors[i].getName());
                }
        }
        public synchronized void onInterestListUpdate(Attribute[] interests) {
                System.out.println("Got interest list update event");

                for (int i = 0; i < interests.length; i++) {
                        System.out.println("Interest: " + interests[i].toString());
                }
        }
	public synchronized void onShutdown(int reason) {
                System.out.println("Got shutdown event, reason=" + reason);
        }
        public TestApp(String name)
        {
                super();
                this.name = name;
        }
        public void start()
        {
                Attribute attr = new Attribute("foo", "bar", 3);
                
                System.out.println("Attribute " + attr.toString());
                
                long pid = Handle.getDaemonPid();

                System.out.println("Pid is " + pid);

                if (pid == 0) {
                        // Haggle is not running
                        if (Handle.spawnDaemon(new LaunchCallback() { 
                                        public int callback(long milliseconds) {
                                                System.out.println("Launching haggle, milliseconds=" + milliseconds); 
                                                return 0;
                                        }
                                }) == false) {
                                // Could not spawn daemon
                                System.out.println("Could not spawn new Haggle daemon");
                                return;
                        }
                }
                try {
			final int num_dataobjects = 5;
			DataObject[] dobjs = new DataObject[num_dataobjects];
                        h = new Handle(name);

                        h.registerEventInterest(EVENT_NEIGHBOR_UPDATE, this);
                        h.registerEventInterest(EVENT_INTEREST_LIST_UPDATE, this);
                        h.registerEventInterest(EVENT_HAGGLE_SHUTDOWN, this);
                        
                        h.registerInterest(attr);
                
                        h.eventLoopRunAsync();
                        
                        h.getApplicationInterestsAsync();

                        Thread.sleep(3000);

			for (int i = 0; i < num_dataobjects; i++) {
				dobjs[i] = new DataObject();
				dobjs[i].addAttribute("num", "" + i);
				h.publishDataObject(dobjs[i]);
				Thread.sleep(1000);
			}
			for (int i = 0; i < num_dataobjects; i++) {
				h.deleteDataObject(dobjs[i]);
				dobjs[i].dispose();
				Thread.sleep(1000);
			}
                        h.shutdown();
                        
                        Thread.sleep(5000);

                        h.eventLoopStop();
                        
                        Thread.sleep(2000);

                } catch (Handle.RegistrationFailedException e) {
                        System.out.println("Could not get handle: " + e.getMessage());
                        return;
                } catch (Exception e) {
                        System.out.println("Got run loop exception\n");
                        return;
                }

                h.dispose();
        }
        public static void main(String args[])
        {
                TestApp app = new TestApp("JavaTestApp");

                app.start();

                System.out.println("Done...\n");
        }
}
