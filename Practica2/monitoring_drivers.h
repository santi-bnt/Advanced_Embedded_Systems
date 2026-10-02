#ifndef MONITORING_DRIVERS_H
#define MONITORING_DRIVERS_H

#include <MKL25Z4.H>

#define DS3231_ADDRESS  0x68
#define TMP102_ADDRESS  0x48
#define LCD_I2C_ADDRESS 0x27
#define SHT31_ADDRESS   0x44
#define BH1750_ADDRESS  0x23

#define DECODE     9
#define INTENSITY  10
#define SCANLIMIT  11
#define SHUTDOWN   12
#define TEST       15

#define ROW_MASK ((1u << 8) | (1u << 9) | (1u << 10) | (1u << 11))

unsigned char rtc_second;
unsigned char rtc_minute;
unsigned char rtc_hour;
unsigned char rtc_date;
unsigned char rtc_month;
unsigned char rtc_year;

volatile unsigned char alarm_active = 0;
unsigned char alarm_enabled = 0;

void delay_ms(unsigned int time)
{
    volatile unsigned int i;

    while (time > 0u)
    {
        for (i = 0; i < 7000u; i++)
        {
        }

        time--;
    }
}

unsigned char to_bcd(unsigned char number)
{
    return ((number / 10u) << 4) | (number % 10u);
}

unsigned char from_bcd(unsigned char number)
{
    return ((number >> 4) * 10u) + (number & 0x0Fu);
}


/* ---------------- I2C1: PTC1 SCL, PTC2 SDA ---------------- */

void I2C1_init(void)
{
    SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK;
    SIM->SCGC4 |= SIM_SCGC4_I2C1_MASK;

    /* PTC1 = I2C1_SCL */
    PORTC->PCR[1] = PORT_PCR_MUX(2) |
                    PORT_PCR_PE_MASK |
                    PORT_PCR_PS_MASK;

    /* PTC2 = I2C1_SDA */
    PORTC->PCR[2] = PORT_PCR_MUX(2) |
                    PORT_PCR_PE_MASK |
                    PORT_PCR_PS_MASK;

    I2C1->F = 0x14;
    I2C1->C1 = I2C_C1_IICEN_MASK;
}


void I2C_wait(void)
{
    while (!(I2C1->S & I2C_S_IICIF_MASK))
    {
    }

    I2C1->S = I2C_S_IICIF_MASK;
}


unsigned char I2C_probe(unsigned char address)
{
    unsigned char ack;

    I2C1->C1 |= I2C_C1_TX_MASK;
    I2C1->C1 |= I2C_C1_MST_MASK;

    I2C1->D = (address << 1);

    I2C_wait();

    ack = ((I2C1->S & I2C_S_RXAK_MASK) == 0u);

    I2C1->C1 &= ~I2C_C1_MST_MASK;
    I2C1->C1 &= ~I2C_C1_TX_MASK;

    delay_ms(2);

    return ack;
}


void I2C_send(unsigned char address, unsigned char data)
{
    I2C1->C1 |= I2C_C1_TX_MASK;
    I2C1->C1 |= I2C_C1_MST_MASK;

    I2C1->D = address << 1;
    I2C_wait();

    I2C1->D = data;
    I2C_wait();

    I2C1->C1 &= ~I2C_C1_MST_MASK;
    I2C1->C1 &= ~I2C_C1_TX_MASK;
}


void I2C_command(unsigned char address,
                 unsigned char data[],
                 unsigned char size)
{
    unsigned char i;

    I2C1->C1 |= I2C_C1_TX_MASK;
    I2C1->C1 |= I2C_C1_MST_MASK;

    I2C1->D = address << 1;
    I2C_wait();

    for (i = 0; i < size; i++)
    {
        I2C1->D = data[i];
        I2C_wait();
    }

    I2C1->C1 &= ~I2C_C1_MST_MASK;
    I2C1->C1 &= ~I2C_C1_TX_MASK;
}


void I2C_write(unsigned char address,
               unsigned char reg,
               unsigned char data[],
               unsigned char size)
{
    unsigned char i;

    I2C1->C1 |= I2C_C1_TX_MASK;
    I2C1->C1 |= I2C_C1_MST_MASK;

    I2C1->D = address << 1;
    I2C_wait();

    I2C1->D = reg;
    I2C_wait();

    for (i = 0; i < size; i++)
    {
        I2C1->D = data[i];
        I2C_wait();
    }

    I2C1->C1 &= ~I2C_C1_MST_MASK;
    I2C1->C1 &= ~I2C_C1_TX_MASK;
}


