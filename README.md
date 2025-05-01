# camera-viewer
A program for reading out a GigE-vision/USB-vision industrial camera, displaying images and storing them if desired

Debendencies: see file "packages"

sudo apt-get install $(cat packages)


### Aravis (use a 0.8 release) ###

https://github.com/AravisProject/aravis

pushd aravis
meson build
pushd build
ninja
sudo ninja install
popd
sudo cp src/aravis.rules /etc/udev/rules.d
popd


### RunningStats ###

git clone https://github.com/abrock/RunningStats
mkdir build-runningstats
cd build-runningstats
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ../RunningStats
ninja
sudo ninja install

### ParallelTime ###

Basically the same procedure as for RunningStats
git clone https://github.com/abrock/ParallelTime
mkdir build-paralleltime
cd build-paralleltime
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ../ParallelTime
ninja
sudo ninja install

### OpenCV ###

You can use system libraries, but on Debian they are built without QT enabled, so for a better user interface you have to build it yourself:

git clone --recursive https://github.com/opencv/opencv
git clone --recursive https://github.com/opencv/opencv_contrib
mkdir -p build-opencv
pushd build-opencv
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DOPENCV_EXTRA_MODULES_PATH=../opencv_contrib/modules ../opencv -DWITH_QT=True
ninja
sudo ninja install
popd

### Building and Installing ###

git clone https://github.com/abrock/camera-viewer
mkdir build-camera-viewer
cd build-camera-viewer
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ../camera-viewer
ninja

### Updating ###

cd camera-viewer
git pull
cd ../build-camera-viewer
ninja

### Using camera-viewer ###

cd build-camera-viewer
./camera-viewer

A GUI should show up and show two windows.
One with controlls and one with the image.
Keyboard shortcuts for the image window (which is controlled by OpenCV):

q: quit
w: Increase exposure time
s: Decrease exposure time
e: Increase gain ("ISO" in many consumer cameras)
d: Decrease gain

### Using camera-viewer-dbus-sender ###

The program camera-position-dbus-sender sends a DBus signal to the (already running) camera-viewer and waits for a return signal.
This can be used e.g. in custom M100-M199 commands in LinuxCNC, see https://linuxcnc.org/docs/devel/html/gcode/m-code.html#mcode:m100-m199

Example for a custom M100 file:
```
#!/bin/bash
set -euxo pipefail
sleep 1 # This makes the program wait for 1 second before running the following line. Adjust as needed.
~/build-camera-viewer/camera-viewer-dbus-sender -p $1
```

Steps:
1. Put the M100 file (no filename extension) into your PROGRAM_PREFIX directory, typically ~/linuxcnc/nc_files
2. Make it executable (run "chmod 755 M100" in a terminal)
3. (Re-)Start LinuxCNC

Now you should be able to use "M100 P<some value>" in your CNC-programs if camera-viewer is running.

