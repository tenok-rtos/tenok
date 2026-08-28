Supported Platforms
===================

### ARM Cortex-M4

* [STM32F4DISCOVERY](https://www.st.com/en/evaluation-tools/stm32f4discovery.html) (STM32F407VG)
  - Select with `make stm32f4disc_defconfig`
  - UART1 (console): PA9 (TX), PB7 (RX)
  - UART3 (debug-link): PC10 (TX), PC11 (RX)

* [32F429IDISCOVERY](https://www.st.com/en/evaluation-tools/32f429idiscovery.html) (STM32F429ZI)
  - Select with `make stm32f429disc_defconfig`
  - UART1 (console): PA9 (TX), PB7 (RX)
  - UART3 (debug-link): PC10 (TX), PC11 (RX)

* QEMU Emulation of [netduinoplus2](https://www.qemu.org/docs/master/system/arm/stm32.html) (STM32F405RGT6)
  - Select with `make qemu_defconfig`