void I2C_read_direct(unsigned char address,
                     unsigned char data[],
                     unsigned char size)
{
    unsigned char i;
    volatile unsigned char dummy;

    I2C1->C1 |= I2C_C1_TX_MASK;
    I2C1->C1 |= I2C_C1_MST_MASK;

    I2C1->D = (address << 1) | 1u;
    I2C_wait();

    I2C1->C1 &= ~I2C_C1_TX_MASK;

    if (size == 1u)
        I2C1->C1 |= I2C_C1_TXAK_MASK;
    else
        I2C1->C1 &= ~I2C_C1_TXAK_MASK;

    dummy = I2C1->D;
    (void)dummy;

    for (i = 0; i < size; i++)
    {
        I2C_wait();

        if (i == (unsigned char)(size - 2u))
        {
            I2C1->C1 |= I2C_C1_TXAK_MASK;
        }

        if (i == (unsigned char)(size - 1u))
        {
            I2C1->C1 &= ~I2C_C1_MST_MASK;
        }

        data[i] = I2C1->D;
    }

    I2C1->C1 &= ~I2C_C1_TXAK_MASK;
    I2C1->C1 |= I2C_C1_TX_MASK;
}


void I2C_read(unsigned char address,
              unsigned char reg,
              unsigned char data[],
              unsigned char size)
{
    unsigned char i;
    volatile unsigned char dummy;

    I2C1->C1 |= I2C_C1_TX_MASK;
    I2C1->C1 |= I2C_C1_MST_MASK;

    I2C1->D = address << 1;
    I2C_wait();

    I2C1->D = reg;
    I2C_wait();

    I2C1->C1 |= I2C_C1_RSTA_MASK;

    I2C1->D = (address << 1) | 1u;
    I2C_wait();

    I2C1->C1 &= ~I2C_C1_TX_MASK;

    if (size == 1u)
        I2C1->C1 |= I2C_C1_TXAK_MASK;
    else
        I2C1->C1 &= ~I2C_C1_TXAK_MASK;

    dummy = I2C1->D;
    (void)dummy;

    for (i = 0; i < size; i++)
    {
        I2C_wait();

        if (i == (unsigned char)(size - 2u))
        {
            I2C1->C1 |= I2C_C1_TXAK_MASK;
        }

        if (i == (unsigned char)(size - 1u))
        {
            I2C1->C1 &= ~I2C_C1_MST_MASK;
        }

        data[i] = I2C1->D;
    }

    I2C1->C1 &= ~I2C_C1_TXAK_MASK;
    I2C1->C1 |= I2C_C1_TX_MASK;
}

/* ---------------- DS3231 ---------------- */

void RTC_set(unsigned char h, unsigned char m, unsigned char s,
             unsigned char d, unsigned char mon, unsigned char y)
{
    unsigned char data[7];

    data[0] = to_bcd(s);
    data[1] = to_bcd(m);
    data[2] = to_bcd(h);
    data[3] = 1;
    data[4] = to_bcd(d);
    data[5] = to_bcd(mon);
    data[6] = to_bcd(y);
    I2C_write(DS3231_ADDRESS, 0x00, data, 7);
}

void RTC_read(void)
{
    unsigned char data[7];

    I2C_read(DS3231_ADDRESS, 0x00, data, 7);
    rtc_second = from_bcd(data[0] & 0x7F);
    rtc_minute = from_bcd(data[1] & 0x7F);
    rtc_hour = from_bcd(data[2] & 0x3F);
    rtc_date = from_bcd(data[4] & 0x3F);
    rtc_month = from_bcd(data[5] & 0x1F);
    rtc_year = from_bcd(data[6]);
}

void RTC_alarm_set(unsigned char h, unsigned char m)
{
    unsigned char data[9];

    /* Alarm 1 registers: 0x07 to 0x0A. */
    data[0] = to_bcd(0);
    data[1] = to_bcd(m);
    data[2] = to_bcd(h);
    data[3] = 0x80;

    /* Leave Alarm 2 disabled. */
    data[4] = 0x80;
    data[5] = 0x80;
    data[6] = 0x80;

    /* Control and status registers: enable Alarm 1 and clear flags. */
    data[7] = 0x05;
    data[8] = 0x00;

    I2C_write(DS3231_ADDRESS, 0x07, data, 9);

    alarm_active = 0;
    alarm_enabled = 1;
    PORTD->ISFR = (1u << 7);
}

