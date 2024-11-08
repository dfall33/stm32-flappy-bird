#include "stm32f0xx.h"
#include <string.h> // for memmove()
#include <stdlib.h> // for srandom() and random()
#include <stdio.h>
#include "utils.h"

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

uint16_t msg[8] = {0x0000, 0x0100, 0x0200, 0x0300, 0x0400, 0x0500, 0x0600, 0x0700};

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

void printfloat(float f)
{
    char buf[10];
    snprintf(buf, 10, "%f", f);
    for (int i = 1; i < 10; i++)
    {
        if (buf[i] == '.')
        {
            // Combine the decimal point into the previous digit.
            buf[i - 1] |= 0x80;
            memcpy(&buf[i], &buf[i + 1], 10 - i - 1);
        }
    }
    print(buf);
}

void append_segments(char val)
{
    for (int i = 0; i < 7; i++)
    {
        set_digit_segments(i, msg[i + 1] & 0xff);
    }
    set_digit_segments(7, val);
}

void clear_display(void)
{
    for (int i = 0; i < 8; i++)
    {
        msg[i] = msg[i] & 0xff00;
    }
}

// 16 history bytes.  Each byte represents the last 8 samples of a button.
uint8_t hist[16];
char queue[2]; // A two-entry queue of button press/release events.
int qin;       // Which queue entry is next for input
int qout;      // Which queue entry is next for output

const char keymap[] = "DCBA#9630852*741";

void push_queue(int n)
{
    queue[qin] = n;
    qin ^= 1;
}

char pop_queue()
{
    char tmp = queue[qout];
    queue[qout] = 0;
    qout ^= 1;
    return tmp;
}

void update_history(int c, int rows)
{
    // We used to make students do this in assembly language.
    for (int i = 0; i < 4; i++)
    {
        hist[4 * c + i] = (hist[4 * c + i] << 1) + ((rows >> i) & 1);
        if (hist[4 * c + i] == 0x01)
            push_queue(0x80 | keymap[4 * c + i]);
        if (hist[4 * c + i] == 0xfe)
            push_queue(keymap[4 * c + i]);
    }
}

void drive_column(int c)
{
    GPIOC->BSRR = 0xf00000 | ~(1 << (c + 4));
}

int read_rows()
{
    return (~GPIOC->IDR) & 0xf;
}

char get_key_event(void)
{
    for (;;)
    {
        asm volatile("wfi"); // wait for an interrupt
        if (queue[qout] != 0)
            break;
    }
    return pop_queue();
}

char get_keypress()
{
    char event;
    for (;;)
    {
        // Wait for every button event...
        event = get_key_event();
        // ...but ignore if it's a release.
        if (event & 0x80)
            break;
    }
    return event & 0x7f;
}

void show_keys(void)
{
    char buf[] = "        ";
    for (;;)
    {
        char event = get_key_event();
        memmove(buf, &buf[1], 7);
        buf[7] = event;
        print(buf);
    }
}

// Turn on the dot of the rightmost display element.
void dot()
{
    msg[7] |= 0x80;
}

//===========================================================================
// Initialize the SPI2 peripheral.
//===========================================================================
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

//===========================================================================
// Configure the SPI2 peripheral to trigger the DMA channel when the
// transmitter is empty.  Use the code from setup_dma from lab 5.
//===========================================================================
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

//===========================================================================
// Enable the DMA channel.
//===========================================================================
void spi2_enable_dma(void)
{
    // Enable the channel.
    DMA1_Channel5->CCR |= DMA_CCR_EN;
}

extern uint16_t display[34];

int score = 0;
void init_tim17(void);
void init_spi2(void);
void spi2_setup_dma(void);
void spi2_enable_dma(void);

void game(void)
{
    print("Score99");
    init_spi2();
    spi2_setup_dma();
    spi2_enable_dma();
    return;
    // init_tim17(); // start timer
    // get_keypress(); // Wait for key to start

    // Use the timer counter as random seed...
    srandom(TIM17->CNT);

    // Then enable interrupt...
    NVIC->ISER[0] = 1 << TIM17_IRQn;

    int score = 0;
    char score_str[10]; // Buffer to hold score as string

    for (;;)
    {
        char key = get_keypress();
        if (key == 'A' || key == 'B')
        {
            // If the A or B key is pressed, disable interrupts while we update the display.
            asm("cpsid i");
            score++;
            snprintf(score_str, sizeof(score_str), "%d", score); // Convert score to string
            print(score_str);                                    // Now passing a string
            asm("cpsie i");
        }
    }
}