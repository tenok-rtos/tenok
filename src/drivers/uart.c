#include <stm32f4xx.h>
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

	/* initialize the gpio */
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource10, GPIO_AF_USART3);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource11, GPIO_AF_USART3);
	GPIO_InitTypeDef GPIO_InitStruct = {
		.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11,
		.GPIO_Mode = GPIO_Mode_AF,
		.GPIO_Speed = GPIO_Speed_50MHz,
		.GPIO_OType = GPIO_OType_PP,
		.GPIO_PuPd = GPIO_PuPd_UP
	};
	GPIO_Init(GPIOC, &GPIO_InitStruct);

	/* initialize the uart3 */
	USART_InitTypeDef USART_InitStruct = {
		.USART_BaudRate = 115200,
		.USART_Mode = USART_Mode_Rx | USART_Mode_Tx,
		.USART_WordLength = USART_WordLength_8b,
		.USART_StopBits = USART_StopBits_1,
		.USART_Parity = USART_Parity_No
	};
	USART_Init(USART3, &USART_InitStruct);
	USART_Cmd(USART3, ENABLE);
	USART_ClearFlag(USART3, USART_FLAG_TC);

	/* enable uart3's interrupt */
	NVIC_InitTypeDef NVIC_InitStruct = {
		.NVIC_IRQChannel = USART3_IRQn,
		.NVIC_IRQChannelPreemptionPriority = 0,
		.NVIC_IRQChannelSubPriority = 0,
		.NVIC_IRQChannelCmd = ENABLE
	};
	NVIC_Init(&NVIC_InitStruct);
	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
}

void USART3_IRQHandler(void)
{
	if(USART_GetITStatus(USART3, USART_IT_RXNE) == SET) {
		recvd_c = USART_ReceiveData(USART3);
		sem_post(&sem_uart3_rx);
	}
}

char uart3_getc(void)
{
	sem_wait(&sem_uart3_rx);
	return recvd_c;
}

void uart3_putc(char c)
{
	/* wait until TXE (Transmit Data Register Empty) flag is set */
	while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
	USART_SendData(USART3, c);
	/* wait until TC (Tranmission Complete) flag is set */
	while(USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);
}

void uart3_puts(char *string)
{
	for(; *string != '\0'; string++) {
		uart3_putc(*string);
	}
}
