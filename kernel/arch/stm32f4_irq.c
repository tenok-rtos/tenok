#include <stdint.h>

#define ISR __attribute__((weak, alias("Default_Handler")))

void Default_Handler(void)
{
    while(1);
}

void Reset_Handler(void);
ISR void NMI_Handler(void);
ISR void HardFault_Handler(void);
ISR void MemManage_Handler(void);
ISR void BusFault_Handler(void);
ISR void UsageFault_Handler(void);
ISR void SVC_Handler(void);
ISR void DebugMon_Handler(void);
ISR void PendSV_Handler(void);
ISR void SysTick_Handler(void);
ISR void WWDG_IRQHandler(void);
ISR void PVD_IRQHandler(void);
ISR void TAMP_STAMP_IRQHandler(void);
ISR void RTC_WKUP_IRQHandler(void);
ISR void FLASH_IRQHandler(void);
ISR void RCC_IRQHandler(void);
ISR void EXTI0_IRQHandler(void);
ISR void EXTI1_IRQHandler(void);
ISR void EXTI2_IRQHandler(void);
ISR void EXTI3_IRQHandler(void);
ISR void EXTI4_IRQHandler(void);
ISR void DMA1_Stream0_IRQHandler(void);
ISR void DMA1_Stream1_IRQHandler(void);
ISR void DMA1_Stream2_IRQHandler(void);
ISR void DMA1_Stream3_IRQHandler(void);
ISR void DMA1_Stream4_IRQHandler(void);
ISR void DMA1_Stream5_IRQHandler(void);
ISR void DMA1_Stream6_IRQHandler(void);
ISR void ADC_IRQHandler(void);
ISR void CAN1_TX_IRQHandler(void);
ISR void CAN1_RX0_IRQHandler(void);
ISR void CAN1_RX1_IRQHandler(void);
ISR void CAN1_SCE_IRQHandler(void);
ISR void EXTI9_5_IRQHandler(void);
ISR void TIM1_BRK_TIM9_IRQHandler(void);
ISR void TIM1_UP_TIM10_IRQHandler(void);
ISR void TIM1_TRG_COM_TIM11_IRQHandler(void);
ISR void TIM1_CC_IRQHandler(void);
ISR void TIM2_IRQHandler(void);
ISR void TIM3_IRQHandler(void);
ISR void TIM4_IRQHandler(void);
ISR void I2C1_EV_IRQHandler(void);
ISR void I2C1_ER_IRQHandler(void);
ISR void I2C2_EV_IRQHandler(void);
ISR void I2C2_ER_IRQHandler(void);
ISR void SPI1_IRQHandler(void);
ISR void SPI2_IRQHandler(void);
ISR void USART1_IRQHandler(void);
ISR void USART2_IRQHandler(void);
ISR void USART3_IRQHandler(void);
ISR void EXTI15_10_IRQHandler(void);
ISR void RTC_Alarm_IRQHandler(void);
ISR void OTG_FS_WKUP_IRQHandler(void);
ISR void TIM8_BRK_TIM12_IRQHandler(void);
ISR void TIM8_UP_TIM13_IRQHandler(void);
ISR void TIM8_TRG_COM_TIM14_IRQHandler(void);
ISR void TIM8_CC_IRQHandler(void);
ISR void DMA1_Stream7_IRQHandler(void);
ISR void FSMC_IRQHandler(void);
ISR void SDIO_IRQHandler(void);
ISR void TIM5_IRQHandler(void);
ISR void SPI3_IRQHandler(void);
ISR void UART4_IRQHandler(void);
ISR void UART5_IRQHandler(void);
ISR void TIM6_DAC_IRQHandler(void);
ISR void TIM7_IRQHandler(void);
ISR void DMA2_Stream0_IRQHandler(void);
ISR void DMA2_Stream1_IRQHandler(void);
ISR void DMA2_Stream2_IRQHandler(void);
ISR void DMA2_Stream3_IRQHandler(void);
ISR void DMA2_Stream4_IRQHandler(void);
ISR void ETH_IRQHandler(void);
ISR void ETH_WKUP_IRQHandler(void);
ISR void CAN2_TX_IRQHandler(void);
ISR void CAN2_RX0_IRQHandler(void);
ISR void CAN2_RX1_IRQHandler(void);
ISR void CAN2_SCE_IRQHandler(void);
ISR void OTG_FS_IRQHandler(void);
ISR void DMA2_Stream5_IRQHandler(void);
ISR void DMA2_Stream6_IRQHandler(void);
ISR void DMA2_Stream7_IRQHandler(void);
ISR void USART6_IRQHandler(void);
ISR void I2C3_EV_IRQHandler(void);
ISR void I2C3_ER_IRQHandler(void);
ISR void OTG_HS_EP1_OUT_IRQHandler(void);
ISR void OTG_HS_EP1_IN_IRQHandler(void);
ISR void OTG_HS_WKUP_IRQHandler(void);
ISR void OTG_HS_IRQHandler(void);
ISR void DCMI_IRQHandler(void);
ISR void CRYP_IRQHandler(void);
ISR void HASH_RNG_IRQHandler(void);
ISR void FPU_IRQHandler(void);

