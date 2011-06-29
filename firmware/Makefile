#
# Configuration
#

# Device characteristics
DEVICE=attiny45
DEVICE_RAM=256
DEVICE_FLASH=4096

# Define which programmer to use
AVRDUDE = avrdude -c avrispv2 -P /dev/ttyACM0 -p $(DEVICE)

# How do we need to compile the project?
# NOTE: DO NOT compile the final project with debugging symbols
#       Any debug output will distort the timings so that the specs can't be met
COMPILE = avr-gcc -Wall -Os -Iusbdrv -I. -mmcu=$(DEVICE) -DF_CPU=16500000 -DDEBUG_LEVEL=0

# Fuse high byte:
# 0xdd = 1 1 0 1   1 1 0 1
#        ^ ^ ^ ^   ^ \-+-/ 
#        | | | |   |   +------ BODLEVEL 2..0 (brownout trigger level -> 2.7V)
#        | | | |   +---------- EESAVE (preserve EEPROM on Chip Erase -> not preserved)
#        | | | +-------------- WDTON (watchdog timer always on -> disable)
#        | | +---------------- SPIEN (enable serial programming -> enabled)
#        | +------------------ DWEN (debug wire enable)
#        +-------------------- RSTDISBL (disable external reset -> enabled)
FUSE_HIGH=0xdd

# Fuse low byte:
# 0xe1 = 1 1 1 0   0 0 0 1
#        ^ ^ \+/   \--+--/
#        | |  |       +------- CKSEL 3..0 (clock selection -> HF PLL)
#        | |  +--------------- SUT 1..0 (BOD enabled, fast rising power)
#        | +------------------ CKOUT (clock output on CKOUT pin -> disabled)
#        +-------------------- CKDIV8 (divide clock by 8 -> don't divide)
FUSE_LOW=0xe1


#
# Sybmolic targets
#

all:	main.hex

.c.o:
	$(COMPILE) -c $< -o $@

.S.o:
	$(COMPILE) -c $< -o $@

.c.s:
	$(COMPILE) -S $< -o $@

flash:	all
	$(AVRDUDE) -U flash:w:main.hex:i

fuse:
	$(AVRDUDE) -U hfuse:w:$(FUSE_HIGH):m -U lfuse:w:$(FUSE_LOW):m

readcal:
	$(AVRDUDE) -U calibration:r:/dev/stdout:i | head -1

clean:
	rm -f main.hex main.lst main.obj main.cof main.list main.map main.eep.hex main.bin main.asm *.o usbdrv/*.o main.s usbdrv/oddebug.s usbdrv/usbdrv.s


#
# File targets
#

OBJECTS = usbdrv/usbdrv.o usbdrv/usbdrvasm.o usbdrv/oddebug.o main.o

# Main binary
main.bin:	$(OBJECTS)
	$(COMPILE) -o main.bin $(OBJECTS)

# Hex version of the binary
main.hex:	main.bin
	rm -f main.hex main.eep.hex
	avr-objcopy -j .text -j .data -O ihex main.bin main.hex
	./checksize main.bin $(DEVICE_FLASH) $(DEVICE_RAM)

# Disassembly
main.asm:	main.bin
	avr-objdump -d main.bin > main.asm

