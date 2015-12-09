# Prerequisites #

If you want to compile Haggle for Windows mobile, you need to have a copy of Visual Studio 2008. Earlier versions probably work, but may require converting the project files somehow. You also need to install the **Windows mobile 6 professional SDK** and/or **Windows mobile 6 standard SDK**. The main difference between these two is that "professional" is for touch screen devices and "standard" is for non-touch screen ones. We recommend going for "professional" as some Haggle applications (e.g., PhotoShare) are optimized for touch screen phones.

In order to access the source code from the repository you also need [Mercurial](http://mercurial.selenic.com/wiki/Mercurial). We recommend using [TortoiseHg](http://bitbucket.org/tortoisehg/stable/wiki/Home) for good integration with the Windows file manager.

If you download a Haggle release package (i.e., a zip-file from the 'Downloads' page), you need the `haggle-<version>-full.zip` package as this includes the full source of the Libxml2, OpenSSL, and Sqlite, which are needed when compiling under Windows.

# Installing a Pre-compiled Package #

We try to provide pre-compiled binaries of the releases we make in order to make it easy for people to try Haggle. These packages can be accessed from the [Downloads](http://code.google.com/p/haggle/downloads/list) page.

A package installs Haggle and the PhotoShare application. Note that you may have to disable the security on the device in order for Haggle to launch. You also need to install .NET compact framework 3.5 in order to run the PhotoShare application

To install, download the package that suits your device (e.g., HaggleInstaller-WM6.cab) and transfer it to the device. Run the cab file to install Haggle.


# Compiling the Source Code #

Simply navigate into the **winmobile** directory of the source code tree. There you will find a number of directories with VS project files for building Haggle and some applications. Open, e.g., the **solution file** for Haggle (`Haggle.sln`) and press F7 to compile.

# Troubleshooting #

If you have compilation problems, make sure that you select the correct target before building (e.g., Windows mobile professional), and using the "Debug" configuration. These settings are the most likely to work as they are being used by developers regularly. You may need to check the 'Configuration Manager' that all projects in the solution are building for the correct target. Before deploying on your device, make sure that all projects are set to deploy on your target device (and not to, e.g., the emulator, unless that is your choice).