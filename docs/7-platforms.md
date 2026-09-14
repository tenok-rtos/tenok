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

### ARM Cortex-M7

* [NUCLEO-F746ZG](https://www.st.com/en/evaluation-tools/nucleo-f746zg.html) (STM32F746ZG)
  - Select with `make nucleo_f746zg_defconfig`
  - USART3 (console): PD8 (TX), PD9 (RX), carried to the host by the debugger
  - Ethernet: LAN8742A, RMII. See [STM32F7 and Networking](10-stm32f7.md)

* QEMU Emulation of [netduinoplus2](https://www.qemu.org/docs/master/system/arm/stm32.html) (STM32F405RGT6)
  - Select with `make qemu_defconfig`
