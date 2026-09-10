/* Programming MAX7219 via SPI with FRDM-KL25Z */
/* Part 2: four-digit counter controlled by four push buttons */
/*
 * PTC5 = SPI0 SCK
 * PTC6 = SPI0 MOSI
 * PTC4 = GPIO Chip Select / LOAD
 *
 * PB1 PTA12: play/pause
 * PB2 PTA13: increment/decrement mode
 * PB3 PTE22: manual adjustment while paused
 * PB4 PTE23: reset according to the selected mode
 */

#include <MKL25Z4.H>

#define DECODE      9
#define INTENSITY   10
#define SCANLIMIT   11
#define SHUTDOWN    12
#define TEST        15

#define DEBOUNCE_MS 30u

typedef enum
{
    INCREMENT,
    DECREMENT
} CounterMode;

volatile unsigned int milliseconds = 0;

void SPI0_init(void);
void max7219_write(unsigned char command, unsigned char data);
void display_number(unsigned int value);
void buttons_init(void);

void SysTick_Handler(void)
{
    milliseconds++;
}

int main(void)
{
    unsigned int count = 0;
    unsigned int last_second = 0;
    unsigned int last_debounce = 0;
    unsigned int now;
    unsigned char running = 0;
    unsigned char buttons = 0;
    unsigned char last_buttons = 0;
    unsigned char pressed;
    CounterMode mode = INCREMENT;

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

    buttons_init();

    SystemCoreClockUpdate();
    SysTick->LOAD = (SystemCoreClock / 1000u) - 1u;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk |
                    SysTick_CTRL_ENABLE_Msk;

    display_number(count);

    while (1)
    {
        now = milliseconds;

        /* Read the buttons every 30 ms for a simple debounce. */
        if ((unsigned int)(now - last_debounce) >= DEBOUNCE_MS)
        {
            last_debounce = now;
            buttons = 0;

            if ((PTA->PDIR & (1u << 12)) == 0u)
            {
                buttons |= 0x01;
            }

            if ((PTA->PDIR & (1u << 13)) == 0u)
            {
                buttons |= 0x02;
            }

            if ((PTE->PDIR & (1u << 22)) == 0u)
            {
                buttons |= 0x04;
            }

            if ((PTE->PDIR & (1u << 23)) == 0u)
            {
                buttons |= 0x08;
            }

            pressed = buttons & (unsigned char)(~last_buttons);
            last_buttons = buttons;

            if (pressed & 0x01)
            {
                running = !running;
                last_second = now;
            }

            if (pressed & 0x02)
            {
                if (mode == INCREMENT)
                {
                    mode = DECREMENT;
                }
                else
                {
                    mode = INCREMENT;
                }
            }

            if ((pressed & 0x04) && !running)
            {
                if (mode == INCREMENT)
                {
                    count++;

                    if (count > 9999u)
                    {
                        count = 0;
                    }
                }
                else
                {
                    if (count == 0u)
                    {
                        count = 9999u;
                    }
                    else
                    {
                        count--;
                    }
                }
            }

            if (pressed & 0x08)
            {
                if (mode == INCREMENT)
                {
                    count = 0;
                }
                else
                {
                    count = 9999;
                }

                last_second = now;
            }
        }

        if (running && ((unsigned int)(now - last_second) >= 1000u))
        {
            last_second += 1000u;

            if (mode == INCREMENT)
            {
                count++;

                if (count > 9999u)
                {
                    count = 0;
                }
            }
            else
            {
                if (count == 0u)
                {
                    count = 9999u;
                }
                else
                {
                    count--;
                }
            }
        }

        display_number(count);
    }
}

void buttons_init(void)
{
    SIM->SCGC5 |= SIM_SCGC5_PORTA_MASK | SIM_SCGC5_PORTE_MASK;

    PORTA->PCR[12] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    PORTA->PCR[13] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    PORTE->PCR[22] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    PORTE->PCR[23] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;

    PTA->PDDR &= ~((1u << 12) | (1u << 13));
    PTE->PDDR &= ~((1u << 22) | (1u << 23));
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

    /* Make PTC4 output */
    PTC->PDDR |= (1 << 4);

    /* CS idle HIGH */
    PTC->PSOR = (1 << 4);

    /* Enable clock to SPI0 */
    SIM->SCGC4 |= SIM_SCGC4_SPI0_MASK;

    /* SPI0 master mode, disabled while configuring */
    SPI0->C1 = SPI_C1_MSTR_MASK;

    /* Baud rate configuration */
    SPI0->BR = 0x60;

    /* Enable SPI0 */
    SPI0->C1 |= SPI_C1_SPE_MASK;
}

void max7219_write(unsigned char command, unsigned char data)
{
    volatile unsigned char dummy;

    /* CS LOW: start transmission */
    PTC->PCOR = (1 << 4);

    /* Wait until transmit buffer is ready */
    while (!(SPI0->S & SPI_S_SPTEF_MASK))
    {
    }

    /* Send command/address byte */
    SPI0->D = command;

    /* Wait until received byte is available */
    while (!(SPI0->S & SPI_S_SPRF_MASK))
    {
    }

    dummy = SPI0->D;
    (void)dummy;

    /* Wait until transmit buffer is ready */
    while (!(SPI0->S & SPI_S_SPTEF_MASK))
    {
    }

    /* Send data byte */
    SPI0->D = data;

    /* Wait until transmission/received byte completes */
    while (!(SPI0->S & SPI_S_SPRF_MASK))
    {
    }

    dummy = SPI0->D;

    /* CS HIGH: MAX7219 latches the 16 transmitted bits */
    PTC->PSOR = (1 << 4);
}

void display_number(unsigned int value)
{
    max7219_write(0x01, (unsigned char)(value / 1000u));
    max7219_write(0x02, (unsigned char)((value / 100u) % 10u));
    max7219_write(0x03, (unsigned char)((value / 10u) % 10u));
    max7219_write(0x04, (unsigned char)(value % 10u));
}
