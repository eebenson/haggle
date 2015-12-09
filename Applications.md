# Introduction #

This page describes some of the Haggle applications that have been implemented running on top of Haggle. They may provide a good starting point for people interested in writing applications for Haggle, both for finding inspiration and for reviewing code examples.

# PhotoShare #

PhotoShare is probably the most complete application that we have currently implemented for Haggle and it is described in its [separate page](PhotoShare.md). PhotoShare exists for Android and Windows mobile.

# Webserver #

Not the most describing name perhaps, but this is a simple application that serves a web page to browsers. By connecting with your browser to http://localhost:8081 you get a very simple search page as pictured below.

|![http://wiki.haggle.googlecode.com/hg/images/webserver-screenshots/webserver-search-window-coke-small.png](http://wiki.haggle.googlecode.com/hg/images/webserver-screenshots/webserver-search-window-coke-small.png)|
|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|

In the search field you may enter keywords, and if there are nearby devices with matching content (in this case pictures) you will received them and they will be displayed in the window.

# Filedrop #

Filedrop is a simple file monitor application that allows you to publish files into the Haggle network by simply dragging and dropping files into a folder on your desktop computer. Metadata can be automatically extracted from the files and then added as Haggle attributes that can be searched and matched in Haggle.

# Mailproxy #

Mailproxy is actually a set of daemons that make it possible to use a regular email client to send email over Haggle. Your email address and other metadata are extracted from mails and used as metadata that forwards the mail in the Haggle network to those persons interested in receiving it. This may sound scary, but we are working on security solutions that may make this functionality more attractive for the general public. A simple fix that significantly improves the security aspect is simply to encrypt the emails. However, that we believe is not the job of Haggle, but rather the email client itself. Remember that emails go in plain text also over the Internet.

# BBS #

BBS is a Bulletin Board System over Haggle. Similarly to the Webserver application, a simple BBS interface is presented in a web browser if BBS is running and the browser is pointed to http://localhost:8082. Below is a screenshot that shows BBS running in a browser. Users can create topics and other users can follow them by subscribing to the topic.

|![http://wiki.haggle.googlecode.com/hg/images/bbs-screenshots/haggle-bbs-small.png](http://wiki.haggle.googlecode.com/hg/images/bbs-screenshots/haggle-bbs-small.png)|
|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------|

# MobiClique #

MobiClique is a social networking application for Haggle. It is being developed by [Thomson Research](http://www.thlab.net) and more information can be found on their [MobiClique website](http://www.thlab.net/~apietila/mobiclique/).