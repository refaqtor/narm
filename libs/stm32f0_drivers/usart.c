/*****************************************************************/
/* Copyright (C) 2015 Jeff Ciesielski <jeffciesielski@gmail.com> */
/*                                                               */
/* usart: Interrupt driven usart driver for STM32F0              */
/*                                                               */
/* This software may be modified and distributed under the terms */
/* of the MIT license.  See the LICENSE file for details.        */
/*****************************************************************/

#include <stdio.h>
#include <errno.h>

#include <narmos.h>

#include <stm32f0xx_usart.h>
#include <stm32f0xx_misc.h>
#include <stm32f0xx_gpio.h>
#include <stm32f0xx_rcc.h>

#include <usart.h>


/* Queue Storage */
#ifdef USART1_ENABLE
DECLARE_QUEUE(uint8_t, uart1_tx_queue, UART_BUFFER_SIZE);
DECLARE_QUEUE(uint8_t, uart1_rx_queue, UART_BUFFER_SIZE);
#endif

#ifdef USART2_ENABLE
DECLARE_QUEUE(uint8_t, uart2_tx_queue, UART_BUFFER_SIZE);
DECLARE_QUEUE(uint8_t, uart2_rx_queue, UART_BUFFER_SIZE);
#endif

struct usart {
	USART_TypeDef *periph;
	bool autocrlf;
	bool initialized;

	struct {
		void *tx;
		void *rx;
	} buffers;

	struct {
		void (*periph_clock_cmd)(uint32_t, FunctionalState);
		uint32_t rccperiph;
	} rcc;

	struct {
		GPIO_TypeDef *rxport;
		uint16_t rxpin;
		void (*rx_clock_cmd)(uint32_t, FunctionalState);
		uint32_t rx_pin_clock;
		uint8_t  rx_pinsource;
		GPIO_TypeDef *txport;
		uint16_t txpin;
		void (*tx_clock_cmd)(uint32_t, FunctionalState);
		uint32_t tx_pin_clock;
		uint8_t         tx_pinsource;
	} gpios;

	bool rx_overflow;
	uint32_t irq_no;
} usarts[] = {
#ifdef USART1_ENABLE
	{
		.periph = USART1,
		.autocrlf = false,
		.initialized = false,
		.buffers = {
			.tx = &uart1_tx_queue,
			.rx = &uart1_rx_queue,
		},
		.rcc = {
			.periph_clock_cmd = RCC_APB2PeriphClockCmd,
			.rccperiph = RCC_APB2Periph_USART1,
		},
		.gpios = {
			.rxport = GPIOA,
			.rxpin = GPIO_Pin_10,
			.rx_clock_cmd = RCC_AHBPeriphClockCmd,
			.rx_pin_clock = RCC_AHBPeriph_GPIOA,
			.rx_pinsource = GPIO_PinSource10,
			.txport = GPIOA,
			.txpin = GPIO_Pin_9,
			.tx_clock_cmd = RCC_AHBPeriphClockCmd,
			.tx_pin_clock = RCC_AHBPeriph_GPIOA,
			.tx_pinsource = GPIO_PinSource9,
		},
		.irq_no = USART1_IRQn,
		.rx_overflow = false,
	},
#endif
#ifdef USART2_ENABLE
	{
		.periph = USART2,
		.autocrlf = false,
		.initialized = false,
		.buffers = {
			.tx = &uart2_tx_queue,
			.rx = &uart2_rx_queue,
		},
		.rcc = {
			.periph_clock_cmd = RCC_APB1PeriphClockCmd,
			.rccperiph = RCC_APB1Periph_USART2,
		},
		.gpios = {
			.rxport = GPIOA,
			.rxpin = GPIO_Pin_3,
			.rx_clock_cmd = RCC_AHBPeriphClockCmd,
			.rx_pin_clock = RCC_AHBPeriph_GPIOA,
			.rx_pinsource = GPIO_PinSource3,
			.txport = GPIOA,
			.txpin = GPIO_Pin_2,
			.tx_clock_cmd = RCC_AHBPeriphClockCmd,
			.tx_pin_clock = RCC_AHBPeriph_GPIOA,
			.tx_pinsource = GPIO_PinSource2,
		},
		.irq_no = USART2_IRQn,
		.rx_overflow = false,
	},
#endif
};



static struct usart *usart_lookup(uint8_t usart_no)
{

#ifdef USART1_ENABLE
	if (USART_1 == usart_no) {
		return &usarts[0];
	}
#endif

#ifdef USART2_ENABLE
	if (USART_2 == usart_no) {
		return &usarts[1];
	}
#endif

	return NULL;
}