void RTC_alarm_enable(unsigned char enable)
{
    unsigned char control;

    I2C_read(DS3231_ADDRESS, 0x0E, &control, 1);
    control |= 0x04;

    if (enable)
    {
        control |= 0x01;
        alarm_enabled = 1;
    }
    else
    {
        control &= (unsigned char)~0x01;
        alarm_enabled = 0;
    }

    I2C_write(DS3231_ADDRESS, 0x0E, &control, 1);
}

void RTC_alarm_clear(void)
{
    unsigned char status = 0x00;

    I2C_write(DS3231_ADDRESS, 0x0F, &status, 1);
    delay_ms(2);
    alarm_active = 0;
    PTB->PCOR = (1u << 8);                  /* Buzzer off */
    PTB->PSOR = (1u << 18) | (1u << 19);   /* Red and green off */
    PTD->PSOR = (1u << 1);                 /* Blue off */
}

/* ---------------- Parallel LCD ---------------- */

void LCD_nibble(unsigned char value)
{
    PTD->PCOR = (1u << 3) |
                (1u << 4) |
                (1u << 5) |
                (1u << 6);

    if (value & 0x01u)
        PTD->PSOR = (1u << 3);

    if (value & 0x02u)
        PTD->PSOR = (1u << 4);

    if (value & 0x04u)
        PTD->PSOR = (1u << 5);

    if (value & 0x08u)
        PTD->PSOR = (1u << 6);

    PTD->PSOR = (1u << 2);
    delay_ms(1);

    PTD->PCOR = (1u << 2);
    delay_ms(1);
}

void LCD_send(unsigned char value, unsigned char rs)
{
    if (rs)
        PTD->PSOR = (1u << 0);
    else
        PTD->PCOR = (1u << 0);

    LCD_nibble(value >> 4);
    LCD_nibble(value & 0x0F);

    delay_ms(1);
}

void LCD_command(unsigned char command)
{
    LCD_send(command, 0);

    if ((command == 0x01u) || (command == 0x02u))
        delay_ms(3);
    else
        delay_ms(1);
}

void LCD_data(unsigned char data)
{
    LCD_send(data, 1);
}

void LCD_text(char text[])
{
    while (*text)
    {
        LCD_data((unsigned char)*text);
        text++;
    }
}

void LCD_init(void)
{
    SIM->SCGC5 |= SIM_SCGC5_PORTD_MASK;

    PORTD->PCR[0] = PORT_PCR_MUX(1); // RS
    PORTD->PCR[2] = PORT_PCR_MUX(1); // E
    PORTD->PCR[3] = PORT_PCR_MUX(1); // D4
    PORTD->PCR[4] = PORT_PCR_MUX(1); // D5
    PORTD->PCR[5] = PORT_PCR_MUX(1); // D6
    PORTD->PCR[6] = PORT_PCR_MUX(1); // D7

    PTD->PDDR |= (1u << 0) |
                 (1u << 2) |
                 (1u << 3) |
                 (1u << 4) |
                 (1u << 5) |
                 (1u << 6);

    PTD->PCOR = (1u << 0) |
                (1u << 2) |
                (1u << 3) |
                (1u << 4) |
                (1u << 5) |
                (1u << 6);

    delay_ms(50);

    LCD_nibble(0x03);
    delay_ms(5);

    LCD_nibble(0x03);
    delay_ms(5);

    LCD_nibble(0x03);
    delay_ms(5);

    LCD_nibble(0x02);
    delay_ms(5);

    LCD_command(0x28);
    LCD_command(0x08);
    LCD_command(0x01);
    LCD_command(0x06);
    LCD_command(0x0C);

    delay_ms(5);
}

void LCD_two(unsigned char number)
{
    LCD_data((number / 10u) + '0');
    LCD_data((number % 10u) + '0');
}


void LCD_time(void)
{
    LCD_command(0x80);
    LCD_text("Time: ");
    LCD_two(rtc_hour);
    LCD_data(':');
    LCD_two(rtc_minute);
    LCD_data(':');
    LCD_two(rtc_second);
}

void LCD_date(void)
{
    LCD_command(0xC0);
    LCD_text("Date: ");
    LCD_two(rtc_date);
    LCD_data('/');
    LCD_two(rtc_month);
    LCD_data('/');
    LCD_two(rtc_year);
}

