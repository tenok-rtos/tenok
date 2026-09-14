/**
 * @file
 * What runs before anything else on an STM32F746, and the number the kernel
 * counts its ticks against.
 *
 * The family this part belongs to was never given a Standard Peripheral
 * Library, and the file its own vendor ships in that library's place brings the
 * part up on the 16 MHz its reset leaves it at and goes no further. So this is
 * kept here instead, the way the older family's copy is, and it takes the part
 * the rest of the way.
 *
 * Nothing here may touch a variable that carries a value: the reset handler
 * calls it before it has copied the initialisers out of flash, so anything
 * written would be written over. That is why the clock the kernel reads is not
 * assigned but declared with the value it will have by the time anyone asks.
 */

#include "stm32f7xx_ll_pwr.h"
#include "stm32f7xx_ll_rcc.h"
#include "stm32f7xx_ll_system.h"

/* The rate the part is left running at, which the reset handler copies into
 * place after this file has already made it true
 */
uint32_t SystemCoreClock = SYSTEM_CORE_CLOCK;

/* The three the vendor's own file declares alongside it, which the peripheral
 * library reads to work out what a bus is running at
 */
const uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                   1, 2, 3, 4, 6, 7, 8, 9};
const uint8_t APBPrescTable[8] = {0, 0, 0, 0, 1, 2, 3, 4};

/* Take the part from the 16 MHz its reset leaves it at up to the 216 MHz it is
 * rated for.
 *
 * The crystal is not the board's own: the debugger on the far end of it feeds
 * in 8 MHz, so the oscillator is bypassed rather than driven. From there the
 * loop divides by 4 to the 2 MHz it wants to count from, multiplies by 216,
 * and halves the 432 MHz that comes out.
 *
 * Above 180 MHz the regulator has to be asked for its over-drive, and it is
 * asked only once the loop is already running: the loop is what the part
 * switches to afterwards, and nothing may be switched to before the regulator
 * can hold it. The flash is told to wait its seven cycles before the switch
 * for the same reason, and in that order.
 */
static void clock_init(void)
{
    /* The regulator is reached over the slower of the two buses */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);

    LL_RCC_HSE_EnableBypass();
    LL_RCC_HSE_Enable();
    while (!LL_RCC_HSE_IsReady())
        ;

    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE, LL_RCC_PLLM_DIV_4, 216,
                                LL_RCC_PLLP_DIV_2);
    LL_RCC_PLL_Enable();
    while (!LL_RCC_PLL_IsReady())
        ;

    LL_PWR_EnableOverDriveMode();
    while (!LL_PWR_IsActiveFlag_OD())
        ;
    LL_PWR_EnableOverDriveSwitching();
    while (!LL_PWR_IsActiveFlag_ODSW())
        ;

    LL_FLASH_SetLatency(LL_FLASH_LATENCY_7);
    while (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_7)
        ;

    /* At this speed the flash answers in its own time and the core waits
     * seven cycles for every read of it. What takes most of that back is the
     * accelerator in front of it, which keeps what was last read and reads
     * ahead of where the core is. It is reset before it is turned on because
     * what it kept while the clock was slower says nothing about now
     */
    FLASH->ACR |= FLASH_ACR_ARTRST;
    FLASH->ACR &= ~FLASH_ACR_ARTRST;
    FLASH->ACR |= FLASH_ACR_ARTEN | FLASH_ACR_PRFTEN;

    /* The two slower buses run as fast as they are allowed to: 54 MHz and 108
     * MHz. The first is where the port this board talks over gets its count
     */
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_4);
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2);

    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
        ;
}

void SystemInit(void)
{
    /* Let the floating point unit be reached */
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
    SCB->CPACR |= ((3UL << 20) | (3UL << 22));
#endif

    clock_init();
}
