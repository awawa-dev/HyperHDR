## Available methods to build HyperHDR:
### 1. [Native build](https://github.com/awawa-dev/HyperHDR/wiki/Compiling-HyperHDR#native-build)
### 2. [Build a HyperHDR installer for any supported Linux system on any system using Docker](https://github.com/awawa-dev/HyperHDR/wiki/Compiling-HyperHDR#build-a-hyperhdr-installer-for-any-supported-linux-system-on-any-system-using-docker)
### 3. [Github Action (online - easiest)](https://github.com/awawa-dev/HyperHDR/wiki/Compiling-HyperHDR#github-action-online---easiest)

## Available CMake HyperHDR configuration options:
Use -D prefix when configuring the build.

* LED DEVICES  
  * ENABLE_SPIDEV = ON | OFF, enables SPI LED devices on supported systems
  * ENABLE_WS281XPWM = ON | OFF, enables WS281x LED library on supported systems
  
* SOFTWARE GRABBERS
  * ENABLE_DX = ON | OFF, enables the DirectX11 software grabber (Windows)
  * ENABLE_FRAMEBUFFER = ON | OFF, enables the Framebuffer software grabber (Linux)
  * ENABLE_MAC_SYSTEM = ON | OFF, enables the macOS software grabber (macOS)
  * ENABLE_PIPEWIRE = ON | OFF, enables the Pipewire software grabber (Linux)
  * ENABLE_PIPEWIRE_EGL = ON | OFF, enables EGL for the Pipewire grabber (Linux)
  * ENABLE_X11 = ON | OFF, enables the X11 software grabber (Linux)

* HARDWARE GRABBERS
  * ENABLE_AVF = ON | OFF, enables the AVF USB grabber support (macOS)
  * ENABLE_MF = ON | OFF, enables the MediaFoundation USB grabber support (Windows)
  * ENABLE_V4L2 = ON | OFF, enables the V4L2 USB grabber support (Linux)

* SOUND CAPTURING
  * ENABLE_SOUNDCAPLINUX = ON | OFF, enables the ALSA sound grabber (Linux)
  * ENABLE_SOUNDCAPMACOS = ON | OFF, enables the sound grabber (macOS)
  * ENABLE_SOUNDCAPWINDOWS = ON | OFF, enables the sound grabber (Windows)

* SERVICE SUPPORT
  * ENABLE_BONJOUR = ON | OFF, enables mDNS (do not disable unless required)
  * ENABLE_CEC = ON | OFF, enables the HDMI-CEC support (Linux)
  * ENABLE_MQTT = ON | OFF, enables the MQTT support
  * ENABLE_POWER_MANAGEMENT = ON | OFF, enables sleep/wake up OS events support
  * ENABLE_PROTOBUF = ON | OFF, enables Proto-Buffer server
  * ENABLE_SYSTRAY = ON | OFF, enables the systray-widget
  * ENABLE_XZ = ON | OFF, enables XZ support for LUT downloading

* BUILD FEATURES
  * USE_SHARED_LIBS = ON | OFF, if disabled, build the application as monolithic
  * USE_EMBEDDED_WEB_RESOURCES = ON | OFF, if enable, embed web content into the application
  * USE_PRECOMPILED_HEADERS = ON | OFF, use pre-compiled headers when building
  * USE_CCACHE_CACHING = ON | OFF, enable CCache support if available
  * USE_SYSTEM_MQTT_LIBS = ON | OFF, prefer system qMQTT libs
  * USE_SYSTEM_FLATBUFFERS_LIBS = ON | OFF, prefer system Flatbuffers libs
  * USE_STATIC_QT_PLUGINS = ON | OFF, embed static QT-plugins into the application
  * USE_STANDARD_INSTALLER_NAME = ON | OFF, use standard Linux package naming

----

# Native build

## Preparing build environment

### Debian/Ubuntu

