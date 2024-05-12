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

clean: $(OUT)
	rm -rf $?
