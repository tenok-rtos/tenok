#include "string.h"
#include "stm32f4xx.h"
#include "semaphore.h"

sem_t sem_uart3_tx;
sem_t sem_uart3_rx;

char recvd_c;

void uart3_init(void)
{
	/* initialize the semaphores */
	sem_init(&sem_uart3_tx, 0, 0);
	sem_init(&sem_uart3_rx, 0, 0);

	/* initialize the rcc */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);

	/* initialize the gpio */
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource10, GPIO_AF_USART3);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource11, GPIO_AF_USART3);
	GPIO_InitTypeDef gpio = {
		.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11,
		.GPIO_Mode = GPIO_Mode_AF,
		.GPIO_Speed = GPIO_Speed_50MHz,
		.GPIO_OType = GPIO_OType_PP,
		.GPIO_PuPd = GPIO_PuPd_UP
	};
	GPIO_Init(GPIOC, &gpio);

	/* initialize the uart3 */
	USART_InitTypeDef uart3 = {
		.USART_BaudRate = 115200,
		.USART_Mode = USART_Mode_Rx | USART_Mode_Tx,
		.USART_WordLength = USART_WordLength_8b,
		.USART_StopBits = USART_StopBits_1,
		.USART_Parity = USART_Parity_No
	};
	USART_Init(USART3, &uart3);
	USART_Cmd(USART3, ENABLE);
	USART_ClearFlag(USART3, USART_FLAG_TC);

	/* enable uart3's interrupt */
	NVIC_InitTypeDef nvic = {
		.NVIC_IRQChannel = USART3_IRQn,
		.NVIC_IRQChannelPreemptionPriority = 10,
		.NVIC_IRQChannelSubPriority = 0,
		.NVIC_IRQChannelCmd = ENABLE
	};
	NVIC_Init(&nvic);

	/* enable dma1's interrupt */
	nvic.NVIC_IRQChannel = DMA1_Stream4_IRQn;
	NVIC_Init(&nvic);
}

#if 0
char uart3_getc(void)
{
	while(USART_GetFlagStatus(USART3, USART_FLAG_RXNE) != SET);
	return USART_ReceiveData(USART3);
}
#else
char uart3_getc(void)
{
	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
	sem_wait(&sem_uart3_rx);
	return recvd_c;
}
#endif

void uart3_putc(char c)
{
	/* wait until TXE (Transmit Data Register Empty) flag is set */
	while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
	USART_SendData(USART3, c);
	/* wait until TC (Tranmission Complete) flag is set */
	while(USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);
}

#if 0
void uart3_puts(char *string)
{
	for(; *string != '\0'; string++) {
		uart3_putc(*string);
	}
}
#else
void uart3_puts(char *string)
{
	int len = strlen(string);

	/* initialize the dma */
	DMA_InitTypeDef DMA_InitStructure = {
		.DMA_BufferSize = (uint32_t)len,
		.DMA_FIFOMode = DMA_FIFOMode_Disable,
		.DMA_FIFOThreshold = DMA_FIFOThreshold_Full,
		.DMA_MemoryBurst = DMA_MemoryBurst_Single,
		.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte,
		.DMA_MemoryInc = DMA_MemoryInc_Enable,
		.DMA_Mode = DMA_Mode_Normal,
		.DMA_PeripheralBaseAddr = (uint32_t)(&USART3->DR),
		.DMA_PeripheralBurst = DMA_PeripheralBurst_Single,
		.DMA_PeripheralInc = DMA_PeripheralInc_Disable,
		.DMA_Priority = DMA_Priority_Medium,
		.DMA_Channel = DMA_Channel_7,
		.DMA_DIR = DMA_DIR_MemoryToPeripheral,
		.DMA_Memory0BaseAddr = (uint32_t)string
	};
	DMA_Init(DMA1_Stream4, &DMA_InitStructure);
	DMA_ClearFlag(DMA1_Stream4, DMA_FLAG_TCIF4);

	/*enable dma and its interrupt */
	DMA_Cmd(DMA1_Stream4, ENABLE);
	DMA_ITConfig(DMA1_Stream4, DMA_IT_TC, ENABLE);

	/* trigger data sending */
	USART_DMACmd(USART3, USART_DMAReq_Tx, ENABLE);

	sem_wait(&sem_uart3_tx);
}
#endif

void USART3_IRQHandler(void)
{
	if(USART_GetITStatus(USART3, USART_IT_RXNE) == SET) {
		recvd_c = USART_ReceiveData(USART3);
		USART_ITConfig(USART3, USART_IT_RXNE, DISABLE);
		sem_post(&sem_uart3_rx);
	}
}

void DMA1_Stream4_IRQHandler(void)
{
	if(DMA_GetITStatus(DMA1_Stream4, DMA_IT_TCIF4) == SET) {
		DMA_ClearITPendingBit(DMA1_Stream4, DMA_IT_TCIF3);
		DMA_ITConfig(DMA1_Stream4, DMA_IT_TC, DISABLE);
		sem_post(&sem_uart3_tx);
	}
}

