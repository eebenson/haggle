### What is Haggle (I am a user and do not understand all this technical jargon)? ###

Think of Haggle as a system that makes it easy to exchange files between mobile devices, such as mobile phones. On the Web, people can go to Google to find the information they want in the "online cloud" by typing in a few keywords. Haggle tries to do the same for the "offline cloud" of people in social communities. Imagine that you can type in a number of keywords into your mobile phone, and as you move about during the course of a day, you will receive content that match your keywords from the people you meet. The more precise the keywords are, the more accurate will also the content received be.

### What do you mean by opportunistic communication? ###

Nowadays, people have powerful communication devices (e.g., smartphones) with lots of content which they carry everywhere they go during the course of a normal day. While doing this, lots of communication opportunities occur that are not exploited, e.g., one walks past an open access point, or meets another mobile device holding content of interest, and so forth. Opportunistic communication is about devising mechanisms to exploit these opportunities in smart and efficient ways, by using any means possible.

### Why should I care about Haggle if I am a developer? ###

You should, if you have ever been frustrated by the following:

  * Mobile phones nowadays have a multitude of communication interfaces and content capabilities, but they suck at simple things such as exchanging data directly between themselves.
  * The "standard" synchronous communication APIs are overly complex and difficult to work with in a mobile environment where disconnections are common, and delays may be long.
  * To get some content you are interested in, you have to find a specific host to get it from rather than getting it automatically from the first person you meet and that carries it.

### Why the name Haggle? ###

Haggle is a play on the words Google and Ad-hoc. Haggle also means _barter_ or _negotiate_, which is exactly what mobile devices do with content when they meet and run Haggle.

### How does Haggle relate to Delay-Tolerant Networking (DTN)? ###