void LCD_temperature(int temperature)
{
    unsigned int value;

    if (temperature < 0)
    {
        LCD_data('-');
        value = (unsigned int)(-temperature);
    }
    else
    {
        value = (unsigned int)temperature;
    }

    LCD_data((value / 100u) + '0');
    LCD_data(((value / 10u) % 10u) + '0');
    LCD_data('.');
    LCD_data((value % 10u) + '0');
    LCD_data('C');
}

/* ---------------- SPI0 and MAX7219 ---------------- */

void SPI0_init(void)
{
    SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK;
    PORTC->PCR[5] = PORT_PCR_MUX(2);
    PORTC->PCR[6] = PORT_PCR_MUX(2);
    PORTC->PCR[4] = PORT_PCR_MUX(1);
    PTC->PDDR |= (1u << 4);
    PTC->PSOR = (1u << 4);
    SIM->SCGC4 |= SIM_SCGC4_SPI0_MASK;
    SPI0->C1 = SPI_C1_MSTR_MASK;
    SPI0->BR = 0x60;
    SPI0->C1 |= SPI_C1_SPE_MASK;
}

void max7219_write(unsigned char command, unsigned char data)
{
    volatile unsigned char dummy;

    PTC->PCOR = (1u << 4);
    while (!(SPI0->S & SPI_S_SPTEF_MASK)) {}
    SPI0->D = command;
    while (!(SPI0->S & SPI_S_SPRF_MASK)) {}
    dummy = SPI0->D;
    (void)dummy;
    while (!(SPI0->S & SPI_S_SPTEF_MASK)) {}
    SPI0->D = data;
    while (!(SPI0->S & SPI_S_SPRF_MASK)) {}
    dummy = SPI0->D;
    PTC->PSOR = (1u << 4);
}

void MAX7219_init(void)
{
    max7219_write(DECODE, 0x1F);
    max7219_write(SCANLIMIT, 4);
    max7219_write(INTENSITY, 8);
    max7219_write(TEST, 0);
    max7219_write(SHUTDOWN, 1);
}

void MAX7219_time(void)
{
    max7219_write(0x01, rtc_hour / 10u);
    max7219_write(0x02, (rtc_hour % 10u) | 0x80u);
    max7219_write(0x03, rtc_minute / 10u);
    max7219_write(0x05, rtc_minute % 10u);
}

/* ---------------- Keypad ---------------- */

void keypad_init(void)
{
    SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK;
    PORTC->PCR[8] = PORT_PCR_MUX(1);
    PORTC->PCR[9] = PORT_PCR_MUX(1);
    PORTC->PCR[10] = PORT_PCR_MUX(1);
    PORTC->PCR[11] = PORT_PCR_MUX(1);
    PORTC->PCR[12] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    PORTC->PCR[13] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    PORTC->PCR[16] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    PORTC->PCR[17] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    PTC->PDDR |= ROW_MASK;
    PTC->PDDR &= ~((1u << 12) | (1u << 13) | (1u << 16) | (1u << 17));
    PTC->PSOR = ROW_MASK;
}

char keypad_raw(void)
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
        for (wait = 0; wait < 10u; wait++) {}

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

char keypad_key(void)
{
    static char last_key = 0;
    char key = keypad_raw();

    if (key != last_key)
    {
        delay_ms(20);
        key = keypad_raw();

        if (key != last_key)
        {
            last_key = key;
            if (key != 0) return key;
        }
    }

    return 0;
}

void LCD_input(unsigned char data[], unsigned char count)
{
    unsigned char i;

    LCD_command(0xC0);
    for (i = 0; i < count; i++) LCD_data(data[i] + '0');
    for (; i < 16u; i++) LCD_data(' ');
}

/* ---------------- Alarm GPIO ---------------- */

void alarm_gpio_init(void)
{
    SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK |
                  SIM_SCGC5_PORTD_MASK |
                  SIM_SCGC5_PORTE_MASK;

    /* Only the buzzer is new. The visual alarm uses the existing RGB LED. */
    PORTB->PCR[8] = PORT_PCR_MUX(1);
    PORTB->PCR[18] = PORT_PCR_MUX(1);
    PORTB->PCR[19] = PORT_PCR_MUX(1);
    PORTD->PCR[1] = PORT_PCR_MUX(1);

    PTB->PDDR |= (1u << 8) | (1u << 18) | (1u << 19);
    PTD->PDDR |= (1u << 1);

    PTB->PCOR = (1u << 8);                  /* Buzzer off */
    PTB->PSOR = (1u << 18) | (1u << 19);   /* RGB is active-low */
    PTD->PSOR = (1u << 1);

    /* PTD7 receives the active-low INT/SQW signal from the DS3231. */
    PORTD->PCR[7] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK |
                    PORT_PCR_PS_MASK | PORT_PCR_IRQC(10);
    PTD->PDDR &= ~(1u << 7);
    PORTD->ISFR = (1u << 7);
    NVIC_EnableIRQ(PORTD_IRQn);
}