int usart_init(int usart_no, uint32_t baud)
{
	USART_InitTypeDef uart_conf;
	GPIO_InitTypeDef gpio_conf;
	NVIC_InitTypeDef nvic_init;
	struct usart *u = usart_lookup(usart_no);

	if (!u) {
		return -ENODEV;
	}

	u->gpios.rx_clock_cmd(u->gpios.rx_pin_clock, ENABLE);
	u->gpios.tx_clock_cmd(u->gpios.tx_pin_clock, ENABLE);
	u->rcc.periph_clock_cmd(u->rcc.rccperiph, ENABLE);

	/* Enable the appropriate NVIC Channels */
	nvic_init.NVIC_IRQChannel = u->irq_no;
	nvic_init.NVIC_IRQChannelPriority = 5;
	nvic_init.NVIC_IRQChannelCmd = ENABLE;

	NVIC_Init(&nvic_init);

	/* Set up the GPIOs */
	gpio_conf.GPIO_Speed = GPIO_Speed_50MHz;
	gpio_conf.GPIO_PuPd = GPIO_PuPd_UP;

	/*Tx*/
	gpio_conf.GPIO_Pin = u->gpios.txpin;
	gpio_conf.GPIO_Mode = GPIO_Mode_AF;
	gpio_conf.GPIO_OType = GPIO_OType_PP ;
	GPIO_Init(u->gpios.txport, &gpio_conf);
	GPIO_SetBits(u->gpios.txport, u->gpios.txpin);

	/*Rx*/
	gpio_conf.GPIO_Pin = u->gpios.rxpin;
	gpio_conf.GPIO_Mode = GPIO_Mode_AF;
	gpio_conf.GPIO_OType = GPIO_OType_PP ;
	GPIO_Init(u->gpios.rxport, &gpio_conf);
	GPIO_SetBits(u->gpios.rxport, u->gpios.txpin);

	/* Configure GPIOS as AF */
	GPIO_PinAFConfig(u->gpios.txport, u->gpios.tx_pinsource, GPIO_AF_1);
	GPIO_PinAFConfig(u->gpios.rxport, u->gpios.rx_pinsource, GPIO_AF_1);

	uart_conf.USART_BaudRate = baud;
	uart_conf.USART_WordLength = USART_WordLength_8b;
	uart_conf.USART_StopBits = USART_StopBits_1;
	uart_conf.USART_Parity = USART_Parity_No;
	uart_conf.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	uart_conf.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

	USART_Init(u->periph, &uart_conf);

	USART_Cmd(u->periph, ENABLE);

	USART_ITConfig(u->periph, USART_IT_RXNE, ENABLE);
	u->initialized = true;

	return 0;
}

int usart_enable_autocrlf(int usart_no, bool enabled)
{
	struct usart *u = usart_lookup(usart_no);

	if (!u) {
		return -ENODEV;
	}

	u->autocrlf = enabled;

	return 0;
}

int usart_write(int usart_no, char *s, int len)
{
	int q_ret;
	int ret = 0;
	static char linefeed = '\r';

	struct usart *u = usart_lookup(usart_no);

	if (!u) {
		return -ENODEV;
	}

	if(!u->initialized) {
		return -ENOTCONN;
	}

	while (len--) {
		if ('\n' == *s && u->autocrlf) {
			q_ret = queue_enqueue(u->buffers.tx, &linefeed, -1);
			q_ret &= queue_enqueue(u->buffers.tx, s, -1);
		} else {
			q_ret = queue_enqueue(u->buffers.tx, s, -1);
		}

		s++;
		ret++;

		if (0 != q_ret) {
			return -EIO;
		}

		USART_ITConfig(u->periph, USART_IT_TXE, ENABLE);
	}

	return ret;
}

int usart_putchar(int usart_no, char c)
{
	return usart_write(usart_no, &c, 1);
}

int usart_read(int usart_no, char *d, int len, int timeout)
{
	int ret = 0;
	struct usart *u = usart_lookup(usart_no);

	if (!u)
		return -ENODEV;

	if (!u->initialized) {
		return -ENOTCONN;
	}

	while (len--) {
		if (queue_dequeue(u->buffers.rx, d++, timeout) != 0) {
			return ret;
		}
		ret++;
	}

	return ret;
}

int usart_getchar(int usart_no, char *c, int timeout)
{
	return usart_read(usart_no, c, 1, timeout);
}

static void usart_common_irq(struct usart *u)
{
	int q_ret;
	char c;

	if (USART_GetITStatus(u->periph, USART_IT_TXE) != RESET) {
		if (queue_dequeue(u->buffers.tx, &c, 0) != 0) {
			USART_ITConfig(u->periph, USART_IT_TXE, DISABLE);
		} else {
			USART_SendData(u->periph, (uint16_t)c);
		}
	}

	if (USART_GetITStatus(u->periph, USART_IT_RXNE) != RESET) {
		c = (char)USART_ReceiveData(u->periph);
		q_ret = queue_enqueue(u->buffers.rx, &c, 0);
		if (0 != q_ret) {
			u->rx_overflow = true;
		}
	}
}

#ifdef USART1_ENABLE
void USART1_IRQHandler(void)
{
	usart_common_irq(&usarts[0]);
}
#endif

#ifdef USART2_ENABLE
void USART2_IRQHandler(void)
{
	usart_common_irq(&usarts[1]);
}
#endif
