/* Programming MAX7219 via SPI with FRDM-KL25Z */
/* Part 3 - Option A: ADC monitoring system */
/*
 * PTC5 = SPI0 SCK
 * PTC6 = SPI0 MOSI
 * PTC4 = GPIO Chip Select / LOAD
 * PTB0 = ADC0_SE8 potentiometer input
 *
 * Keypad rows: PTC8, PTC9, PTC10, PTC11
 * Keypad columns: PTC12, PTC13, PTC16, PTC17
 * PB1 = PTA12
 * PB2 = PTA13
 */

#include <MKL25Z4.H>

#define DECODE      9
#define INTENSITY   10
#define SCANLIMIT   11
#define SHUTDOWN    12
#define TEST        15

#define ROW_MASK    ((1u << 8) | (1u << 9) | (1u << 10) | (1u << 11))

#define LED_OFF     0
#define LED_RED     1
#define LED_GREEN   2
#define LED_BLUE    3
#define LED_PURPLE  4

typedef enum
{
    NORMAL_MODE,
    MINIMUM_MODE,
    MAXIMUM_MODE,
    THRESHOLD_MODE
} MonitorMode;

volatile unsigned int milliseconds = 0;

void SPI0_init(void);
void max7219_write(unsigned char command, unsigned char data);
void display_number(unsigned int value);
void display_entry(unsigned char digits[], unsigned char count);
void ADC0_init(void);
unsigned int ADC0_read(void);
void GPIO_init(void);
char keypad_read(void);
void RGB_color(unsigned char color);

void SysTick_Handler(void)
{
    milliseconds++;
}

int main(void)
{
    unsigned int adc_value;
    unsigned int minimum_value;
    unsigned int maximum_value;
    unsigned int threshold = 2000;
    unsigned int new_threshold;
    unsigned int now;
    unsigned int last_adc = 0;
    unsigned int last_debounce = 0;

    unsigned char digits[4] = {0, 0, 0, 0};
    unsigned char digit_count = 0;
    unsigned char buttons = 0;
    unsigned char last_buttons = 0;
    unsigned char pressed;

    char key;
    char raw_key;
    char last_key = 0;

    MonitorMode mode = NORMAL_MODE;

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

    GPIO_init();
    ADC0_init();

    SystemCoreClockUpdate();
    SysTick->LOAD = (SystemCoreClock / 1000u) - 1u;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk |
                    SysTick_CTRL_ENABLE_Msk;

    /* The first reading initializes both stored values. */
    adc_value = ADC0_read();
    minimum_value = adc_value;
    maximum_value = adc_value;

    while (1)
    {
        now = milliseconds;
        pressed = 0;
        key = 0;

        /* Take a new ADC sample every 100 ms. */
        if ((unsigned int)(now - last_adc) >= 100u)
        {
            last_adc = now;
            adc_value = ADC0_read();

            if (adc_value < minimum_value)
            {
                minimum_value = adc_value;
            }

            if (adc_value > maximum_value)
            {
                maximum_value = adc_value;
            }
        }

        /* Simple debounce: read buttons and keypad every 30 ms. */
        if ((unsigned int)(now - last_debounce) >= 30u)
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

            pressed = buttons & (unsigned char)(~last_buttons);
            last_buttons = buttons;

            raw_key = keypad_read();

            if (raw_key != last_key)
            {
                last_key = raw_key;

                if (raw_key != 0)
                {
                    key = raw_key;
                }
            }
        }

        /* PB1 changes to the next operating mode. */
        if (pressed & 0x01)
        {
            mode++;

            if (mode > THRESHOLD_MODE)
            {
                mode = NORMAL_MODE;
            }

            digit_count = 0;
        }

        /* PB2 resets minimum and maximum to the current reading. */
        if (pressed & 0x02)
        {
            minimum_value = adc_value;
            maximum_value = adc_value;
        }

        if (mode == THRESHOLD_MODE)
        {
            if ((key >= '0') && (key <= '9'))
            {
                if (digit_count < 4u)
                {
                    digits[digit_count] = (unsigned char)(key - '0');
                    digit_count++;
                }
            }
            else if (key == '*')
            {
                digit_count = 0;
            }
            else if ((key == '#') && (digit_count == 4u))
            {
                new_threshold = (unsigned int)digits[0] * 1000u;
                new_threshold += (unsigned int)digits[1] * 100u;
                new_threshold += (unsigned int)digits[2] * 10u;
                new_threshold += digits[3];

                threshold = new_threshold;
                digit_count = 0;
            }
        }

        if (mode == NORMAL_MODE)
        {
            display_number(adc_value);

            if (adc_value >= threshold)
            {
                RGB_color(LED_RED);
            }
            else
            {
                RGB_color(LED_OFF);
            }
        }
        else if (mode == MINIMUM_MODE)
        {
            display_number(minimum_value);
            RGB_color(LED_BLUE);
        }
        else if (mode == MAXIMUM_MODE)
        {
            display_number(maximum_value);
            RGB_color(LED_GREEN);
        }
        else
        {
            if (digit_count > 0u)
            {
                display_entry(digits, digit_count);
            }
            else
            {
                display_number(threshold);
            }

            RGB_color(LED_PURPLE);
        }
    }
}

