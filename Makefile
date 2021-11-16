PROJECT=naive_f4_os
ELF=$(PROJECT).elf
BIN=$(PROJECT).bin

CC=arm-none-eabi-gcc
OBJCOPY=arm-none-eabi-objcopy

CFLAGS=-g -mlittle-endian -mthumb \
	-mcpu=cortex-m4 \
	-mfpu=fpv4-sp-d16 -mfloat-abi=hard \
	--specs=nano.specs \
	--specs=nosys.specs
CFLAGS+=-D USE_STDPERIPH_DRIVER
CFLAGS+=-D STM32F4xx
CFLAGS+=-D __FPU_PRESENT=1 \
        -D ARM_MATH_CM4 \
	-D __FPU_USED=1

CFLAGS+=-Wl,-T,startup/stm32_flash.ld

ST_LIB=./lib/STM32F4xx_StdPeriph_Driver
CFLAGS+=-I./lib/CMSIS/ST/STM32F4xx/Include
CFLAGS+=-I./lib/CMSIS/Include
CFLAGS+=-I$(ST_LIB)/inc

CFLAGS+=-I./
CFLAGS+=-I./startup/
CFLAGS+=-I./src/

SRC=./lib/CMSIS/system_stm32f4xx.c

SRC+=$(ST_LIB)/src/misc.c \
	$(ST_LIB)/src/stm32f4xx_rcc.c \
	$(ST_LIB)/src/stm32f4xx_dma.c \
	$(ST_LIB)/src/stm32f4xx_flash.c \
	$(ST_LIB)/src/stm32f4xx_gpio.c \
	$(ST_LIB)/src/stm32f4xx_usart.c \
	$(ST_LIB)/src/stm32f4xx_tim.c\
	$(ST_LIB)/src/stm32f4xx_spi.c\
	$(ST_LIB)/src/stm32f4xx_i2c.c

SRC+=./src/task.c \
	./src/uart.c \
	./src/main.c

STARTUP=./startup/startup_stm32f4xx.s

all:$(BIN)

$(BIN):$(ELF)
	$(OBJCOPY) -O binary $^ $@

STARTUP_OBJ = startup_stm32f4xx.o

$(STARTUP_OBJ): $(STARTUP) 
	$(CC) $(CFLAGS) $^ -c $(STARTUP)

$(ELF):$(SRC) $(STARTUP_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf $(ELF)
	rm -rf $(BIN)
	rm -rf startup_stm32f4xx.o
flash:
	st-flash write $(BIN) 0x8000000

.PHONY:all clean flash
