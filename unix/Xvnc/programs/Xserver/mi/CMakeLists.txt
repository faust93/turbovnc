include_directories(. ../Xi ../composite ../damageext ../render ../xfixes)

disable_compiler_warnings()

add_library(mi STATIC
	miarc.c
	mibitblt.c
	micmap.c
	micopy.c
	midash.c
	midispcur.c
	mieq.c
	miexpose.c
	mifillarc.c
	mifillrct.c
	migc.c
	miglblt.c
	miinitext.c
	mioverlay.c
	mipointer.c
	mipoly.c
	mipolypnt.c
	mipolyrect.c
	mipolyseg.c
	mipolytext.c
	mipushpxl.c
	miscrinit.c
	misprite.c
	mivaltree.c
	miwideline.c
	miwindow.c
	mizerarc.c
	mizerclip.c
	mizerline.c)
