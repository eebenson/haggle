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
#ifndef _RESOURCEMONITORLINUX_H
#define _RESOURCEMONITORLINUX_H

#ifndef  _IN_RESOURCEMONITOR_H
#error "Do not include this file directly, include ResourceMonitor.h"
#endif

#include <libcpphaggle/Platform.h>
#include "ManagerModule.h"

/** */
class ResourceMonitor : public ManagerModule<ResourceManager>
{
        int uevent_fd;
        int uevent_init();
        void uevent_close();
        void uevent_read();
	// Thread entry and exit
	void cleanup();
	bool run();

        unsigned int batteryLife;
public:
	ResourceMonitor(ResourceManager *resMan);
	~ResourceMonitor();
	
	/**
	   Returns: battery charge left in percent.
	*/
	unsigned char getBatteryLifePercent() const;
	/**
	   Returns: battery time left in seconds.
	*/
	unsigned int getBatteryLifeTime() const;
	
	/**
	   Returns: number of bytes of physical memory left
	*/
	unsigned long getAvaliablePhysicalMemory() const;
	/**
	   Returns: number of bytes of virtual memory left
	*/
	unsigned long getAvaliableVirtualMemory() const;
};


#endif /* _RESOURCEMONITORLINUX_H */
