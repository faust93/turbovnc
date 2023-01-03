set(DEFAULT_TVNC_H264 OFF)
option(TVNC_H264 "Include OpenH264 encoding support in the TurboVNC Server"
	${DEFAULT_TVNC_H264})

set(DEFAULT_TVNC_FFH264 ON)
option(TVNC_FFH264 "Include FFmpeg H264 encoding support in the TurboVNC Server"
	${DEFAULT_TVNC_FFH264})

set(DEFAULT_VNC_AUDIO ON)
option(TVNC_AUDIO "Include audio support in the TurboVNC Server"
	${DEFAULT_VNC_AUDIO})

if(TVNC_AUDIO)
  set(VNC_HAVE_AUDIO 1)
  add_definitions(-DVNC_HAVE_AUDIO)
  set(VNC_AUDIO_C
	audio.c
    )
endif()

if(TVNC_H264 AND TVNC_FFH264)
  message(FATAL_ERROR "Can't use both H264 codecs at the same time, select the only one please")
endif()

if(TVNC_H264)
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/h264/")
  find_package(OPENH264)
endif()

if(OPENH264_FOUND)
  set(LIBVNCSERVER_HAVE_LIBOPENH264 1)
  add_definitions(-DLIBVNCSERVER_HAVE_LIBOPENH264)
  include_directories(${OPENH264_INCLUDE_DIR})
  set(OPENH264_C
		h264/common.c
		h264/openh264.c
    )
endif()

if(TVNC_FFH264)
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/h264/")
  find_package(Ffmpeg)
endif()

if(AVCODEC_FOUND AND AVUTIL_FOUND AND SWSCALE_FOUND)
  set(LIBVNCSERVER_HAVE_FFH264 1)
  add_definitions(-DLIBVNCSERVER_HAVE_FFH264)
  include_directories(${AVCODEC_INCLUDE_DIRS} ${AVUTIL_INCLUDE_DIRS} ${SWSCALE_INCLUDE_DIRS})
  set(OPENH264_C
		h264/common.c
		h264/ffh264.c
    )
else()
  message(WARNING "FFMPEG can't be found")
endif()

set(DEFAULT_TVNC_USEPAM 1)
option(TVNC_USEPAM "Include PAM authentication support in the TurboVNC Server"
	${DEFAULT_TVNC_USEPAM})

include_directories(. ../../fb ../../mi ../../os ../../randr ../../render
	${CMAKE_SOURCE_DIR}/common/rfb ${TJPEG_INCLUDE_DIR})

add_definitions(${ServerOSDefines})
set(PAMSRC "")
if(TVNC_USEPAM)
	if(NOT APPLE)
		check_include_files(security/pam_appl.h HAVE_PAM_APPL_H)
		if(NOT HAVE_PAM_APPL_H)
			message(FATAL_ERROR "Could not find PAM headers")
		endif()
	endif()
	add_definitions(-DXVNC_AuthPAM)
	set(PAMSRC authpam.c)
endif()

set(NVCTRLSRC "")
if(TVNC_NVCONTROL)
	set(NVCTRLSRC nvctrlext.c)
	include_directories(${CMAKE_SOURCE_DIR}/unix/libXNVCtrl ${X11_INCLUDE_DIR})
endif()

set(TVNC_USETLS_DESC
	"Implement server-side TLS encryption using the specified library [Options: OpenSSL (default), None]")
set(TVNC_USETLS "OpenSSL" CACHE STRING ${TVNC_USETLS_DESC})
set_property(CACHE TVNC_USETLS PROPERTY STRINGS OpenSSL None)
string(TOLOWER "${TVNC_USETLS}" TVNC_USETLS)

if(APPLE OR CMAKE_SYSTEM_NAME MATCHES "(OpenBSD|FreeBSD|NetBSD|DragonFly)")
	find_path(ICONV_INCLUDE_DIR iconv.h)
	set_property(SOURCE cutpaste.c APPEND_STRING PROPERTY COMPILE_FLAGS
		" -I${ICONV_INCLUDE_DIR}")
endif()

if(TVNC_USETLS STREQUAL "openssl")
	include(FindOpenSSL)
	if(NOT OPENSSL_FOUND)
		message(STATUS "OpenSSL not found.  Disabling TLS encryption")
		set(TVNC_USETLS "None" CACHE STRING ${TVNC_USETLS_DESC} FORCE)
	else()
		set(RFBSSLSRC rfbssl_openssl.c)
		message(STATUS "Enabling TLS encryption using OpenSSL")
		add_definitions(-DUSETLS)
		set_property(SOURCE ${RFBSSLSRC} APPEND_STRING PROPERTY COMPILE_FLAGS
			" -I${OPENSSL_INCLUDE_DIR}")
	endif()
	set(DEFAULT_TVNC_DLOPENSSL 1)
	option(TVNC_DLOPENSSL "Load OpenSSL using dlopen()/dlsym() instead of linking directly with it"
		${DEFAULT_TVNC_DLOPENSSL})
	if(TVNC_DLOPENSSL)
		add_definitions(-DDLOPENSSL)
	endif()
else()
	message(STATUS "Disabling TLS encryption")
endif()

check_function_exists(strsep HAVE_STRSEP)
if(NOT HAVE_STRSEP)
	set(STRSEPSRC strsep.c)
else()
	add_definitions(-DHAVE_STRSEP)
endif()

add_library(vnc STATIC
	auth.c
	base64.c
	cmap.c
	corre.c
	cursor.c
	cutpaste.c
	dispcur.c
	draw.c
	flowcontrol.c
	hextile.c
	init.c
	input-xkb.c
	kbdptr.c
	randr.c
	rfbscreen.c
	rfbserver.c
	rre.c
	sockets.c
	sprite.c
	stats.c
	${STRSEPSRC}
	tight.c
	translate.c
	vncextinit.c
	websockets.c
	ws_decode.c
	zlib.c
	zrle.c
	zrleoutstream.c
	zrlepalettehelper.c
	${PAMSRC}
	${NVCTRLSRC}
	${RFBSSLSRC}
	${OPENH264_C}
	${VNC_AUDIO_C})

if(LIBVNCSERVER_HAVE_LIBOPENH264)
	target_link_libraries(vnc ${OPENH264_LIBRARIES})
endif()

if(LIBVNCSERVER_HAVE_FFH264)
	target_link_libraries(vnc ${AVCODEC_LIBRARIES} ${AVUTIL_LIBRARIES} ${SWSCALE_LIBRARIES})
endif()

if(TVNC_USETLS STREQUAL "openssl" AND NOT TVNC_DLOPENSSL)
	target_link_libraries(vnc ${OPENSSL_LIBRARIES})
endif()
