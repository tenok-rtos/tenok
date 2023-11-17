Supported Platforms
===================

## ARM Cortex-M4

* [STM32F4DISCOVERY](https://www.st.com/en/evaluation-tools/stm32f4discovery.html) (STM32F407VG)
  - Select by enabling `include platform/stm32f4disc.mk` in the Makefile
  - UART1 (console): PA9 (TX), PB7 (RX)
  - UART3 (debug-link): PC10 (TX), PC11 (RX)

* [32F429IDISCOVERY](https://www.st.com/en/evaluation-tools/32f429idiscovery.html) (STM32F429ZI)
  - Select by enabling `include platform/stm32f429disc.mk` in the Makefile
  - UART1 (console): PA9 (TX), PB7 (RX)
  - UART3 (debug-link): PC10 (TX), PC11 (RX)

* QEMU Emulation of [netduinoplus2](https://www.qemu.org/docs/master/system/arm/stm32.html) (STM32F405RGT6)
  - Select by enabling `include platform/qemu.mk` in the Makefile