void PORTD_IRQHandler(void)
{
    if (PORTD->ISFR & (1u << 7))
    {
        PORTD->ISFR = (1u << 7);
        alarm_active = 1;
        PTB->PSOR = (1u << 8);              /* Buzzer on */
        PTB->PCOR = (1u << 18);             /* Red on */
        PTB->PSOR = (1u << 19);             /* Green off */
        PTD->PSOR = (1u << 1);              /* Blue off */
    }
}

/* ---------------- TMP102 temperature ---------------- */

int TMP102_read(void)
{
    unsigned char data[2];
    int temperature;

    I2C_read_direct(TMP102_ADDRESS, data, 2);

    temperature = ((int)data[0] << 8) | data[1];

    return temperature;
}

/* ---------------- I2C LCD extra ---------------- */

void LCDI2C_nibble(unsigned char value, unsigned char rs)
{
    unsigned char data = (value << 4) | 0x08;
    if (rs) data |= 0x01;
    I2C_send(LCD_I2C_ADDRESS, data | 0x04);
    I2C_send(LCD_I2C_ADDRESS, data);
}

void LCDI2C_send(unsigned char value, unsigned char rs)
{
    LCDI2C_nibble(value >> 4, rs);
    LCDI2C_nibble(value & 0x0F, rs);
}

void LCDI2C_command(unsigned char command)
{
    LCDI2C_send(command, 0);
    delay_ms(2);
}

void LCDI2C_data(unsigned char data)
{
    LCDI2C_send(data, 1);
}

void LCDI2C_text(char text[])
{
    while (*text)
    {
        LCDI2C_data((unsigned char)*text);
        text++;
    }
}

void LCDI2C_two(unsigned char number)
{
    LCDI2C_data((number / 10u) + '0');
    LCDI2C_data((number % 10u) + '0');
}

void LCDI2C_init(void)
{
    delay_ms(40);
    LCDI2C_nibble(0x03, 0);
    delay_ms(5);
    LCDI2C_nibble(0x03, 0);
    LCDI2C_nibble(0x03, 0);
    LCDI2C_nibble(0x02, 0);
    LCDI2C_command(0x28);
    LCDI2C_command(0x0C);
    LCDI2C_command(0x06);
    LCDI2C_command(0x01);
}

/* ---------------- Extra sensors ---------------- */

void SHT31_read(int *temperature, unsigned int *humidity)
{
    unsigned char command[2] = {0x24, 0x00};
    unsigned char data[6];
    unsigned long raw_temperature;
    unsigned long raw_humidity;

    I2C_command(SHT31_ADDRESS, command, 2);
    delay_ms(20);
    I2C_read_direct(SHT31_ADDRESS, data, 6);

    raw_temperature = ((unsigned long)data[0] << 8) | data[1];
    raw_humidity = ((unsigned long)data[3] << 8) | data[4];
    *temperature = -450 + (int)((1750ul * raw_temperature) / 65535ul);
    *humidity = (unsigned int)((1000ul * raw_humidity) / 65535ul);
}

void BH1750_init(void)
{
    I2C_send(BH1750_ADDRESS, 0x10);
    delay_ms(180);
}

unsigned int BH1750_read(void)
{
    unsigned char data[2];
    unsigned long raw;

    I2C_read_direct(BH1750_ADDRESS, data, 2);
    raw = ((unsigned long)data[0] << 8) | data[1];
    return (unsigned int)((raw * 10ul) / 12ul);
}

void ADC0_init(void)
{
    SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
    PORTB->PCR[0] = 0;
    SIM->SCGC6 |= SIM_SCGC6_ADC0_MASK;
    ADC0->CFG1 = ADC_CFG1_ADIV(2) | ADC_CFG1_MODE(1);
    ADC0->SC2 = 0;
    ADC0->SC3 = 0;
}

unsigned int ADC0_read(void)
{
    ADC0->SC1[0] = ADC_SC1_ADCH(8);
    while (!(ADC0->SC1[0] & ADC_SC1_COCO_MASK)) {}
    return ADC0->R[0];
}

#endif
