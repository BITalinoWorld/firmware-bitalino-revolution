
BUILDDIR = build

CC     = avr-gcc
CPP    = avr-cpp
COPY   = avr-objcopy
SIZE   = avr-size
PROG   = avrdude
DUMP   = avr-objdump 

MCU    = atmega328p
CFLAGS = -mmcu=$(MCU) -std=c99 -Wall -Wextra -O2 -fno-strict-aliasing -g
LFLAGS = -Wl,-Map=$(BUILDDIR)/main.map

OBJS = $(addprefix $(BUILDDIR)/, live.o main.o simul.o testconfig.o timer.o uart.o)

all: $(BUILDDIR) $(BUILDDIR)/main.hex

%.hex: %.elf
	$(COPY) -O ihex -R .eeprom $< $@

$(BUILDDIR)/main.elf: $(OBJS)
	$(CC) $(CFLAGS) $(LFLAGS) $^ -o $@

$(BUILDDIR)/%.o: %.c main.h $(BUILDDIR)/pins.h makefile
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/pins.h: pins.h
	$(CPP) -CC $< $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -f $(BUILDDIR)/pins.h $(BUILDDIR)/*.o $(BUILDDIR)/*.elf $(BUILDDIR)/*.hex

size: $(BUILDDIR)/main.elf
	$(SIZE) $<
	$(SIZE) -C --mcu=$(MCU) $<

list: $(BUILDDIR)/main.lst

$(BUILDDIR)/main.lst: $(BUILDDIR)/main.elf
	$(DUMP) -S $< > $@

#  make send - program firmware on device
send: $(BUILDDIR)/main.hex
	$(PROG) -P usb -p m328p -c avrispmkii -B 5 -U flash:w:$< -U lfuse:w:0xFF:m -U hfuse:w:0xDF:m -U efuse:w:0x05:m

# low fuse:  0xFF - no CKDIV8, no CKOUT, low power crystal (8-16 MHz), slowly rising power
# high fuse: 0xDF - no RSTDISBL, no DWEN, SPIEN, no WDTON, no EESAVE, boot size = 256 words (the minimum), no BOOTRST
# extended fuse: 0x05 - BOD level ~ 2.7 V
