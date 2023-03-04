#32F429IDISCOVERY board (https://www.st.com/en/evaluation-tools/32f429idiscovery.html)

CFLAGS+=-D STM32F429_439xx
CFLAGS+=-Wl,-T,platform/stm32f429.ld

all:

flash:
	openocd -f interface/stlink.cfg \
	-f target/stm32f4x.cfg \
	-c "init" \
	-c "reset init" \
	-c "halt" \
	-c "flash write_image erase $(ELF)" \
	-c "verify_image $(ELF)" \
	-c "reset run" -c shutdown

openocd:
	openocd -s /opt/openocd/share/openocd/scripts/ -f ./gdb/openocd.cfg

.PHONY: all flash openocd
