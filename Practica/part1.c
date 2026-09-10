/* Programming MAX7219 via SPI with FRDM-KL25Z */
/* MAX7219 is connected to four 7-segment displays */
/* The program displays "1234" */
/*
 * PTC5 = SPI0 SCK
 * PTC6 = SPI0 MOSI
 * PTC4 = GPIO Chip Select / LOAD
 */

#include <MKL25Z4.H>

void SPI0_init(void);
void max7219_write(unsigned char command, unsigned char data);

#define DECODE      9
#define INTENSITY   10
#define SCANLIMIT   11
#define SHUTDOWN    12
#define TEST        15

int main(void)
{
    SPI0_init();

    max7219_write(DECODE, 0x0F);
    /* Enable BCD decode for digits 1, 2, 3 and 4 */

    max7219_write(SCANLIMIT, 3);
    /* Scan four digits: DIG0-DIG3 */

    max7219_write(INTENSITY, 4);
    /* Set display intensity */

    max7219_write(TEST, 0);
    /* Disable display test mode */

    max7219_write(SHUTDOWN, 1);
    /* Enable MAX7219 */

    max7219_write(0x01, 1);
    /* Display 1 */

    max7219_write(0x02, 2);
    /* Display 2 */

    max7219_write(0x03, 3);
    /* Display 3 */

    max7219_write(0x04, 4);
    /* Display 4 */

    while (1)
    {
    }
}

void SPI0_init(void)
{
    /* Enable clock to PORT C */
    SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK;

    /*
     * PTC5 = SPI0_SCK
     * MUX = ALT2
     */
    PORTC->PCR[5] = PORT_PCR_MUX(2);

    /*
     * PTC6 = SPI0_MOSI
     * MUX = ALT2
     */
    PORTC->PCR[6] = PORT_PCR_MUX(2);

    /*
     * PTC4 = GPIO for CS / LOAD
     * MUX = ALT1
     */
    PORTC->PCR[4] = PORT_PCR_MUX(1);

    /*
     * Make PTC4 output
     */
    PTC->PDDR |= (1 << 4);

    /*
     * CS idle HIGH
     */
    PTC->PSOR = (1 << 4);

    /*
     * Enable clock to SPI0
     */
    SIM->SCGC4 |= SIM_SCGC4_SPI0_MASK;

    /*
     * SPI0 master mode
     * SPI disabled while configuring
     */
    SPI0->C1 = SPI_C1_MSTR_MASK;

    /*
     * Baud rate configuration
     */
    SPI0->BR = 0x60;

    /*
     * Enable SPI0
     */
    SPI0->C1 |= SPI_C1_SPE_MASK;
}

void max7219_write(unsigned char command, unsigned char data)
{
    volatile unsigned char dummy;

    /*
     * CS LOW
     * Start transmission
     */
    PTC->PCOR = (1 << 4);

    /*
     * Wait until transmit buffer is ready
     */
    while (!(SPI0->S & SPI_S_SPTEF_MASK))
    {
    }

    /*
     * Send command/address byte
     */
    SPI0->D = command;

    /*
     * Wait until received byte is available
     */
    while (!(SPI0->S & SPI_S_SPRF_MASK))
    {
    }

    dummy = SPI0->D;
    (void)dummy;

    /*
     * Wait until transmit buffer is ready
     */
    while (!(SPI0->S & SPI_S_SPTEF_MASK))
    {
    }

    /*
     * Send data byte
     */
    SPI0->D = data;

    /*
     * Wait until transmission/received byte completes
     */
    while (!(SPI0->S & SPI_S_SPRF_MASK))
    {
    }

    dummy = SPI0->D;

    /*
     * CS HIGH
     * MAX7219 latches the 16 transmitted bits
     */
    PTC->PSOR = (1 << 4);
}
