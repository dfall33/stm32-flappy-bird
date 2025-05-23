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
#include "score_display.h"

/* Create a pointer to a picture object with an internal pix2 array holding pixel data */
#define TempPicturePtr(name, width, height) Picture name[(width) * (height) / 6 + 2] = {{width, height, 2}}

extern const Picture background; // load the background from background.c
extern const Picture bird;       // load the bird from bird.c
extern const Picture barrier;    // load the barrier from barrier.c

#define BIRD_WIDTH 19                // bird width
#define BIRD_HEIGHT 19               // bird height
#define PADDING 5                    // padding
#define GAP_WIDTH 80                 // gap width
#define GAP_RANGE 140                // gap range
#define BARRIER_WIDTH 220            // barrier width
#define BARRIER_HEIGHT 20            // barrier height
#define GAP_HEIGHT 20                // gap height
#define BIRD_V0 0                    // bird initial velocity
#define BARRIER_V0 2                 // barrier initial velocity
#define BIRD_X0 100                  // bird initial x position
#define BIRD_Y0 140                  // bird initial y position
#define BARRIER_Y0 (320 - (30 / 2))  // barrier initial y position
#define BARRIER_X0 (240 - (230 / 2)) // barrier initial x position
#define BARRIER_Y_RESET 100          // barrier min y position before new barrier
#define BIRD_MIN_X 50                // bird min x position
#define BIRD_MAX_X 200               // bird max x position

// boolean values so we don't have to include stdbool.h
#define FALSE 0
#define TRUE 1

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
int bird_a = -3;

int game_over = FALSE;
int y_gap = GAP_RANGE << 2;
int score = 0;
int in_barrier = FALSE;
uint32_t high_score = 0;

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

    pic_overlay(bird_ptr, PADDING, PADDING, &bird, 0xffff);

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

    pic_overlay(barrier_ptr, PADDING, PADDING, &barrier, 0xffff);

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
 * @brief Initialize push button inputs.
 * @return void
 */
void init_input()
{
    // Enable clock to GPIOA
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;

    // Set PA0 as input
    GPIOA->MODER &= ~GPIO_MODER_MODER0;

    // Set PA0 as pull-down
    GPIOA->PUPDR &= ~GPIO_PUPDR_PUPDR0;
    GPIOA->PUPDR |= GPIO_PUPDR_PUPDR0_1;

    // enable the clock to GPIOB
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;

    // Set PB2 as input
    GPIOB->MODER &= ~GPIO_MODER_MODER2;

    // Set PB2 as pull-down
    GPIOB->PUPDR &= ~GPIO_PUPDR_PUPDR2;
    GPIOB->PUPDR |= GPIO_PUPDR_PUPDR2_1;
}

/**
 * @brief Initialize timer 2 and enable it to generate interrupts.
 * @return void
 */
void init_tim16()
{
    // Enable clock to TIM2
    // RCC->APB1ENR |= RCC_APB1ENR_TIM1;
    RCC->APB2ENR |= RCC_APB2ENR_TIM16EN;

    // Set prescaler value for interrupt every half second (interval can be adjusted)
    TIM16->PSC = 3000 - 1;

    // Set refresh value for interrupt every half second
    TIM16->ARR = 2000 - 1;

    // Enable the update interrupt
    TIM16->DIER |= TIM_DIER_UIE;

    // Enable Timer
    TIM16->CR1 |= TIM_CR1_CEN;

    // Enable Interrupt
    NVIC_EnableIRQ(TIM16_IRQn);
}

