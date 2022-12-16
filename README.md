# Experimental H264 encoder support for TurboVNC Xvnc


## Details

H264 encoder implementation for TurboVNC server part aimed to work with the H264 decoding protocol implemented in TigerVNC viewer in the following commit:

https://github.com/TigerVNC/tigervnc/pull/1194/commits/03df44849617122fba9b521006ae147e4520bf73

Two H264 encoder implementations are available:

- Cisco OpenH264, software only encoding
- FFMpeg libvacodec, software (x264) and hardware encoding (GPU h264 encoding profile support required)


## Building

Install all the dependencies (openh264, ffmpeg package etc.. see your distro repos) then to build Xvnc with FFMpeg H264 encoder implementation:

```sh
$ git clone https://github.com/faust93/turbovnc.git
$ cd turbovnc
$ mkdir build && cd build
$ cmake .. -DTVNC_H264=OFF -DTVNC_FFH264=ON
$ cmake --build .

```

To run:

```sh
$ cp ../unix/Xvnc/programs/Xserver/hw/vnc/h264/h264.cfg /tmp
$ ./bin/vncpasswd .vncpass
$ ./bin/Xvnc :1 -depth 32 -geometry 1680x1024 -rfbport 5901 +iglx -rfbauth .vncpass -deferupdate 1 -h264conf /tmp/h264.cfg &
$ DISPLAY=:1 startxfce4
```

Then connect to the server using TigerVNC viewer

**You have to build TigerVNC viewer manually** since H264 decoding is not included in the binary distros

H264 parameters can be tuned according to your needs/preferences, see `h264.cfg` file


## Cudos

@martin19 for his work on libvncserver
https://github.com/martin19/libvncserver