Lets avoid some confusion by defining what is really meant by DTN. Firstly, one can refer to DTNs when one talks about the broad collection of network types that generally have very long delays (due to slow data links and frequent disconnections), and use a postal-style store-carry-forward approach to data delivery. Haggle can be seen as an architecture that generally works well with such networks. Secondly, DTN can refer to the [DTN architecture](http://www.dtnrg.org/wiki/Code) -- an effort to standardize a set of protocols that works well with DTNs when the Internet protocols do not. Here we focus on the differences between Haggle and the DTN architecture.

Although both architectures target similar style networks, the approach taken and scenarios envisaged are quite different. In general, Haggle focuses more on applications that incorporate social aspects, have content-focus, and work on consumer devices such as mobile phones. DTN, on the other hand, focuses more on efficient communication in very challenged environments, such as in deep space and rural areas where no infrastructure is available. DTN sort of aims to create a _virtual link_ to move data between two endpoints by using mobile data mules, while Haggle aims to allow users to _disseminate_ content using their mobile devices even though there is no infrastructure (or it is too costly and slow).

On a technical level there are many differences between the DTN architecture and Haggle. For instance, while the DTN architecture has a traditional host-centric approach to addressing using End-point Identifiers (EIDs), Haggle has a data-centric addressing approach based on metadata (in the form of key-value pairs). Further, DTN encapsulates several application data items into _bundles_ for efficient delivery, while Haggle does _'application layer framing'_ with its own _data object_ format.

### Isn't Haggle really a [publish-subscribe](http://en.wikipedia.org/wiki/Publish/subscribe) system? ###

Well yes, in many ways it is a pub-sub system for DTNs. But it is quite different from pub-sub systems that are created for the Internet. Internet pub-sub systems typically use an overlay, such as a distributed hash table ([DHT](http://en.wikipedia.org/wiki/Distributed_hash_table)), to disseminate events about where content matching a user's subscription has been published. The subscriber can then choose to pull the content from one of the publishers. Obviously, this pull approach does not work in a disconnected DTN-style network. Haggle instead disseminates a user's profile (called the _node description_) in the network, and when the profile matches some data on another user's device, that device may try to push the matching data to the owner of the profile by disseminating to a subset of its neighbors. This push approach works better without end-to-end paths, which are not always available in the networks that Haggle targets.

A novelty of Haggle, compared to other pub-sub systems, is how published data is matched against user interests (the subscriptions). Pub-sub systems generally use topic channels or filters to match content. Both these approaches are boolean -- either a data object matches or it doesn't. Haggle instead uses a search approach to matching,  similarly to Google. Imagine going to Google and typing in a few keywords (the subscription). The result may be millions of hits, but usually only the top ten hits are of interest (which is also what is shown by default). By using ranking, Haggle does the exact same thing to match data against a user's profile, i.e., there may be many matches, but only the top _N_ ones are pushed to the owner of the profile.

### Do I understand it correctly that Haggle does epidemic dissemination of data? ###

Both yes and no. By default, Haggle only disseminates data epidemically among the groups of people that have an interest in the data (the _interest group_). This follows naturally from the interest-driven data exchange that happens when two devices come in contact. When these two later meet other devices in the same group, they will further spread the data and so forth. The users that have no interested in a data object will hence not receive it. However, Haggle also has a framework for supporting forwarding algorithms that can compute what we call _delegates_, which are people/devices that carry the data on behalf of interest groups they are not part of themselves. Thus, forwarding is useful to "connect" otherwise segmented interest groups, or to just reduce the delay it takes to disseminate to the entire group.

### How are disseminations triggered? ###

There are three ways disseminations are triggered: (1) by meeting a neighbor, (2) by generating new data, or (3) by periodic timeouts. Only (1) is enabled by default, however, while the other two mechanisms are optional. Although (2) might seem to be a good idea it goes against Haggle's design, which is to disseminate data according to rank when matched against a user's interest. When nodes encounter each other, only the top N (where N is typically 10) data objects are exchanged. If one of the nodes in this encounter generates a new data object while still in contact, the new object might not be among the next N ranked objects that should be disseminated. Therefore, mechanism (2) somewhat breaks the ranking and priority scheme that Haggle is built around. Mechanism (3) is a better option, as this is similar to (1) in that it can be seen as artificially triggering existing neighbors to be rediscovered at the cost of more power consumption.

### What about security? ###

This is the omnipresent research question that certainly has validity also in the context of Haggle. Security issues related to opportunistic networking is researched broadly within the Haggle EU project. The outcome of that research may or may not apply directly to the Haggle architecture and implementation, and, in case it does apply, it may be integrated in the code.

Haggle currently implements a basic security framework that allows data objects to be signed and verified. This is a proof of concept as we use our own self-signed root-certificate, but it illustrates the integration of security mechanisms in Haggle. The framework supports three security levels: LOW, MEDIUM, and HIGH. These currently correspond to "accept all data objects", "accept only signed node descriptions but all other types of unsigned objects", and "all data objects must be signed". We use the MEDIUM setting by default as that protects the "subscriptions" in the network, but allows nodes to accept data even if they do not have the certificate of the person/entity that generated it.

The security philosophy we adopt in Haggle is to be permissive about sharing and forwarding information, but to be restrictive about accepting information. The reason for this approach is that participating nodes rely on the services of each other for the communication to function. Hence nodes should be happy to give information to other nodes as that helps the dissemination of content in the network. If nodes require privacy, they should encrypt their data rather than being restrictive about disseminating it.

### There is a Haggle project on [SourceForge](http://sourceforge.net/projects/haggle/) as well. How does that project relate to this one? ###

Haggle is a EU funded research project, and the SourceForge page and this one are both part of that project. However, the code on the SourceForge page is an implementation of an earlier version of the Haggle architecture, which was developed by Intel Research Cambridge - a now defunct research lab that is no longer part of the project. That implementation was written in Java, and it is based on an architecture design that differs substantially from the later one found here, which has been developed by Uppsala University. To make a clean break, and to make clear the differences from the earlier implementation, we decided to create a new project page. Further, we also like the cleaner design of the Google code pages.

### Which platforms does Haggle run on? ###

Haggle runs on Linux, Mac OS X, Android, iPhone (limited functionality), Windows, Windows Mobile.

### Which programming language is Haggle written in? ###

Haggle itself is written mostly in C++, with some bits in C. In addition to Haggle itself, there is an application programming library called **libhaggle** which is written in C. Over libhaggle exists also language wrappers for C# and Java, which makes it possible to write applications in any of those languages.

### I am starting a project that uses Haggle, which platform would you recommend I work with? ###

This is a tough question that depends on the nature of your project. A lot of work was put into making Haggle work well across many platforms, so our general recommendation is to follow that example and ensure that your project runs on as many platforms as possible. However, if you are looking to do applications on mobile phones we would recommend Android, because it is very open and it is easy to write nice looking applications. People that are used to Windows may prefer Windows mobile, however, and Visual Studio is quite easy to work with.

### When I run your code, I experience crashes or weird behavior? ###

That is very unfortunate. Although we try to make releases as stable as possible, Haggle is still experimental software that is primarily created for research purposes. Therefore, we cannot maintain commercial quality code as we are continuously exploring new grounds. We encourage you to file a bug report under "Issues". Remember to describe your problem in detail, and preferably a way to reproduce it. We also gladly accept patches if you feel up to the task of fixing the problem yourself.