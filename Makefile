CCFLAGS+=\
	-Wall\
	-Wextra\
	-Iinclude\
	deps/uv/.libs/libuv.a

INSTALL_PATH?=/

.PHONY: all debug install uninstall unload clean
.DEFAULT: all

all: localnamed

debug: CCFLAGS+= -g
debug: all

install: all unload
	install -m 755 localnamed /usr/libexec
	install -m 644 us.sidnicio.localnamed.plist /Library/LaunchDaemons
	launchctl load /Library/LaunchDaemons/us.sidnicio.localnamed.plist

uninstall: unload
	-rm /usr/libexec/localnamed
	-rm /Library/LaunchDaemons/us.sidnicio.localnamed.plist

unload:
	@launchctl remove us.sidnicio.localnamed ||:


clean:
	$(RM) localnamed

localnamed: src/localnamed.c src/ffilter.h src/ffilter.c
	$(CC) $(CCFLAGS) -o $@ src/ffilter.c $<
