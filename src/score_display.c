#include "stm32f0xx.h"
#include <string.h> // for memmove()
#include <stdlib.h> // for srandom() and random()
#include <stdio.h>
#include "utils.h"
#include "lcd.h"

/* 8 byte message array for DMA transfer to the 8 segment displays */
uint16_t msg[8] = {0x0000, 0x0100, 0x0200, 0x0300, 0x0400, 0x0500, 0x0600, 0x0700};

/* 8x8 font for the 8 segment displays */
const char font[] = {
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x00, // 32: space
    0x86, // 33: exclamation
    0x22, // 34: double quote
    0x76, // 35: octothorpe
    0x00, // dollar
    0x00, // percent
    0x00, // ampersand
    0x20, // 39: single quote
    0x39, // 40: open paren
    0x0f, // 41: close paren
    0x49, // 42: asterisk
    0x00, // plus
    0x10, // 44: comma
    0x40, // 45: minus
    0x80, // 46: period
    0x00, // slash
    // digits
    0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x67,
    // seven unknown
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    // Uppercase
    0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71, 0x6f, 0x76, 0x30, 0x1e, 0x00, 0x38, 0x00,
    0x37, 0x3f, 0x73, 0x7b, 0x31, 0x6d, 0x78, 0x3e, 0x00, 0x00, 0x00, 0x6e, 0x00,
    0x39, // 91: open square bracket
    0x00, // backslash
    0x0f, // 93: close square bracket
    0x00, // circumflex
    0x08, // 95: underscore
    0x20, // 96: backquote
    // Lowercase
    0x5f, 0x7c, 0x58, 0x5e, 0x79, 0x71, 0x6f, 0x74, 0x10, 0x0e, 0x00, 0x30, 0x00,
    0x54, 0x5c, 0x73, 0x7b, 0x50, 0x6d, 0x78, 0x1c, 0x00, 0x00, 0x00, 0x6e, 0x00};

/**
 * @brief Print a string to the 8 segment displays.
 * @param str The string to print.
 * @return void
 */
void print(const char str[])
{
    const char *p = str;
    for (int i = 0; i < 8; i++)
    {
        if (*p == '\0')
        {
            msg[i] = (i << 8);
        }
        else
        {
            msg[i] = (i << 8) | font[*p & 0x7f] | (*p & 0x80);
            p++;
        }
    }
}

/**
 * @brief Initialize the SPI2 peripheral to run at 12MHz.
 * @return void
 */
void init_spi2(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    // Set the mode of PB12, PB13, PB15 to alternate function mode for each of the Timer 3 channels.
    GPIOB->MODER &= ~(GPIO_MODER_MODER12 | GPIO_MODER_MODER13 | GPIO_MODER_MODER15);
    GPIOB->MODER |= (GPIO_MODER_MODER12_1 | GPIO_MODER_MODER13_1 | GPIO_MODER_MODER15_1);
    // Some pins can have more than one alternate function associated with them; specifying which alternate function is to be used is the purpose of the GPIOx_AFRL (called AFR[0]) and GPIOx_AFRH (called AFR[1]) registers.
    // PB12
    GPIOB->AFR[1] &= ~(0xf << (4 * (12 - 8))); // clear bits
    GPIOB->AFR[1] |= 0x0 << (4 * (12 - 8));    // set bits to 0 for AF0
    // PB13
    GPIOB->AFR[1] &= ~(0xf << (4 * (13 - 8))); // clear bits
    GPIOB->AFR[1] |= 0x0 << (4 * (13 - 8));    // set bits to 0 for AF0
    // PB15
    GPIOB->AFR[1] &= ~(0xf << (4 * (15 - 8))); // clear bits
    GPIOB->AFR[1] |= 0x0 << (4 * (15 - 8));    // set bits to 0 for AF0
    // Ensure that the CR1_SPE bit is clear first.
    SPI2->CR1 &= ~SPI_CR1_SPE;
    // Set the baud rate as low as possible (maximum divisor for BR).
    SPI2->CR1 |= (SPI_CR1_BR_2 | SPI_CR1_BR_1 | SPI_CR1_BR_0);
    // Configure the interface for a 16-bit word size.
    SPI2->CR2 |= (SPI_CR2_DS_1 | SPI_CR2_DS_2 | SPI_CR2_DS_3);
    // Configure the SPI channel to be in "master configuration".
    SPI2->CR1 |= SPI_CR1_MSTR;
    // Set the SS Output enable bit and enable NSSP.
    SPI2->CR2 |= SPI_CR2_SSOE;
    SPI2->CR2 |= SPI_CR2_NSSP;
    // Set the TXDMAEN bit to enable DMA transfers on transmit buffer empty
    SPI2->CR2 |= SPI_CR2_TXDMAEN;
    // Enable the SPI channel.
    SPI2->CR1 |= SPI_CR1_SPE;
}

/**
 * @brief Setup the DMA channel for SPI2.
 * @return void
 */
void spi2_setup_dma(void)
{
    //============================================================================
    // setup_dma() + enable_dma()
    //===========================================================================
    //   Enables the RCC clock to the DMA controller
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    // Turn off the enable bit for the channel first
    DMA1_Channel5->CCR &= ~DMA_CCR_EN;
    // Set CMAR to the address of the msg array.
    DMA1_Channel5->CMAR = msg;
    // Set CPAR to the address of the GPIOB_ODR register.
    DMA1_Channel5->CPAR = &SPI2->DR;
    // Set CNDTR to 8. (the amount of LEDs.)
    DMA1_Channel5->CNDTR = 8;
    // Set the DIRection for copying from-memory-to-peripheral.
    DMA1_Channel5->CCR |= DMA_CCR_DIR;
    // Set the MINC to increment the CMAR for every transfer. (Each LED will have something different on it.)
    DMA1_Channel5->CCR |= DMA_CCR_MINC;
    // Set the Memory datum SIZE to 16-bit.
    DMA1_Channel5->CCR |= DMA_CCR_MSIZE_0;
    // Set the Peripheral datum SIZE to 16-bit.
    DMA1_Channel5->CCR |= DMA_CCR_PSIZE_0;
    // Set the channel for CIRCular operation.
    DMA1_Channel5->CCR |= DMA_CCR_CIRC;

    SPI2->CR2 |= SPI_CR2_TXDMAEN;
}

/**
 * @brief Enable the DMA channel for SPI2.
 * @return void
 */
void spi2_enable_dma(void)
{
    // Enable the channel.
    DMA1_Channel5->CCR |= DMA_CCR_EN;
}
