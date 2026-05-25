CC      := x86_64-elf-gcc
LD      := x86_64-elf-ld

ifeq (, $(shell which $(CC) 2>/dev/null))
  CC := gcc
  LD := ld
endif

CFLAGS := \
    -std=c11 \
    -ffreestanding \
    -fno-stack-protector \
    -fno-pic \
    -mno-red-zone \
    -mno-mmx -mno-sse -mno-sse2 \
    -Wall -Wextra -O2 \
    -I../libs/zirvlibc/include \
    -I../zirvflux/include \
    -I../zirvtk/include

LDFLAGS := \
    -nostdlib \
    -static \
    -no-pie \
    -z max-page-size=0x1000

LIBC_SRCS := \
    ../libs/zirvlibc/src/string.c \
    ../libs/zirvlibc/src/ctype.c \
    ../libs/zirvlibc/src/stdio.c \
    ../libs/zirvlibc/src/stdlib.c \
    ../libs/zirvlibc/src/unistd.c \
    ../libs/zirvlibc/src/syscall.c \
    ../libs/zirvlibc/src/datetime.c

LIBC_BUILD := ../build/zirvui-libc
LIBC_OBJS := $(patsubst ../libs/zirvlibc/src/%.c,$(LIBC_BUILD)/%.o,$(LIBC_SRCS))

ZIRVFLUX_LIB := ../zirvflux/libzirvflux.a
ZIRVTK_LIB    := ../zirvtk/target/release/libzirvtk.a

SRCS := src/main.c src/crt0.asm src/stubs.c
OBJS := src/main.o src/crt0.o src/stubs.o

TARGET := zirvui.elf

.PHONY: all clean zirvtk

all: $(TARGET)

$(TARGET): $(OBJS) $(LIBC_OBJS) $(ZIRVFLUX_LIB) $(ZIRVTK_LIB)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.asm
	nasm -f elf64 -o $@ $<

$(LIBC_BUILD)/%.o: ../libs/zirvlibc/src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(ZIRVTK_LIB): zirvtk

zirvtk:
	ZIRVFLUX_DIR=../zirvflux RUSTFLAGS="-C panic=abort" cargo build --release --no-default-features --manifest-path ../zirvtk/Cargo.toml

clean:
	rm -f $(TARGET) $(OBJS)
	rm -rf $(LIBC_BUILD)