void ADC0_init(void)
{
    /* PTB0 is ADC0_SE8, so its pin mux stays in analog mode. */
    SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
    PORTB->PCR[0] = 0;

    SIM->SCGC6 |= SIM_SCGC6_ADC0_MASK;

    /* Bus clock divided by 4, 12-bit conversion. */
    ADC0->CFG1 = ADC_CFG1_ADIV(2) | ADC_CFG1_MODE(1);
    ADC0->SC2 = 0;
    ADC0->SC3 = 0;
}

unsigned int ADC0_read(void)
{
    ADC0->SC1[0] = ADC_SC1_ADCH(8);

    while (!(ADC0->SC1[0] & ADC_SC1_COCO_MASK))
    {
    }

    return ADC0->R[0];
}

void GPIO_init(void)
{
    SIM->SCGC5 |= SIM_SCGC5_PORTA_MASK |
                  SIM_SCGC5_PORTB_MASK |
                  SIM_SCGC5_PORTC_MASK |
                  SIM_SCGC5_PORTD_MASK;

    /* PB1 and PB2 are inputs with internal pull-up. */
    PORTA->PCR[12] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    PORTA->PCR[13] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    PTA->PDDR &= ~((1u << 12) | (1u << 13));

    /* Keypad rows are outputs. */
    PORTC->PCR[8] = PORT_PCR_MUX(1);
    PORTC->PCR[9] = PORT_PCR_MUX(1);
    PORTC->PCR[10] = PORT_PCR_MUX(1);
    PORTC->PCR[11] = PORT_PCR_MUX(1);
    PTC->PDDR |= ROW_MASK;
    PTC->PSOR = ROW_MASK;

    /* Keypad columns are inputs with internal pull-up. */
    PORTC->PCR[12] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    PORTC->PCR[13] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    PORTC->PCR[16] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    PORTC->PCR[17] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    PTC->PDDR &= ~((1u << 12) | (1u << 13) | (1u << 16) | (1u << 17));

    /* Integrated RGB LED outputs, all off at startup. */
    PORTB->PCR[18] = PORT_PCR_MUX(1);
    PORTB->PCR[19] = PORT_PCR_MUX(1);
    PORTD->PCR[1] = PORT_PCR_MUX(1);
    PTB->PDDR |= (1u << 18) | (1u << 19);
    PTD->PDDR |= (1u << 1);
    RGB_color(LED_OFF);
}

char keypad_read(void)
{
    static const char keys[4][4] =
    {
        {'1', '2', '3', 'A'},
        {'4', '5', '6', 'B'},
        {'7', '8', '9', 'C'},
        {'*', '0', '#', 'D'}
    };

    static const unsigned int rows[4] =
    {
        (1u << 8), (1u << 9), (1u << 10), (1u << 11)
    };

    static const unsigned int columns[4] =
    {
        (1u << 12), (1u << 13), (1u << 16), (1u << 17)
    };

    unsigned int row;
    unsigned int column;
    volatile unsigned int wait;

    PTC->PSOR = ROW_MASK;

    for (row = 0; row < 4u; row++)
    {
        PTC->PCOR = rows[row];

        for (wait = 0; wait < 10u; wait++)
        {
        }

        for (column = 0; column < 4u; column++)
        {
            if ((PTC->PDIR & columns[column]) == 0u)
            {
                PTC->PSOR = rows[row];
                return keys[row][column];
            }
        }

        PTC->PSOR = rows[row];
    }

    return 0;
}

void RGB_color(unsigned char color)
{
    /* The integrated RGB LED is active-low. */
    PTB->PSOR = (1u << 18) | (1u << 19);
    PTD->PSOR = (1u << 1);

    if (color == LED_RED)
    {
        PTB->PCOR = (1u << 18);
    }
    else if (color == LED_GREEN)
    {
        PTB->PCOR = (1u << 19);
    }
    else if (color == LED_BLUE)
    {
        PTD->PCOR = (1u << 1);
    }
    else if (color == LED_PURPLE)
    {
        PTB->PCOR = (1u << 18);
        PTD->PCOR = (1u << 1);
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

    while (!(SPI0->S & SPI_S_SPTEF_MASK))
    {
    }

    SPI0->D = command;

    while (!(SPI0->S & SPI_S_SPRF_MASK))
    {
    }

    dummy = SPI0->D;
    (void)dummy;

    while (!(SPI0->S & SPI_S_SPTEF_MASK))
    {
    }

    SPI0->D = data;

    while (!(SPI0->S & SPI_S_SPRF_MASK))
    {
    }

    dummy = SPI0->D;

    /* CS HIGH: MAX7219 latches the 16 transmitted bits */
    PTC->PSOR = (1 << 4);
}

void display_number(unsigned int value)
{
    if (value > 9999u)
    {
        value = 9999u;
    }

    max7219_write(0x01, (unsigned char)(value / 1000u));
    max7219_write(0x02, (unsigned char)((value / 100u) % 10u));
    max7219_write(0x03, (unsigned char)((value / 10u) % 10u));
    max7219_write(0x04, (unsigned char)(value % 10u));
}

void display_entry(unsigned char digits[], unsigned char count)
{
    unsigned char position;

    for (position = 0; position < 4u; position++)
    {
        if (position < count)
        {
            max7219_write((unsigned char)(position + 1u), digits[position]);
        }
        else
        {
            max7219_write((unsigned char)(position + 1u), 0x0F);
        }
    }
}
