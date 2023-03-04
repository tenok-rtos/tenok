#STM32F4DISCOVERY board (https://www.st.com/en/evaluation-tools/stm32f4discovery.html)

CFLAGS+=-D STM32F40_41xxx
CFLAGS+=-Wl,-T,platform/stm32f407.ld
