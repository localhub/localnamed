CXXFLAGS+=\
	-Wall\
	-Wextra\
	-Wno-unused-parameter\
	-Werror\
	--std=c++11\
	--stdlib=libc++\
	-D_XOPEN_SOURCE=1\
	-Iinclude\
	-framework CoreFoundation\
	-framework CoreServices\
	deps/uv/libuv.a

INSTALL_PATH?=/

.PHONY: all debug install uninstall unload clean
.DEFAULT: all

all: localnamed

debug: CXXFLAGS+= -g
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

localnamed: src/* deps/uv/libuv.a
	$(CXX) $(CXXFLAGS) -o $@ src/*.cpp

deps/uv/libuv.a: deps/uv/Makefile
	$(MAKE) -C deps/uv

deps/uv/Makefile:
	git submodule init && git submodule update
