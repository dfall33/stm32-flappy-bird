#include "eeprom.h"

// I2C initialization for EEPROM communication
void eeprom_init(void) {
    // Enable GPIOB clock
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    
    // Enable I2C1 clock
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    
    // Configure PB6 (SCL) and PB7 (SDA) for I2C1
    // Set alternate function mode (0b10)
    GPIOB->MODER &= ~(GPIO_MODER_MODER6 | GPIO_MODER_MODER7);
    GPIOB->MODER |= (GPIO_MODER_MODER6_1 | GPIO_MODER_MODER7_1);
    
    // Set to open-drain
    GPIOB->OTYPER |= (GPIO_OTYPER_OT_6 | GPIO_OTYPER_OT_7);
    
    // Set to high speed
    GPIOB->OSPEEDR |= (GPIO_OSPEEDER_OSPEEDR6 | GPIO_OSPEEDER_OSPEEDR7);
    
    // Enable pull-up
    GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPDR6 | GPIO_PUPDR_PUPDR7);
    GPIOB->PUPDR |= (GPIO_PUPDR_PUPDR6_0 | GPIO_PUPDR_PUPDR7_0);
    
    // Set alternate function to AF1 for I2C1
    GPIOB->AFR[0] &= ~(GPIO_AFRL_AFRL6 | GPIO_AFRL_AFRL7);
    GPIOB->AFR[0] |= (1 << (4 * 6)) | (1 << (4 * 7));
    
    // Reset I2C1
    RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;
    
    // Configure I2C1 timing for 100kHz with 48MHz clock
    I2C1->TIMINGR = 0x10420F13;
    
    // Enable I2C1
    I2C1->CR1 |= I2C_CR1_PE;
}

static void i2c_wait_for_idle(void) {
    while ((I2C1->ISR & I2C_ISR_BUSY) == I2C_ISR_BUSY);
}

int8_t eeprom_write_byte(uint16_t addr, uint8_t data) {
    i2c_wait_for_idle();
    
    // Set number of bytes to transmit (3: 2 for address, 1 for data)
    I2C1->CR2 = (3 << I2C_CR2_NBYTES_Pos);
    
    // Set slave address
    I2C1->CR2 &= ~I2C_CR2_SADD;
    I2C1->CR2 |= (EEPROM_ADDR << 1);
    
    // Generate START condition
    I2C1->CR2 |= I2C_CR2_START;
    
    // Send high byte of memory address
    while (!(I2C1->ISR & I2C_ISR_TXIS));
    I2C1->TXDR = (addr >> 8) & 0xFF;
    
    // Send low byte of memory address
    while (!(I2C1->ISR & I2C_ISR_TXIS));
    I2C1->TXDR = addr & 0xFF;
    
    // Send data byte
    while (!(I2C1->ISR & I2C_ISR_TXIS));
    I2C1->TXDR = data;
    
    // Wait for transfer complete
    while (!(I2C1->ISR & I2C_ISR_TC));
    
    // Generate STOP condition
    I2C1->CR2 |= I2C_CR2_STOP;
    
    // Wait for write cycle time
    for(volatile int i = 0; i < 48000 * EEPROM_WRITE_CYCLE_TIME; i++);
    
    return EEPROM_OK;
}

int8_t eeprom_read_byte(uint16_t addr, uint8_t *data) {
    i2c_wait_for_idle();
    
    // Set number of bytes to transmit (2 for address)
    I2C1->CR2 = (2 << I2C_CR2_NBYTES_Pos);
    
    // Set slave address and write direction
    I2C1->CR2 &= ~I2C_CR2_SADD;
    I2C1->CR2 |= (EEPROM_ADDR << 1);
    
    // Generate START condition
    I2C1->CR2 |= I2C_CR2_START;
    
    // Send high byte of memory address
    while (!(I2C1->ISR & I2C_ISR_TXIS));
    I2C1->TXDR = (addr >> 8) & 0xFF;
    
    // Send low byte of memory address
    while (!(I2C1->ISR & I2C_ISR_TXIS));
    I2C1->TXDR = addr & 0xFF;
    
    // Wait for transfer complete
    while (!(I2C1->ISR & I2C_ISR_TC));
    
    // Set number of bytes to receive (1)
    I2C1->CR2 = (1 << I2C_CR2_NBYTES_Pos);
    
    // Set read direction
    I2C1->CR2 |= I2C_CR2_RD_WRN;
    
    // Set slave address for read
    I2C1->CR2 &= ~I2C_CR2_SADD;
    I2C1->CR2 |= (EEPROM_ADDR << 1);
    
    // Generate repeated START condition
    I2C1->CR2 |= I2C_CR2_START;
    
    // Wait for received data
    while (!(I2C1->ISR & I2C_ISR_RXNE));
    *data = I2C1->RXDR;
    
    // Wait for transfer complete
    while (!(I2C1->ISR & I2C_ISR_TC));
    
    // Generate STOP condition
    I2C1->CR2 |= I2C_CR2_STOP;
    
    return EEPROM_OK;
}

int8_t eeprom_save_high_score(uint32_t score) {
    // Write score byte by byte
    for(int i = 0; i < 4; i++) {
        if(eeprom_write_byte(HIGH_SCORE_ADDR + i, (score >> (i*8)) & 0xFF) != EEPROM_OK) {
            return EEPROM_ERROR;
        }
    }
    return EEPROM_OK;
}

int8_t eeprom_get_high_score(uint32_t *score) {
    uint8_t byte;
    *score = 0;
    
    // Read score byte by byte
    for(int i = 0; i < 4; i++) {
        if(eeprom_read_byte(HIGH_SCORE_ADDR + i, &byte) != EEPROM_OK) {
            return EEPROM_ERROR;
        }
        *score |= ((uint32_t)byte << (i*8));
    }
    return EEPROM_OK;
}
