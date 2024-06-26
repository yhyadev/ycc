ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif

OUT := out
SRC := src

SOURCE_FILES := $(wildcard $(SRC)/*.c)

CFLAGS = -Wall -Wextra -Werror -O2 `llvm-config --cflags`

LDFLAGS = `llvm-config --ldflags --libs core --system-libs`

all: $(OUT) $(OUT)/ycc

$(OUT):
	mkdir $@

$(OUT)/ycc: $(SOURCE_FILES)
	$(CC) $(CFLAGS) $(LDFLAGS) $? -o $@

install: $(OUT)/ycc
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $(OUT)/ycc $(DESTDIR)$(PREFIX)/bin
	chmod 777 $(DESTDIR)$(PREFIX)/bin/ycc

uninstall: $(DESTDIR)$(PREFIX)/bin/ycc
	rm -rf $?

clean: $(OUT)
	rm -rf $?

.PHONY: all clean install uninstall
