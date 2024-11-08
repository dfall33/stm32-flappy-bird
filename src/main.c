/**
 * @file main.c
 * @authors David Fall, Jennifer Ferguson, Luke Lenz, Eduard Tanase
 * @date November 2024
 * @brief ECE 36200 Group Project, Fall 2024
 */

#include "stdint.h"
#include "stm32f0xx.h"

#include "lcd.h"
#include "utils.h"
#include "eeprom.h"

void internal_clock();
// void nano_wait(unsigned int t);

/* Create a pointer to a picture object with an internal pix2 array holding pixel data */
#define TempPicturePtr(name, width, height) Picture name[(width) * (height) / 6 + 2] = {{width, height, 2}}

extern const Picture background; // load the background from background.c
extern const Picture bird;       // load the bird from bird.c
extern const Picture barrier;    // load the barrier from barrier.c

#define BIRD_WIDTH 19                // bird width
#define BIRD_HEIGHT 19               // bird height
#define PADDING 5                    // padding
#define GAP_WIDTH 50                 // gap width
#define BARRIER_WIDTH 220            // barrier width
#define BARRIER_HEIGHT 20            // barrier height
#define GAP_HEIGHT 20                // gap height
#define BIRD_V0 3                    // bird initial velocity
#define BARRIER_V0 2                 // barrier initial velocity
#define BIRD_X0 100                  // bird initial x position
#define BIRD_Y0 140                  // bird initial y position
#define BARRIER_Y0 (320 - (30 / 2))  // barrier initial y position
#define BARRIER_X0 (240 - (230 / 2)) // barrier initial x position
#define BARRIER_Y_RESET 100          // barrier min y position before new barrier
#define BIRD_MIN_X 50                // bird min x position
#define BIRD_MAX_X 150               // bird max x position

/* Get picture pointers for bird and barrier, include padding for translation */
TempPicturePtr(bird_ptr, BIRD_WIDTH + 2 * PADDING, BIRD_HEIGHT + 2 * PADDING);
TempPicturePtr(barrier_ptr, BARRIER_WIDTH + 2 * PADDING, BARRIER_HEIGHT + 2 * PADDING);

/* Define initial values */
int barrier_x = BARRIER_X0;
int barrier_y = BARRIER_Y0;
int bird_x = BIRD_X0;
int bird_y = BIRD_Y0;
int barrier_v = BARRIER_V0;
int bird_v = BIRD_V0;
int bird_a = 0;

/**
 * @brief Initialize the SPI1 peripheral to run at 12MHz.
 * @return void
 */
void init_spi_slow()
{
    // configure SPI1
    // pb3 sck, pb4 miso, pb5 mosi
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    GPIOB->MODER &= ~(GPIO_MODER_MODER3 | GPIO_MODER_MODER4 | GPIO_MODER_MODER5);
    GPIOB->MODER |= GPIO_MODER_MODER3_1 | GPIO_MODER_MODER4_1 | GPIO_MODER_MODER5_1;
    GPIOB->AFR[0] &= ~(GPIO_AFRL_AFRL3 | GPIO_AFRL_AFRL4 | GPIO_AFRL_AFRL5);

    // enable the clock to SPI1
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    // disable SPI1
    SPI1->CR1 &= ~SPI_CR1_SPE;

    // set the baud rate divisor to the max possible
    SPI1->CR1 |= SPI_CR1_BR;

    // master mode
    SPI1->CR1 |= SPI_CR1_MSTR;

    // 8 bit data size
    SPI1->CR2 |= SPI_CR2_DS_0 | SPI_CR2_DS_1 | SPI_CR2_DS_2 | SPI_CR2_DS_3;
    SPI1->CR2 &= ~SPI_CR2_DS_3;

    // software slave management and internal slave select
    SPI1->CR1 |= SPI_CR1_SSM | SPI_CR1_SSI;

    // set fifo reception bit
    SPI1->CR2 |= SPI_CR2_FRXTH;

    // enable SPI1
    SPI1->CR1 |= SPI_CR1_SPE;
}

/**
 * @brief Enable the SD card in the TFT display.
 * @return void
 */
void enable_sdcard()
{
    GPIOB->ODR &= ~GPIO_ODR_2;
}

/**
 * @brief Disable the SD card in the TFT display.
 * @return void
 */
void disable_sdcard()
{
    GPIOB->ODR |= GPIO_ODR_2;
}

/**
 * @brief Initialize the SD card I/O.
 * @return void
 */
void init_sdcard_io()
{
    // call init_spi_slow()
    init_spi_slow();

    // configure pb2 as output
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    GPIOB->MODER &= ~GPIO_MODER_MODER2;
    GPIOB->MODER |= GPIO_MODER_MODER2_0;

    // call disable_sdcard()
    disable_sdcard();
}

