This FAQ targets people that are interested in developing projects using the Haggle source code, either creating applications or hacking Haggle itself. Here you will find answers and motivations for many of the design choices made in Haggle, and other programming related issues.

**What code style do you use in Haggle?**

For C we prefer the Linux kernel style, which is basically the original Kernighan and Ritchie (K&R) with 8 char-indentations. The Linux kernel source code contains a [coding style document](http://www.kernel.org/doc/Documentation/CodingStyle) with good motivations for this choice, and is a recommended read for everyone. For C++ we use a similar style, based on [Google's coding style](http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml), but also with 8 char-indentations (instead of the way too small two char default). We highly recommend reading the Google code style documents.

**I see that you have implemented a string class and some container classes. Why not use C++ STL instead?**

We wanted Haggle to be portable to platforms that do not necessarily have STL libraries (for example Android). Further, a full STL implementation is quite heavy weight and contains a lot of classes that we do not really need. Therefore, it would be inefficient to install a full STL library on small devices that have limited storage and memory to begin with.

For those platforms that have STL libraries, it is possible to use STL classes instead of our own ones using a configure option.

**I see that you have a Reference class that you use for reference counting of some type of objects. Why not use a general reference counting scheme for all objects so that I do not have to care about memory management?**

Reference counting has a slight overhead, and a general reference counting scheme we thought was unnecessary for our purposes. One reason we chose C++ is for its efficiency, and too much reference counting might mitigate some of that. In general, we only need reference counting of data that is passed in public events (which can be processed by several managers or threads), where the one processing the event might want to hold on to the data for a while after the event handler has returned.

**When I check out haggle, there are so many files and folders all over the place. Could you help me figure out what they all are?**

Sure. Go [here](HaggleCodeGuide.md).

**What branches are there? Can I add a new branch to haggle? What's this "Vendetta" stuff in the haggle-demo branch?**

Good question. [Here](HaggleBranches.md), is a page where we try to explain all questions regarding the branches.