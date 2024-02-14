LD_SCRIPT = platform/stm32f427.ld

CFLAGS += -D STM32F427_437xx \
          -D SYSTEM_CORE_CLOCK=180000000 \
          -D HSE_VALUE=16000000 \
          -D PLL_M=8 \
          -D PLL_N=180 \
          -D PLL_P=2 \
          -D PLL_Q=4 \
          -D ENABLE_UART1_DMA=1 \
          -D ENABLE_UART3_DMA=1 \
          -D __ARCH__=\"armv7m\"

CFLAGS += -D__BOARD_NAME__=\"stm32f427\"

CFLAGS += -I ./drivers/boards

SRC += $(ST_LIB)/src/stm32f4xx_syscfg.c \
       $(ST_LIB)/src/stm32f4xx_exti.c

SRC += ./drivers/boards/dynamics_wizard.c

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

.PHONY: flash openocd
