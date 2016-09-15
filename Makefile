PREFIX ?= /usr
DESTDIR ?=
BINDIR ?= $(PREFIX)/bin
LIBDIR ?= $(PREFIX)/lib
MANDIR ?= $(PREFIX)/share/man
ifeq "$(CURL_VERSION)" ""
CURL_VERSION := 7.47.0
endif
ifeq "$(LIBXML_VERSION)" ""
LIBXML_VERSION := 2.9.3
endif
COMPDIR ?= $(shell pkg-config --variable=completionsdir bash-completion 2>/dev/null || echo $(PREFIX)/share/bash-completion/completions)

CFLAGS ?= -O3 -march=native -fomit-frame-pointer -pipe
CFLAGS += -std=gnu99 -D_GNU_SOURCE
CFLAGS += -pedantic -Wall -Wextra -Wno-language-extension-token
CFLAGS += -MMD

UNAME_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
ifeq ($(UNAME_S),Darwin)
HAS_XCODE := $(shell sh -c 'xcodebuild -version 2>/dev/null && echo 1')
CFLAGS += -Wno-deprecated-declarations -I/usr/local/opt/openssl/include
LDFLAGS += -L/usr/local/opt/openssl/lib
endif

ifeq ($(HAS_XCODE),1)
SDKROOT ?= $(shell xcodebuild -version -sdk macosx | sed -n 's/^Path: \(.*\)/\1/p')
CFLAGS += -isysroot $(SDKROOT) -I$(SDKROOT)/usr/include/libxml2
LDLIBS = -lcurl -lxml2 -lssl -lcrypto
else
CC := gcc -static-libgcc
CFLAGS += -I./libxml2-$(LIBXML_VERSION)/include/ -I./curl-$(CURL_VERSION)/include/ -I/usr/include
LDLIBS = ./curl-$(CURL_VERSION)/lib/.libs/libcurl.a ./libxml2-$(LIBXML_VERSION)/.libs/libxml2.a
LDLIBS +=  /usr/lib/x86_64-linux-gnu/libdl.a /usr/lib/x86_64-linux-gnu/liblzma.a
LDLIBS += /usr/lib/x86_64-linux-gnu/libm.a /usr/lib/x86_64-linux-gnu/libz.a
LDLIBS += /usr/lib/x86_64-linux-gnu/libssl.a /usr/lib/x86_64-linux-gnu/libcrypto.a
LDLIBS += -ldl -lm -lz -lssl -lcrypto
ifeq ($(UNAME_S),OpenBSD)
LDLIBS += -lkvm
endif
endif

all: lpass
doc-man: lpass.1
doc-html: lpass.1.html
doc: doc-man doc-html

lpass: $(patsubst %.c,%.o,$(wildcard *.c))
%.1: %.1.txt
	a2x --no-xmllint -f manpage $<
%.1.html: %.1.txt
	asciidoc -b html5 -a data-uri -a icons -a toc2 $<

install-doc: doc-man
	@install -v -d "$(DESTDIR)$(MANDIR)/man1" && install -m 0644 -v lpass.1 "$(DESTDIR)$(MANDIR)/man1/lpass.1"

install: all
	@install -v -d "$(DESTDIR)$(BINDIR)" && install -m 0755 -v lpass "$(DESTDIR)$(BINDIR)/lpass"
	@install -v -d "$(DESTDIR)$(COMPDIR)" && install -m 0644 -v contrib/lpass_bash_completion "$(DESTDIR)$(COMPDIR)/lpass"

uninstall:
	@rm -vrf "$(DESTDIR)$(MANDIR)/man1/lpass.1" "$(DESTDIR)$(BINDIR)/lpass" "$(DESTDIR)$(COMPDIR)/lpass"
	@rmdir "$(DESTDIR)$(MANDIR)/man1" "$(DESTDIR)$(BINDIR)" 2>/dev/null || true

clean:
	rm -f lpass *.o *.d lpass.1 lpass.1.html certificate.h lpass.exe

analyze: clean
	CFLAGS=-g scan-build -enable-checker alpha.core -enable-checker alpha.deadcode -enable-checker alpha.security -enable-checker alpha.unix -enable-checker security -enable-checker core -enable-checker deadcode -enable-checker unix -disable-checker alpha.core.PointerSub --view --keep-going $(MAKE) lpass

.PHONY: all doc doc-man doc-html test-deps clean analyze

-include *.d
