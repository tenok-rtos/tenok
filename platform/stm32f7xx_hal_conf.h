/**
 * @file
 * What the vendor's driver library is told about this part.
 *
 * The tree drives this family through the Low Layer, which is the half of
 * STM32Cube that speaks to registers the way the older library did. The one
 * thing the Low Layer was never given is Ethernet: for that peripheral ST
 * wrote only the higher half, so that half is brought in for it alone and
 * every other module it offers is left out.
 */
#ifndef __STM32F7xx_HAL_CONF_H__
#define __STM32F7xx_HAL_CONF_H__

#include <stdint.h>

#define HAL_ETH_MODULE_ENABLED

/* The tree is built with every conditional required to say what it is, so the
 * switches this module only ever tests are answered rather than left out
 */
#define USE_HAL_ETH_REGISTER_CALLBACKS 0U

/* Timekeeping is not this half's to do here, so the three calls it makes are
 * answered by the driver that brings it in. This is where it is told they will
 * be, since the file it would otherwise look in belongs to modules the tree
 * does not build
 */
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t ms);
uint32_t HAL_RCC_GetHCLKFreq(void);

/* The board feeds 8 MHz in from its debugger rather than driving a crystal of
 * its own, and the part is left on the internal oscillator only until the
 * clock is raised
 */
#if !defined(HSE_VALUE)
#define HSE_VALUE 8000000U
#endif
#if !defined(HSI_VALUE)
#define HSI_VALUE 16000000U
#endif
#if !defined(LSE_VALUE)
#define LSE_VALUE 32768U
#endif
#if !defined(LSI_VALUE)
#define LSI_VALUE 32000U
#endif
#if !defined(HSE_STARTUP_TIMEOUT)
#define HSE_STARTUP_TIMEOUT 100U
#endif
#if !defined(LSE_STARTUP_TIMEOUT)
#define LSE_STARTUP_TIMEOUT 5000U
#endif
#if !defined(EXTERNAL_CLOCK_VALUE)
#define EXTERNAL_CLOCK_VALUE 12288000U
#endif

#define VDD_VALUE 3300U
#define TICK_INT_PRIORITY 0x0FU
#define USE_RTOS 0
#define PREFETCH_ENABLE 1
#define ART_ACCELERATOR_ENABLE 1
#define INSTRUCTION_CACHE_ENABLE 0
#define DATA_CACHE_ENABLE 0

/* The transceiver on the far side of the RMII pins, which the library reaches
 * over the management wires and which the board wires at address zero. The
 * register numbers below are not the transceiver's own but the ones every
 * transceiver answers to, so they are named the way the standard names them
 */
#define LAN8742A_PHY_ADDRESS 0x00U
#define PHY_RESET_DELAY 0x00000FFFU
#define PHY_CONFIG_DELAY 0x00000FFFU
#define PHY_READ_TO 0x0000FFFFU
#define PHY_WRITE_TO 0x0000FFFFU

#define PHY_BCR ((uint16_t) 0x0000U) /* Basic control */
#define PHY_BSR ((uint16_t) 0x0001U) /* Basic status */
#define PHY_SR ((uint16_t) 0x001FU)  /* Vendor status, where the speed is read \
                                      */

#define PHY_RESET ((uint16_t) 0x8000U)
#define PHY_AUTONEGOTIATION ((uint16_t) 0x1000U)
#define PHY_LOOPBACK ((uint16_t) 0x4000U)
#define PHY_FULLDUPLEX_100M ((uint16_t) 0x2100U)
#define PHY_HALFDUPLEX_100M ((uint16_t) 0x2000U)
#define PHY_FULLDUPLEX_10M ((uint16_t) 0x0100U)
#define PHY_HALFDUPLEX_10M ((uint16_t) 0x0000U)
#define PHY_POWERDOWN ((uint16_t) 0x0800U)
#define PHY_ISOLATE ((uint16_t) 0x0400U)

#define PHY_LINKED_STATUS ((uint16_t) 0x0004U)
#define PHY_AUTONEGO_COMPLETE ((uint16_t) 0x0020U)
#define PHY_SPEED_STATUS ((uint16_t) 0x0004U)
#define PHY_DUPLEX_STATUS ((uint16_t) 0x0010U)

/* The one thing the Ethernet module asks of a module this tree does not bring
 * in: the clock of the block that decides whether the pins carry MII or RMII
 */
#define __HAL_RCC_SYSCFG_CLK_ENABLE()                          \
    do {                                                       \
        __IO uint32_t tmpreg;                                  \
        SET_BIT(RCC->APB2ENR, RCC_APB2ENR_SYSCFGEN);           \
        tmpreg = READ_BIT(RCC->APB2ENR, RCC_APB2ENR_SYSCFGEN); \
        (void) tmpreg;                                         \
    } while (0)

/* The library checks what it is handed. Nothing here is worth the code it
 * would take to say so at run time, so it is left to say nothing
 */
#define assert_param(expr) ((void) 0U)

#ifdef HAL_ETH_MODULE_ENABLED
#include "stm32f7xx_hal_eth.h"
#endif

#endif