/**
 * @brief Set the SPI1 peripheral to run at 48MHz.
 * @return void
 */
void sdcard_io_high_speed()
{
    // disable SPI1
    SPI1->CR1 &= ~SPI_CR1_SPE;

    // set the baud rate register to achieve 12MHz (divide by 4)
    SPI1->CR1 &= ~SPI_CR1_BR;
    SPI1->CR1 |= SPI_CR1_BR_0;

    // enable SPI1
    SPI1->CR1 |= SPI_CR1_SPE;
}

// initialize the LCD
/**
 * @brief Initialize the LCD SPI.
 * @note  This function overrides a weak definition in lcd.c, so it is not called directly in main.c but is still called indirectly.
 * @return void
 */
void init_lcd_spi()
{

    // configure pb8, 11, 14 as ouptut
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    GPIOB->MODER &= ~(GPIO_MODER_MODER8 | GPIO_MODER_MODER11 | GPIO_MODER_MODER14);
    GPIOB->MODER |= GPIO_MODER_MODER8_0 | GPIO_MODER_MODER11_0 | GPIO_MODER_MODER14_0;

    // call init_spi_slow()
    init_spi_slow();

    // call sdcard_io_high_speed()
    sdcard_io_high_speed();
}

/**
 * @brief Copy a subset of a large source picture into a smaller destination.
 * @param dst The destination picture.
 * @param src The source picture.
 * @param sx The x offset into the source picture.
 * @param sy The y offset into the source picture.
 * @return void
 */
void pic_subset(Picture *dst, const Picture *src, int sx, int sy)
{
    int dw = dst->width;
    int dh = dst->height;
    for (int y = 0; y < dh; y++)
    {
        if (y + sy < 0)
            continue;
        if (y + sy >= src->height)
            break;
        for (int x = 0; x < dw; x++)
        {
            if (x + sx < 0)
                continue;
            if (x + sx >= src->width)
                break;
            dst->pix2[dw * y + x] = src->pix2[src->width * (y + sy) + x + sx];
        }
    }
}

/**
 * @brief Overlay a picture onto a destination picture.
 * @param dst The destination picture.
 * @param xoffset The x offset into the destination picture.
 * @param yoffset The y offset into the destination picture.
 * @param src The source picture.
 * @param transparent The color in the source picture that will not be copied.
 * @return void
 */
void pic_overlay(Picture *dst, int xoffset, int yoffset, const Picture *src, int transparent)
{
    for (int y = 0; y < src->height; y++)
    {
        int dy = y + yoffset;
        if (dy < 0)
            continue;
        if (dy >= dst->height)
            break;
        for (int x = 0; x < src->width; x++)
        {
            int dx = x + xoffset;
            if (dx < 0)
                continue;
            if (dx >= dst->width)
                break;
            unsigned short int p = src->pix2[y * src->width + x];
            if (p != transparent)
                dst->pix2[dy * dst->width + dx] = p;
        }
    }
}

/**
 * @brief Update the bird object position.
 * @param x The x position of new center of the bird.
 * @param y The y position of new center of the bird.
 * @return void
 */
void update_bird_pos(int x, int y)
{
    TempPicturePtr(tmp, 29, 29);                                           // Create a temporary 29x29 image.
    pic_subset(tmp, &background, x - tmp->width / 2, y - tmp->height / 2); // Copy the background
    pic_overlay(tmp, 0, 0, bird_ptr, 0xffff);                              // Overlay the object
    LCD_DrawPicture(x - tmp->width / 2, y - tmp->height / 2, tmp);         // Re-draw it to the screen
}

/**
 * @brief Initialize the bird object.
 * @return void
 */
void init_bird()
{
    for (int i = 0; i < 29 * 29; i++)
        bird_ptr->pix2[i] = 0xffff;

    pic_overlay(bird_ptr, 5, 5, &bird, 0xffff);

    update_bird_pos(BIRD_X0, BIRD_Y0);
}

/**
 * @brief Create a barrier object with a gap at y_gap.
 * @param y_gap The y position of the gap.
 * @return void
 */
void create_barrier(int y_gap)
{

    int total_pixels = (BARRIER_WIDTH + 2 * PADDING) * (BARRIER_HEIGHT + 2 * PADDING);
    for (int i = 0; i < total_pixels; i++)
        barrier_ptr->pix2[i] = 0xffff;

    pic_overlay(barrier_ptr, 5, 5, &barrier, 0xffff);

    // overwrite the barrier obj for a gap
    for (int i = 0; i < total_pixels; i++)
    {
        int mod = i % (BARRIER_WIDTH + 2 * PADDING);
        if (mod > y_gap && mod < y_gap + GAP_WIDTH)
        {
            barrier_ptr->pix2[i] = 0xffff;
        }
    }
}

