# Makefile for gb_mnist v4 (full UI: title + menu + draw mode + credits)
CC := sdcc
AS := sdasgb
TARGET := -msm83

LDFLAGS := --no-std-crt0 \
           -Wl-b_HOME=0x0200 \
           -Wl-b_GSINIT=0x02F0 \
           -Wl-b_GSFINAL=0x02F0 \
           -Wl-b_INITIALIZER=0x02F1 \
           -Wl-b_CODE=0x0400 \
           -Wl-b_DATA=0xC000

OBJS := crt0.rel main.rel inference.rel gb_hw.rel \
        ui_tiles.rel font_tiles.rel title_tiles.rel \
        weights.rel fc_asm.rel

ROM := gb_mnist_v6.gb

.PHONY: all clean
all: $(ROM)

$(ROM): $(OBJS)
	$(CC) $(TARGET) $(LDFLAGS) -o gb_mnist_v6.ihx $(OBJS)
	makebin -Z -yo A -yn "GB-MNIST-V6" gb_mnist_v6.ihx $@

%.rel: %.c
	$(CC) $(TARGET) -c $<

%.rel: %.s
	$(AS) -plosgff $<

clean:
	rm -f *.rel *.asm *.lst *.sym *.ihx *.lk *.map *.noi *.cdb *.bi4 $(ROM)
