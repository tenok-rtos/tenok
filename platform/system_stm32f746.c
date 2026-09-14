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

/* Where the memory the Ethernet controller reaches begins. Laid out by the
 * linker so that it is a whole number of the units the protection unit works
 * in, and aligned to its own size, which that unit requires
 */
extern uint32_t _eth_dma_start;

/* The fields of a region's attributes, named the way the architecture names
 * them rather than by the number of the bit they sit at
 */
#define MPU_ATTR(xn, ap, tex, s, c, b)                                         \
    (((xn) << 28) | ((ap) << 24) | ((tex) << 19) | ((s) << 18) | ((c) << 17) | \
     ((b) << 16))

/* Normal memory the core may keep a copy of, normal memory it may not, and
 * memory that is not memory at all
 */
#define MPU_NORMAL_CACHED(xn) MPU_ATTR(xn, 3u, 1u, 0u, 1u, 1u)
#define MPU_NORMAL_UNCACHED(xn) MPU_ATTR(xn, 3u, 1u, 1u, 0u, 0u)
#define MPU_DEVICE(xn) MPU_ATTR(xn, 3u, 0u, 1u, 0u, 1u)

static void mpu_region(uint32_t region,
                       uint32_t base,
                       uint32_t size_log2,
                       uint32_t attr)
{
    MPU->RNR = region;
    MPU->RBAR = base & MPU_RBAR_ADDR_Msk;
    MPU->RASR =
        attr | ((size_log2 - 1u) << MPU_RASR_SIZE_Pos) | MPU_RASR_ENABLE_Msk;
}

/* Describe memory to the protection unit.
 *
 * This is not done to protect anything: the threads of this system share one
 * address space and are not kept out of each other's memory. It is done
 * because the unit is also what says which memory the core may keep a copy of,
 * and one run of memory -- the one the Ethernet controller reads and writes
 * behind the core's back -- must be left out of that.
 *
 * Every region says full access, because a thread here is unprivileged and
 * losing the background region would otherwise stop it reaching anything at
 * all. What differs between them is only whether a copy may be kept.
 */
static void mpu_init(void)
{
    __DMB();
    MPU->CTRL = 0;

    /* The flash the program is read from */
    mpu_region(0, 0x08000000u, 20u, MPU_NORMAL_CACHED(0u));

    /* Everything the linker places, which is all of the internal memory */
    mpu_region(1, 0x20000000u, 19u, MPU_NORMAL_CACHED(0u));

    /* The peripherals, which answer differently every time they are read and
     * so may never be kept
     */
    mpu_region(2, 0x40000000u, 29u, MPU_DEVICE(1u));

    /* The one run of memory shared with a bus master */
    mpu_region(3, (uint32_t) &_eth_dma_start, 13u, MPU_NORMAL_UNCACHED(1u));

    /* The default map still stands for anything not named above, which only
     * privileged code reaches
     */
    MPU->CTRL = MPU_CTRL_PRIVDEFENA_Msk | MPU_CTRL_ENABLE_Msk;

    __DSB();
    __ISB();
}

void SystemInit(void)
{
    /* Let the floating point unit be reached */
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
    SCB->CPACR |= ((3UL << 20) | (3UL << 22));
#endif

    clock_init();

    /* Keep what has been executed, so that the flash is asked for it once
     * rather than every time round a loop.
     *
     * Nothing in this system writes the memory it executes from -- what runs
     * is in flash and stays there -- so there is nothing for the copy the core
     * keeps to fall out of step with. That is what makes this one safe to turn
     * on and its counterpart for data not.
     */
    SCB_EnableICache();

    /* Said before any copy of data is kept, so that what must not be kept is
     * already known not to be
     */
    mpu_init();

    /* Keep what has been read and written, everywhere the region above has
     * not said otherwise
     */
    SCB_EnableDCache();
}
