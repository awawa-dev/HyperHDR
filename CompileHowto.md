# Preparing build environment

## Debian/Ubuntu

```
sudo apt-get update
sudo apt-get install build-essential cmake git libasound2-dev libavahi-core-dev libavahi-compat-libdnssd-dev libglvnd-dev libqt5serialport5-dev libqt5sql5-sqlite libqt5svg5-dev libqt5x11extras5-dev libturbojpeg0-dev libusb-1.0-0-dev python3-minimal rpm qtbase5-dev
```

sometimes on RPI you may need the videocore IV headers

```
sudo apt-get install libraspberrypi-dev
```

## Windows
We assume a 64bit Windows 10. Install the following;
- [Git](https://git-scm.com/downloads) (Check: Add to PATH)
- [CMake (Windows win64-x64 Installer)](https://cmake.org/download/) (Check: Add to PATH)
- [Visual Studio 2019 Build Tools](https://go.microsoft.com/fwlink/?linkid=840931) ([direct link](https://aka.ms/vs/16/release/vs_buildtools.exe))
  - Select C++ Buildtools
  - On the right, just select `MSVC v142 VS 2019 C++ x64/x86-Buildtools` and latest `Windows 10 SDK`. Everything else is not needed.
- [Python 3 (Windows x86-64 executable installer)](https://www.python.org/downloads/windows/) (Check: Add to PATH and Debug Symbols)
  - Open a console window and execute `pip install aqtinstall`.
  - Now we can download Qt to _C:\Qt_ `mkdir c:\Qt && aqt install -O c:\Qt 5.15.0 windows desktop win64_msvc2019_64`
- Optional for package creation: [NSIS 3.x](https://sourceforge.net/projects/nsis/files/NSIS%203/) ([direct link](https://sourceforge.net/projects/nsis/files/latest/download))

## MacOs
First install brew manager.
Next:
- brew install qt@5
- brew install cmake
- brew install jpeg-turbo

# Compiling and installing HyperHDR

### The general quick way (without big comments)

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
# webui is located on http://localhost:8090 or 8091 and https on 8092

# optional: install into your system
sudo make install/strip
# optional: to uninstall (not very well tested, please keep that in mind)
sudo make uninstall
```


### Download
 Creates hyperhdr directory and checkout the code from github

```
export HYPERHDR_DIR="hyperhdr"
git clone --recursive --depth 1 https://github.com/awawa-dev/HyperHDR.git "$HYPERHDR_DIR"
```

### Preparations
Change into hyperhdr folder and create a build folder
```
cd "$HYPERHDR_DIR"
mkdir build
cd build
```

### Generate the make files:

To generate make files with automatic platform detection and default settings:

This should fit to *RPI, x86
```
cmake -DCMAKE_BUILD_TYPE=Release ..
```

To generate files on Windows (Release+Debug capable):

Platform should be auto detected and refer to windows, you can also force windows:

```sh
# You might need to setup MSVC env first
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cmake -DPLATFORM=windows -G "Visual Studio 16 2019" ..
```

### Run make to build HyperHDR

The `-j $(nproc)` specifies the amount of CPU cores to use.
```bash
make -j $(nproc)
```

On Windows run
```bash
cmake --build . --config Release -- -maxcpucount
```
Maintainer: To build installer, install [NSIS](https://nsis.sourceforge.io/Main_Page) and set env `VCINSTALLDIR="C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC"`

### Install HyperHDR into your system

Copy all necessary files to ``/usr/local/share/hyperhdr``
```bash
sudo make install/strip
```

If you want to install into another location call this before installing

```bash
cmake -DCMAKE_INSTALL_PREFIX=/home/pi/apps ..
```
This will install to ``/home/pi/apps/share/hyperhdr``