```console
sudo apt-get update

sudo apt-get install build-essential cmake git libayatana-appindicator3-dev libasound2-dev wget unzip pkg-config
libegl-dev libflatbuffers-dev flatbuffers-compiler libftdi1-dev libgl-dev libglvnd-dev liblzma-dev libgtk-3-dev
libpipewire-0.3-dev libqt5serialport5-dev libssl-dev libx11-dev libsystemd-dev libturbojpeg0-dev libusb-1.0-0-dev 
libzstd-dev python3 pkg-config qtbase5-dev
```

For Raspberry Pi CEC support (optional)
```console
sudo apt-get install libcec-dev libp8-platform-dev libudev-dev
```

### Fedora

```console
sudo dnf -y group install "C Development Tools and Libraries" "Development Tools"

sudo dnf -y install cmake chrpath git alsa-lib-devel flatbuffers-devel flatbuffers-compiler fedora-packager
mesa-libEGL-devel libftdi-c++-devel mesa-libGL-devel gtk3-devel libglvnd-devel libayatana-appindicator-gtk3-devel 
pipewire-devel qt5-qtserialport-devel qt5-qtbase-devel openssl-devel turbojpeg-devel libusb1-devel libX11-devel
libzstd-devel pkg-config wget xz-devel systemd-devel unzip
```

### Arch Linux

```console
sudo pacman -Syy

sudo pacman -S base-devel qt5-base openssl chrpath cmake flatbuffers git alsa-lib gtk3 libayatana-appindicator
libftdi libglvnd libjpeg-turbo qt5-serialport wayland libx11 freetds libfbclient mariadb-libs postgresql-libs
pipewire python mesa dpkg xz fakeroot binutils pkgfile bash systemd-libs wget unzip
```

### Windows

