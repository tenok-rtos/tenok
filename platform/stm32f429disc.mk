#32F429IDISCOVERY board (https://www.st.com/en/evaluation-tools/32f429idiscovery.html)

CFLAGS+=-D STM32F429_439xx
CFLAGS+=-Wl,-T,platform/stm32f429.ld