void TIM16_IRQHandler()
{
    // acknowledge the interrupt
    TIM16->SR &= ~TIM_SR_UIF;

    bird_v += bird_a;

    // max velocity is padding (anything more than padding ruins graphics)
    if (bird_v < -1 * PADDING)
    {
        bird_v = -1 * PADDING;
    }
    // Check if button pushed
    if (GPIOA->IDR & 1)
    {

        // Button is pressed, give velocity boost
        bird_v = 5;
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

    // DO NOT ENABLE THE INTERRUPT here, because we don't want game graphics updating until game is started
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

    // check if bird hit ground
    if (bird_x < BIRD_MIN_X)
    {
        game_over = TRUE;
        return;
    }

    // check if bird is crossing a barrier
    if (bird_y > barrier_y - (BARRIER_HEIGHT >> 1) && bird_y < barrier_y + (BARRIER_HEIGHT >> 1))
    {
        in_barrier = TRUE;

        // bird is in barrier, check if not in gap (crashing)
        if ((bird_x < y_gap || bird_x > y_gap + GAP_WIDTH)) // bird outside of gap (either below or above)
        {
            game_over = TRUE;
            return;
        }

        // bird passed barrier, get one point
    }
    else
    {

        if (in_barrier)
            score++;

        in_barrier = FALSE;
    }

    // bird can't go above the ceiling
    if (bird_x > BIRD_MAX_X)
    {
        bird_x = BIRD_MAX_X;
    }

    // update the bird's position
    update_bird_pos(bird_x, bird_y);

    /* Barrier physics */
    barrier_y -= barrier_v; // update barrier position based on velocity

    /* If barrier at end of the screen, erase it and make a new barrier with a new gap location */
    if (barrier_y < BARRIER_Y_RESET)
    {
        // reset the screen
        LCD_DrawPicture(0, 0, &background);
        barrier_y = BARRIER_Y0;
        // get new gap
        y_gap = rand() % GAP_RANGE;

        // make new barrier
        create_barrier(y_gap);
    }

    /* If barrier not at end of screen, just keep moving it */
    else
    {
        update_barrier_pos(BARRIER_X0, barrier_y);
    }

    char buf[9];
    snprintf(buf, 9, "Score% 3d", score);
    print(buf);
}

void wait_for_start()
{
    // wait for button press
    while (1)
    {
        if (GPIOA->IDR & 1)
        {
            break;
        }
    }
}

void reset_params()
{
    barrier_x = BARRIER_X0;
    barrier_y = BARRIER_Y0;
    bird_x = BIRD_X0;
    bird_y = BIRD_Y0;
    barrier_v = BARRIER_V0;
    bird_v = BIRD_V0;
    bird_a = -3;

    game_over = FALSE;
    y_gap = 100;
    score = 0;

    in_barrier = FALSE;
}

void play()
{
    // print("PressPB2");
    init_spi2();
    spi2_setup_dma();
    spi2_enable_dma();
    init_tim17();

    // play game forever
    for (;;)
    {

        // reset the screen
        LCD_DrawPicture(0, 0, &background); // redraw background
        init_bird();                        // reset bird position
        create_barrier(70);                 // first barrier needs to be created outside of the interrupt handler

        // get and display high score
        eeprom_get_high_score(&high_score);
        char buf[9];
        snprintf(buf, 9, "High% 3d", (int)high_score);
        print(buf);

        wait_for_start(); // wait for user to press PB2 to start

        reset_params(); // reset all parameters

        NVIC_EnableIRQ(TIM17_IRQn); // enable tim17 interrupt to allow graphics to start updating

        // keep playing until game over
        for (;;)
        {
            // check if game over
            if (game_over)
            {

                // stop updating display: disable tim17 interrupt
                NVIC_DisableIRQ(TIM17_IRQn);

                // update high score
                if (score > high_score)
                {
                    eeprom_save_high_score(score);
                }
                break;
            }
        }
    }
}

int main(void)
{

    internal_clock(); // HSI to 48MHz
    eeprom_init();    // initialize eeprom
    init_tim17();     // setup screen refresh
    init_input();     // enable user input via PA0, PB2
    init_tim16();     // bird velocity update and user input
    LCD_Setup();      // enable TFT display
    play();           // play the game forever
    return 0;
}