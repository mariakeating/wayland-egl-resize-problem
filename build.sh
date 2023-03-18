#!/bin/sh

wayland-scanner private-code \
	< /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
	> xdg-shell-protocol.c

wayland-scanner client-header \
	< /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
	> xdg-shell-client-protocol.h

wayland-scanner private-code \
	< /usr/share/wayland-protocols/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml \
	> xdg-decoration-protocol.c

wayland-scanner client-header \
	< /usr/share/wayland-protocols/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml \
	> xdg-decoration-client-protocol.h

LINKER_FLAGS="-lwayland-client -lwayland-egl -lEGL -lGL"
gcc minimal-egl.c -DUSE_DECORATIONS=1 -o minimal-egl-decorations $LINKER_FLAGS
gcc minimal-egl.c -DUSE_DECORATIONS=0 -o minimal-egl-no-decorations $LINKER_FLAGS

gcc working-kde-minimal-egl.c -DUSE_DECORATIONS=1 -o working-kde-minimal-egl-decorations $LINKER_FLAGS
gcc working-kde-minimal-egl.c -DUSE_DECORATIONS=0 -o working-kde-minimal-egl-no-decorations $LINKER_FLAGS