extern uint32_t _estack;

typedef void (*irq_handler_t)(void);

__attribute__((section(".isr_vector")))
irq_handler_t isr_vectors[] = {
    (irq_handler_t)&_estack,
    /* exceptions */
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    MemManage_Handler,
    BusFault_Handler,
    UsageFault_Handler,
    0,
    0,
    0,
    0,
    SVC_Handler,
    DebugMon_Handler,
    0,
    PendSV_Handler,
    SysTick_Handler,
    /* interrupts */
    WWDG_IRQHandler,
    PVD_IRQHandler,
    TAMP_STAMP_IRQHandler,
    RTC_WKUP_IRQHandler,
    FLASH_IRQHandler,
    RCC_IRQHandler,
    EXTI0_IRQHandler,
    EXTI1_IRQHandler,
    EXTI2_IRQHandler,
    EXTI3_IRQHandler,
    EXTI4_IRQHandler,
    DMA1_Stream0_IRQHandler,
    DMA1_Stream1_IRQHandler,
    DMA1_Stream2_IRQHandler,
    DMA1_Stream3_IRQHandler,
    DMA1_Stream4_IRQHandler,
    DMA1_Stream5_IRQHandler,
    DMA1_Stream6_IRQHandler,
    ADC_IRQHandler,
    CAN1_TX_IRQHandler,
    CAN1_RX0_IRQHandler,
    CAN1_RX1_IRQHandler,
    CAN1_SCE_IRQHandler,
    EXTI9_5_IRQHandler,
    TIM1_BRK_TIM9_IRQHandler,
    TIM1_UP_TIM10_IRQHandler,
    TIM1_TRG_COM_TIM11_IRQHandler,
    TIM1_CC_IRQHandler,
    TIM2_IRQHandler,
    TIM3_IRQHandler,
    TIM4_IRQHandler,
    I2C1_EV_IRQHandler,
    I2C1_ER_IRQHandler,
    I2C2_EV_IRQHandler,
    I2C2_ER_IRQHandler,
    SPI1_IRQHandler,
    SPI2_IRQHandler,
    USART1_IRQHandler,
    USART2_IRQHandler,
    USART3_IRQHandler,
    EXTI15_10_IRQHandler,
    RTC_Alarm_IRQHandler,
    OTG_FS_WKUP_IRQHandler,
    TIM8_BRK_TIM12_IRQHandler,
    TIM8_UP_TIM13_IRQHandler,
    TIM8_TRG_COM_TIM14_IRQHandler,
    TIM8_CC_IRQHandler,
    DMA1_Stream7_IRQHandler,
    FSMC_IRQHandler,
    SDIO_IRQHandler,
    TIM5_IRQHandler,
    SPI3_IRQHandler,
    UART4_IRQHandler,
    UART5_IRQHandler,
    TIM6_DAC_IRQHandler,
    TIM7_IRQHandler,
    DMA2_Stream0_IRQHandler,
    DMA2_Stream1_IRQHandler,
    DMA2_Stream2_IRQHandler,
    DMA2_Stream3_IRQHandler,
    DMA2_Stream4_IRQHandler,
    ETH_IRQHandler,
    ETH_WKUP_IRQHandler,
    CAN2_TX_IRQHandler,
    CAN2_RX0_IRQHandler,
    CAN2_RX1_IRQHandler,
    CAN2_SCE_IRQHandler,
    OTG_FS_IRQHandler,
    DMA2_Stream5_IRQHandler,
    DMA2_Stream6_IRQHandler,
    DMA2_Stream7_IRQHandler,
    USART6_IRQHandler,
    I2C3_EV_IRQHandler,
    I2C3_ER_IRQHandler,
    OTG_HS_EP1_OUT_IRQHandler,
    OTG_HS_EP1_IN_IRQHandler,
    OTG_HS_WKUP_IRQHandler,
    OTG_HS_IRQHandler,
    DCMI_IRQHandler,
    CRYP_IRQHandler,
    HASH_RNG_IRQHandler,
    FPU_IRQHandler
};
