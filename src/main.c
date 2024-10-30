/**
 * @file main.c
 * @authors David Fall, Jennifer Ferguson, Luke Lenz, Eduard Tanase
 * @date November 2024
 * @brief ECE 36200 Group Project, Fall 2024
 */

#include "stdint.h"
#include "stm32f0xx.h"

/* ----- Constants ----- */
#define TFT_ON 0x28
#define TFT_OFF 0x29

void internal_clock();
void nano_wait(unsigned int t);

void set_dc(uint8_t state)
{
    if (state)
    {
        GPIOA->ODR |= GPIO_ODR_3;
    }
    else
    {
        GPIOA->ODR &= ~GPIO_ODR_3;
    }
}

/**
 * @brief Set up the GPIO ports for the TFT display
 */
void setup_ports()
{
    // enable clock to GPIOA peripheral
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;

    // set PA4-PA7 to AF mode for SPI1
    GPIOA->MODER &= ~(GPIO_MODER_MODER4 | GPIO_MODER_MODER5 | GPIO_MODER_MODER6 | GPIO_MODER_MODER7);
    GPIOA->MODER |= (GPIO_MODER_MODER4_1 | GPIO_MODER_MODER5_1 | GPIO_MODER_MODER6_1 | GPIO_MODER_MODER7_1);

    // set PA4-PA7 to AF0 for SPI1
    GPIOA->AFR[0] &= ~(GPIO_AFRL_AFRL4 | GPIO_AFRL_AFRL5 | GPIO_AFRL_AFRL6 | GPIO_AFRL_AFRL7);

    // set PA3 as output to manage DC pin
    GPIOA->MODER &= ~GPIO_MODER_MODER3;
    GPIOA->MODER |= GPIO_MODER_MODER3_0;

    // set PA2 as output to manage RESET pin
    GPIOA->MODER &= ~GPIO_MODER_MODER2;
    GPIOA->MODER |= GPIO_MODER_MODER2_0;
}

/**
 * @brief Set up SPI1 peripheral to write data, send commands to TFT display
 */
void init_spi1()
{
    // enable the clock to SPI1 peripheral
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    // disable SPI1 peripheral during configuration
    SPI1->CR1 &= ~SPI_CR1_SPE;

    // set the baud rate
    SPI1->CR1 |= SPI_CR1_BR;

    // master mode
    SPI1->CR1 |= SPI_CR1_MSTR;

    // set SSOE and NSSP bits
    SPI1->CR2 |= SPI_CR2_SSOE | SPI_CR2_NSSP;

    // enable SPI1 peripheral
    SPI1->CR1 |= SPI_CR1_SPE;
}

/**
 * @brief Send a command to the TFT display
 * @param command The command to send
 */
void spi_tft_send_command(uint8_t command)
{

    // set DC pin to low
    set_dc(0);

    // wait until the transmit buffer is empty
    while (!(SPI1->SR & SPI_SR_TXE))
        ;

    // send the command
    SPI1->DR = command;
}

/**
 * @brief Send data to the TFT display
 * @param data The data to send
 */
void spi_tft_send_data(uint8_t data)
{

    // set DC pin to high
    set_dc(1);

    // wait until the transmit buffer is empty
    while (!(SPI1->SR & SPI_SR_TXE))
        ;

    // send the data
    SPI1->DR = data;
}

void tft_hardware_reset()
{
    // set reset pin to low
    GPIOA->ODR &= ~GPIO_ODR_2;

    // wait 1ms
    nano_wait(1000000);

    // set reset pin to high
    GPIOA->ODR |= GPIO_ODR_2;

    // wait 150ms
    nano_wait(150000000);
}

/**
 * @brief Initialize the TFT display
 */
void init_tft()
{

    // hardware reset
    tft_hardware_reset();

    // wait 1ms
    nano_wait(1000000);

    // send software reset command
    spi_tft_send_command(0x01);

    // wait 150ms
    nano_wait(150000000);

    // display off
    spi_tft_send_command(0x28);

    // set frame rate
    spi_tft_send_command(0xB3);

    // display on
    spi_tft_send_command(0x29);
}

void fill_screen(uint16_t color)
{
    // set column address
    spi_tft_send_command(0x2A);
    spi_tft_send_data(0x00);
    spi_tft_send_data(0x00);

    // set page address
    spi_tft_send_command(0x2B);
    spi_tft_send_data(0x00);
    spi_tft_send_data(0x00);

    // write to RAM
    spi_tft_send_command(0x2C);

    for (int i = 0; i < 240 * 320; i++)
    {
        spi_tft_send_data(color >> 8);
        spi_tft_send_data(color & 0xFF);
    }
}

int main()
{
    internal_clock();

    setup_ports();
    init_spi1();
    init_tft();

    // make screen white
    fill_screen(0xFFFF);

    return 0;
}