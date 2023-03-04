#STM32F4DISCOVERY board (https://www.st.com/en/evaluation-tools/stm32f4discovery.html)

CFLAGS+=-D USE_STM32F07
CFLAGS+=-Wl,-T,platform/stm32f407.ld
