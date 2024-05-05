OUT := out
SRC := src

SOURCE_FILES := $(wildcard $(SRC)/*.c)

CFLAGS = -Wall -Wextra -Werror -O2

all: $(OUT) $(OUT)/ycc

$(OUT):
	mkdir $@

$(OUT)/ycc: $(SOURCE_FILES)
	$(CC) $(CFLAGS) $? -o $@

clean: $(OUT)
	rm -rf $?