/**
 * @brief Update the barrier object position.
 * @param x The x position of the center of the barrier.
 * @param y The y position of the center of the barrier.
 * @return void
 */
void update_barrier_pos(int x, int y)
{
    TempPicturePtr(tmp, 230, 30);                                          // Create a temporary 30x110 image.
    pic_subset(tmp, &background, x - tmp->width / 2, y - tmp->height / 2); // Copy the background
    pic_overlay(tmp, 0, 0, barrier_ptr, 0xffff);                           // Overlay the object
    LCD_DrawPicture(x - tmp->width / 2, y - tmp->height / 2, tmp);         // Re-draw it to the screen
}

/**
 * @brief Initialize timer 2 and enable it to generate interrupts.
 * @return void
 */
void init_tim2()
{
    // Enable clock to TIM2
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    // Set prescaler value for interrupt every half second (interval can be adjusted)
    TIM2->PSC = 3000 - 1;

    // Set refresh value for interrupt every half second
    TIM2->ARR = 8000 - 1;

    // Enable the update interrupt
    TIM2->DIER |= TIM_DIER_UIE;

    // Enable Timer
    TIM2->CR1 |= TIM_CR1_CEN;

    // Enable Interrupt
    NVIC_EnableIRQ(TIM2_IRQn);
}

void TIM2_IRQHandler()
{
    // acknowledge the interrupt
    TIM2->SR &= ~TIM_SR_UIF;

    // Check if button pushed
    if (GPIOA->IDR & (1 << 0)) // PA0
    {
        // Yes button is pushed, set acceleration high (arbitrary value, can be changed)
        bird_a = 3;
    }

    else
    {
        // No button is not pushed, set acceleration low (arbitrary value, can be changed)
        bird_a = -3;
    }

    // Update velocity based on bird acceleration
    bird_v += bird_a;

    // Set velocity bounds (not sure if this is neccessary, or if values are appropriate)
    if (bird_v > 18)
    {
        bird_v = 18;
    }
    else if (bird_v < -18)
    {
        bird_v = -18;
    }
}

/**
 * @brief Initialize timer 17 and enable it to generate interrupts.
 * @return void
 */
void init_tim17()
{
    // enable the clock to TIM17
    RCC->APB2ENR |= RCC_APB2ENR_TIM17EN;

    // set the prescaler to 48
    TIM17->PSC = 48;

    // set the auto-reload register to 1000
    TIM17->ARR = 1000;

    // enable the update interrupt
    TIM17->DIER |= TIM_DIER_UIE;

    // enable the timer
    TIM17->CR1 |= TIM_CR1_CEN;

    // enable the interrupt in the NVIC
    NVIC_EnableIRQ(TIM17_IRQn);
}

/**
 * @brief Timer 17 interrupt handler. Used to update position of bird, barriers, control the flow of the game.
 * @return void
 */
void TIM17_IRQHandler()
{
    // acknowledge the interrupt
    TIM17->SR &= ~TIM_SR_UIF;

    // move the bird
    bird_x += bird_v;

    if (bird_x < BIRD_MIN_X)
    {
        // Bird crashes, game end
        // PLACE HOLDER
    }

    if (bird_x > BIRD_MAX_X)
    {
        bird_x = BIRD_MAX_X;
    }

    update_bird_pos(bird_x, bird_y);
    /* End placeholder bird physics */

    /* Barrier physics */
    barrier_y -= barrier_v; // update barrier position based on velocity

    /* If barrier at end of the screen, erase it and make a new barrier with a new gap location */
    if (barrier_y < BARRIER_Y_RESET)
    {
        // reset the screen
        LCD_DrawPicture(0, 0, &background);
        barrier_y = BARRIER_Y0;
        // get new gap
        int y_gap = rand() % 80 + 100;

        // make new barrier
        create_barrier(y_gap);
    }

    /* If barrier not at end of screen, just keep moving it */
    else
    {
        update_barrier_pos(BARRIER_X0, barrier_y);
    }
}
void init_spi2(void);
void spi2_setup_dma(void);
void spi2_enable_dma(void);
void game(void);
int main(void)
{

    internal_clock();
    eeprom_init();
    LCD_Setup();
    LCD_DrawPicture(0, 0, &background);
    init_bird();
    init_tim17();
    init_spi2();
    spi2_setup_dma();
    spi2_enable_dma();

    game();
    while (1)
        asm("wfi");

    return 0;
}