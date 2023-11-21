# 32F429IDISCOVERY board (https://www.st.com/en/evaluation-tools/32f429idiscovery.html)

LD_SCRIPT = platform/stm32f429.ld

CFLAGS += -D STM32F429_439xx \
          -D ENABLE_UART1_DMA=1 \
          -D ENABLE_UART3_DMA=1 \
          -D __ARCH__=\"armv7m\"

CFLAGS += -D__BOARD_NAME__=\"stm32f429\"

CFLAGS += -I ./lib/STM32F429I-Discovery
CFLAGS += -I ./lib/STM32F429I-Discovery/Common

CFLAGS += -I ./drivers/boards

SRC += $(ST_LIB)/src/stm32f4xx_fmc.c \
       $(ST_LIB)/src/stm32f4xx_ltdc.c \
       $(ST_LIB)/src/stm32f4xx_dma2d.c \
       $(ST_LIB)/src/stm32f4xx_syscfg.c \
       $(ST_LIB)/src/stm32f4xx_exti.c

SRC += ./lib/STM32F429I-Discovery/stm32f429i_discovery.c \
       ./lib/STM32F429I-Discovery/stm32f429i_discovery_lcd.c \
       ./lib/STM32F429I-Discovery/stm32f429i_discovery_ioe.c \
       ./lib/STM32F429I-Discovery/stm32f429i_discovery_sdram.c

SRC += ./drivers/boards/stm32f429disc.c

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
