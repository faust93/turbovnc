# Experimental H264 encoder support for TurboVNC Xvnc


## Details

H264 encoder implementation for TurboVNC server part aimed to work with the H264 decoding protocol implemented in TigerVNC viewer in the following commit:

https://github.com/TigerVNC/tigervnc/pull/1194/commits/03df44849617122fba9b521006ae147e4520bf73

Two H264 encoder implementations are available:

- Cisco OpenH264, software only encoding
- FFMpeg libvacodec, software (x264) and hardware encoding: h264 VAAPI (GPU h264 encoding profile support required), h264_rkmpp (Rockchip MPP)

**Attention**

> For h264_rkmpp to work a 3rd party FFMpeg compiled with Rockchip MPP support required - https://github.com/nyanmisaka/ffmpeg-rockchip

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
$ cp ../unix/Xvnc/programs/Xserver/hw/vnc/h264/ffh264.cfg /tmp
$ ./bin/vncpasswd .vncpass
$ ./bin/Xvnc :1 -depth 32 -geometry 1680x1024 -rfbport 5901 +iglx -rfbauth .vncpass -deferupdate 1 -h264conf /tmp/ffh264.cfg &
$ DISPLAY=:1 startxfce4
```

Then connect to the server using TigerVNC viewer

**You have to build TigerVNC viewer manually** since H264 decoding is not included in the binary distros

H264 parameters can be tuned according to your needs/preferences, see `h264.cfg` for OpenH264 and `ffh264.cfg` for FFMpeg encoders

## Audio support

Proof of concept QEMU audio server side protocol implementation

### How to use

#### Server side

**Requirements:**

1. PulseAudio
2. pulseaudio-module-xrdp https://github.com/neutrinolabs/pulseaudio-module-xrdp

I decided to re-use existing Pulseaudio sink from the XRDP project

You have to build it yourself or install prebuilt package if your distro has one

1. Load XRDP sink inside already running Xvnc session:

`$ pacmd load-module module-xrdp-sink sink_name=XVNC rate=22050 format=s16le channels=2 xrdp_socket_path=/tmp/.xvnc`

2. Set `xrdp sink` as a default **Output device** using `pavucontrol` or some another tool used to control PulseAudio

#### Client side

UPD: It's recommended to use bwVNC client for better results and compatibility:

https://github.com/faust93/bwVNC

In theory any VNC client with QEMU audio support should work but there are not many of them around so I personally use TigerVNC client patched for QEMU Audio support:

https://github.com/faust93/tigervnc

**You have to build it from sources yourself**. Nothing more has to be done on the client side, just connect to the server and try to produce some sound

Default audio settings are set to 22050 16 bit 2 channels

[![Demo video](https://img.youtube.com/vi/4m_iZnIf9EE/default.jpg)](https://youtu.be/4m_iZnIf9EE)

## Cudos

@martin19 for his work on libvncserver
https://github.com/martin19/libvncserver
