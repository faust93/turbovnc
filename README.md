# Experimental H264 codec support for TurboVNC Xvnc

Inspired by martin19's work on libvncserver
https://github.com/martin19/libvncserver

## Details

H264 codec implementation for TurboVNC server part aimed to work with the protocol implemented in TigerVNC viewer in the following commit:

https://github.com/TigerVNC/tigervnc/pull/1194/commits/03df44849617122fba9b521006ae147e4520bf73

Cisco OpenH264 is curently used for encoding but perhaps in the future it will be replaced with FFmpeg's libvacodec


## Building

Install all the dependencies (openh264 package etc.. see your distro repos) then:

```sh
$ git clone https://github.com/faust93/turbovnc.git
$ cd turbovnc
$ mkdir build && cd build
$ cmake ..
$ cmake --build .

```

To run:

```sh
$ cp ../unix/Xvnc/programs/Xserver/hw/vnc/h264/h264.cfg /tmp
$ ./bin/vncpasswd .vncpass
$ ./bin/Xvnc :1 -depth 32 -geometry 1680x1024 -rfbport 5901 +iglx -rfbauth .vncpass -deferupdate 20 -h264conf /tmp/h264.cfg &
$ DISPLAY=:1 startxfce4
```

Then connect to the server using TigerVNC viewer

**You have to build TigerVNC viewer manually** since H264 decoder is not included in the binary distros

H264 parameters can be tuned according to your needs/preferences, see /tmp/h264.cfg file

