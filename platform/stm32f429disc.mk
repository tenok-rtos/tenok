#32F429IDISCOVERY board (https://www.st.com/en/evaluation-tools/32f429idiscovery.html)

CFLAGS+=-D USE_STM32F29
CFLAGS+=-Wl,-T,platform/stm32f429.ld