We assume a 64bit Windows 10. Install the following;
- [Git](https://git-scm.com/downloads) (Check during installation: Add to PATH)
- [CMake (Windows win64-x64 Installer)](https://cmake.org/download/) (Check during installation: Add to PATH)
- [Visual Studio 2022 Community Edition](https://visualstudio.microsoft.com/downloads/)
  - Select 'Desktop development with C++'
  - On the right, just select `MSVC v143 VS 2022 C++ x64/x86-Buildtools` and latest `Windows 10 SDK`. Everything else is not needed, but you can stay with default selection.
- [OpenSSL](https://slproweb.com/products/Win32OpenSSL.html) (for QT5.15-6.2: OpenSSL v1.1.1, for QT6: OpenSSL 3)
- [libjpeg-turbo](https://github.com/libjpeg-turbo/libjpeg-turbo/releases)
- [Python 3 (Windows x86-64 executable installer)](https://www.python.org/downloads/windows/) (Check during installation: Add to PATH and Debug Symbols)
  - Open a console window and execute `pip install aqtinstall`.
  - Now we can download Qt to _C:\Qt_ `mkdir c:\Qt && aqt install -O c:\Qt 6.2.2 windows desktop win64_msvc2019_64 -m qtserialport`
  - May need to add Qt path before compiling, for example: `set CMAKE_PREFIX_PATH=C:\Qt\6.2.2\msvc2019_64\lib\cmake\` or for older `set Qt5_Dir=C:\Qt\5.15.2\msvc2019_64\lib\cmake\Qt5\`
- Optional for package creation: [NSIS 3.x](https://sourceforge.net/projects/nsis/files/NSIS%203/) ([direct link](https://sourceforge.net/projects/nsis/files/latest/download))

Hint: after you execute the configuration command in the build folder (for example ```cmake -DCMAKE_BUILD_TYPE=Release ..```) you should receive *.sln solution project file that can be opened in Visual Studio. Select `hyperhdr` project as default for the solution to run it after compilation.

### macOS
First install [brew](https://brew.sh/) manager.  
Next: `brew install qt@6 cmake xz pkg-config`

## Compiling and installing HyperHDR

### Linux/macOS: the general quick & recommended way

```bash
git clone --recursive https://github.com/awawa-dev/HyperHDR.git hyperhdr
cd hyperhdr
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j $(nproc)

# if this get stucked and dmesg says out of memory try:
make -j 2

# Run it from the build directory
bin/hyperhdr -d

# BUILD INSTALLERS (recommended method to install HyperHDR, doesnt work for ArchLinux: use build.sh script )
cpack
# or compile and build the package using one command
cmake --build . --target package --config Release
```

### Windows: the general quick & recommended way

```bash
git clone --recursive https://github.com/awawa-dev/HyperHDR.git hyperhdr
cd hyperhdr
mkdir build
cd build
# You might need to setup MSVC env first
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
# Maintainers: to build the HyperHDR installer, install NSIS and define environment variable:  
# set VCINSTALLDIR="C:\Program Files\Microsoft Visual Studio\2022\Community\VC"
cmake -DPLATFORM=windows -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release -- -maxcpucount

# Run it from the build directory
bin/Release/hyperhdr -d
```

### LibreELEC 

You can find the add-on sources here on branches of my LibreELEC fork: https://github.com/awawa-dev/LibreELEC.tv/ For example `libreelec-11.0-hyperhdr` branch. Adjust HyperHDR package properties in `packages/addons/service/hyperhdr/package.mk` Follow LibreELEC's manual on how to build the image. For example: 

LibreELEC 11/RPi:
```
PROJECT=RPi ARCH=arm DEVICE=RPi4 make image
PROJECT=RPi DEVICE=RPi4 ARCH=arm ./scripts/create_addon hyperhdr
```

LibreELEC 12/RPi:
```
PROJECT=RPi ARCH=aarch64 DEVICE=RPi4 make image
PROJECT=RPi DEVICE=RPi4 ARCH=aarch64 ./scripts/create_addon hyperhdr
```

PC(x86_64):
```
PROJECT=Generic ARCH=x86_64 DEVICE=Generic make image
PROJECT=Generic DEVICE=Generic ARCH=x86_64 ./scripts/create_addon hyperhdr
```

----

# Build a HyperHDR installer for any supported Linux system on any system using Docker

All you need is Docker and bash, which is available on every supported system, even on Windows where you only need to enable "Windows Subsystem for Linux". You don't need to install any packages to build HyperHDR because the script uses Docker images provided by https://github.com/awawa-dev/HyperHDR.dev.docker which contain everything you need to build the installer. Thanks to this, you can compile eg. the aarch64 HyperHDR installer for Raspberry Pi even on a PC. Run the `build.sh` script in the main directory.

```console
pi@ubuntu:~/hyperhdr$ ./build.sh 

Required environmental options:
PLATFORM - one of the supported targets: osx|windows|linux|rpi
DOCKER_TAG | DOCKER_IMAGE - both are required only for linux|rpi platforms:
   Debian => bullseye | x86_64
   Debian => bullseye | arm-32bit-armv6l
   Debian => bullseye | arm-64bit-aarch64
   Debian => bookworm | x86_64
   Debian => bookworm | arm-32bit-armv6l
   Debian => bookworm | arm-64bit-aarch64
   Ubuntu => jammy | x86_64
   Ubuntu => noble | x86_64
   Fedora => Fedora_40 | x86_64
   ArchLinux => ArchLinux | x86_64

Optional environmental options:
BUILD_TYPE - Release|Debug, default is Release version
BUILD_ARCHIVES - false|true, cpack will build ZIP package
USE_STANDARD_INSTALLER_NAME - false|true, use standard Linux package naming
USE_CCACHE - false|true, use ccache if available
RESET_CACHE - false|true, reset ccache storage

Example of usage:
PLATFORM=rpi DOCKER_TAG=bullseye DOCKER_IMAGE=arm-64bit-aarch64 ./build.sh
Installers from Docker builds will be ready in the deploy folder
```

The `build.sh` script can also be used to natively build macOS/Windows installers as an alternative to the method described in the previous point. Of course, then you must have all the necessary packages installed.

----

# Github Action (online - easiest)

Fork HyperHDR project. Now you must enable project's `Settings → Actions → Actions permissions → Allow all actions and reusable workflows` and save it.
Once you've done this, any change, even using the Github online editor, will immediately trigger the build in the Actions tab. If this did not happen, you probably did not enable the option described or did it later after making the changes.
