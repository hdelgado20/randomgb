GBDK_HOME ?= /opt/gbdk
LCC    := $(GBDK_HOME)/bin/lcc
TARGET := DiscoAleatorio.gb
# MBC1 + RAM con bateria, ROM de 64 KB, SRAM de 8 KB
FLAGS  := -Wm-yt0x03 -Wm-yo4 -Wm-ya1 -Wm-ynDISCOALEATO

all: $(TARGET)

src/preload.h: collection.txt tools/mkdata.py
	python3 tools/mkdata.py collection.txt src/preload.h

$(TARGET): src/main.c src/preload.h src/disc_tiles.h
	$(LCC) $(FLAGS) -o $@ src/main.c

clean:
	rm -f $(TARGET) *.map *.sym *.lst *.ihx *.noi *.asm
