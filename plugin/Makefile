GREEN="\033[0;32m"
NOCOLOR="\033[0m"

TARGET	= vitc
OBJS	= src/encoder.o src/main.o
LIBS	= -lSceIofilemgrForDriver_stub -lSceNetPsForDriver_stub \
		  -lSceModulemgrForKernel_stub -lSceDebugForDriver_stub \
		  -lSceThreadmgrForDriver_stub -lSceDisplayForDriver_stub \
		  -lSceSysclibForDriver_stub -lSceSysmemForDriver_stub \
		  -lSceIftuForDriver_stub

PREFIX	= arm-vita-eabi
CC	= $(PREFIX)-gcc
CFLAGS	+= -Wl,-q -Wall -O2 -nostdlib -nostartfiles -mcpu=cortex-a9 -mthumb-interwork -Iinclude
DEPS	= $(OBJS:.o=.d)

all: $(TARGET).skprx clean

%.skprx: %.velf
	vita-make-fself -c $< $@

%.velf: %.elf
	vita-elf-create -e $(TARGET).yml $< $@

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

.PHONY: clean send

clean:
	@rm -rf $(TARGET).velf $(TARGET).elf $(OBJS) $(DEPS)

send: $(TARGET).skprx
	curl -T $(TARGET).skprx ftp://$(PSVITAIP):1337/ur0:/tai/$(TARGET).skprx
	@$(MAKE) clean
	@echo ${GREEN}"Sent."${NOCOLOR}