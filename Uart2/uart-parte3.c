#include <MKL25Z4.H>

/* UART functions */
void UART0_init(void);
void UART0_putc(char c);
char UART0_getc(void);
void UART0_puts(const char *str);

/* Menu functions */
void showMenu(void);
void processCommand(char option);

void optionLED(void);
void optionADC(void);
void optionKeypad(void);
void optionButtons(void);

/* -------------------------------------------------
 * UART FUNCTIONS
 * ------------------------------------------------- */

void UART0_init(void)
{
    /* Enable clock for UART0 */
    SIM->SCGC4 |= 0x0400;

    /* Use FLL output for UART baud rate generator */
    SIM->SOPT2 |= 0x04000000;

    /* Turn off UART0 while changing configurations */
    UART0->C2 = 0x00;

    /* 115200 Baud */
    UART0->BDH = 0x00;
    UART0->BDL = 0x17;

    /* Over Sampling Ratio = 16 */
    UART0->C4 = 0x0F;

    /* 8-bit data, no parity */
    UART0->C1 = 0x00;

    /* Enable transmitter AND receiver */
    UART0->C2 = 0x0C;

    /* Enable clock for PORTA */
    SIM->SCGC5 |= 0x0200;

    /* PTA2 = UART0_TX */
    PORTA->PCR[2] = 0x0200;

    /* PTA1 = UART0_RX */
    PORTA->PCR[1] = 0x0200;
}

void UART0_putc(char c)
{
    /* Wait until transmit data register is empty */
    while (!(UART0->S1 & 0x80))
    {
    }

    UART0->D = c;
}

char UART0_getc(void)
{
    /* Wait until a character is received */
    while (!(UART0->S1 & 0x20))
    {
    }

    return UART0->D;
}

void UART0_puts(const char *str)
{
    while (*str != '\0')
    {
        UART0_putc(*str);
        str++;
    }
}

/* Auxiliary functions added for the menu options. */
static void UART0_put_number(unsigned int number)
{
    char digits[10];
    int i = 0;

    if (number == 0)
    {
        UART0_putc('0');
        return;
    }

    while (number > 0)
    {
        digits[i++] = (char)('0' + (number % 10));
        number /= 10;
    }

    while (i > 0)
        UART0_putc(digits[--i]);
}

static void delay500ms(void)
{
    volatile unsigned int i;
    for (i = 0; i < 3500000; i++)
    {
    }
}

/* -------------------------------------------------
 * MENU
 * ------------------------------------------------- */

void showMenu(void)
{
    UART0_puts("\r\n================================\r\n");
    UART0_puts("      KL25Z UART SYSTEM\r\n");
    UART0_puts("================================\r\n");
    UART0_puts("Commands:\r\n");
    UART0_puts("L - LED control\r\n");
    UART0_puts("A - Read ADC\r\n");
    UART0_puts("K - Read keypad\r\n");
    UART0_puts("B - Button status\r\n");
    UART0_puts("================================\r\n");
    UART0_puts("Please select an option: ");
}

/* -------------------------------------------------
 * MENU OPTIONS
 * ------------------------------------------------- */

void optionLED(void)
{
    char command;

    SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK | SIM_SCGC5_PORTD_MASK;
    PORTB->PCR[18] = PORT_PCR_MUX(1);
    PORTB->PCR[19] = PORT_PCR_MUX(1);
    PORTD->PCR[1] = PORT_PCR_MUX(1);
    PTB->PDDR |= (1u << 18) | (1u << 19);
    PTD->PDDR |= (1u << 1);

    UART0_puts("\r\nLED control\r\n");
    UART0_puts("1 - Red\r\n2 - Green\r\n3 - Blue\r\n");
    UART0_puts("0 - All OFF\r\nQ - Main menu\r\n");

    while (1)
    {
        command = UART0_getc();

        PTB->PSOR = (1u << 18) | (1u << 19);
        PTD->PSOR = (1u << 1);

        if (command == '1')
        {
            PTB->PCOR = (1u << 18);
            UART0_puts("\r\nRed LED ON\r\n");
        }
        else if (command == '2')
        {
            PTB->PCOR = (1u << 19);
            UART0_puts("\r\nGreen LED ON\r\n");
        }
        else if (command == '3')
        {
            PTD->PCOR = (1u << 1);
            UART0_puts("\r\nBlue LED ON\r\n");
        }
        else if (command == '0')
            UART0_puts("\r\nAll LEDs OFF\r\n");
        else if (command == 'Q' || command == 'q')
            return;
        else if (command != '\r' && command != '\n')
            UART0_puts("\r\nInvalid command.\r\n");
    }
}

void optionADC(void)
{
    unsigned int adcValue;
    unsigned int millivolts;
    char command;

    /* Potentiometer on PTE20 / ADC0_SE0. */
    SIM->SCGC5 |= SIM_SCGC5_PORTE_MASK;
    SIM->SCGC6 |= SIM_SCGC6_ADC0_MASK;
    PORTE->PCR[20] = 0;
    ADC0->CFG1 = ADC_CFG1_ADIV(1) | ADC_CFG1_MODE(1);
    ADC0->SC3 = ADC_SC3_AVGE_MASK | ADC_SC3_AVGS(3);

    UART0_puts("\r\nADC Monitoring (Q to return)\r\n");

    while (1)
    {
        if (UART0->S1 & 0x20)
        {
            command = UART0->D;
            if (command == 'Q' || command == 'q')
                return;
            UART0_puts("\r\nInvalid command.\r\n");
        }

        ADC0->SC1[0] = ADC_SC1_ADCH(0);
        while (!(ADC0->SC1[0] & ADC_SC1_COCO_MASK))
        {
        }
        adcValue = ADC0->R[0];
        millivolts = (adcValue * 3300u) / 4095u;

        UART0_puts("ADC Value: ");
        UART0_put_number(adcValue);
        UART0_puts("\r\nVoltage: ");
        UART0_put_number(millivolts / 1000u);
        UART0_putc('.');
        if ((millivolts % 1000u) < 100u) UART0_putc('0');
        if ((millivolts % 1000u) < 10u) UART0_putc('0');
        UART0_put_number(millivolts % 1000u);
        UART0_puts(" V\r\n\r\n");
        delay500ms();
    }
}

void optionKeypad(void)
{
    static const char keys[4][4] = {
        {'1', '2', '3', 'A'},
        {'4', '5', '6', 'B'},
        {'7', '8', '9', 'C'},
        {'*', '0', '#', 'D'}
    };
    int row;
    int column;
    char lastKey = 0;

    // PTB0-PTB7
    SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
    for (row = 0; row < 4; row++)
    {
        PORTB->PCR[row] = PORT_PCR_MUX(1);
        PORTB->PCR[row + 4] = PORT_PCR_MUX(1) |
                             PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    }
    PTB->PDDR |= 0x0F;
    PTB->PDDR &= ~0xF0;
    PTB->PSOR = 0x0F;

    UART0_puts("\r\nPress a key (Q from Tera Term to return):\r\n");

    while (1)
    {
        if (UART0->S1 & 0x20)
        {
            char command = UART0->D;
            if (command == 'Q' || command == 'q') return;
            UART0_puts("\r\nInvalid command.\r\n");
        }

        for (row = 0; row < 4; row++)
        {
            PTB->PSOR = 0x0F;
            PTB->PCOR = (1u << row);
            for (column = 0; column < 4; column++)
            {
                if ((PTB->PDIR & (1u << (column + 4))) == 0)
                {
                    if (lastKey != keys[row][column])
                    {
                        lastKey = keys[row][column];
                        UART0_puts("Key pressed: ");
                        UART0_putc(lastKey);
                        UART0_puts("\r\n");
                    }
                }
            }
        }
        if ((PTB->PDIR & 0xF0) == 0xF0) lastKey = 0;
    }
}

void optionButtons(void)
{
    unsigned int old1;
    unsigned int old2;
    unsigned int button1;
    unsigned int button2;

    /* External buttons on PTC12 and PTC13, active low. */
    SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK;
    PORTC->PCR[12] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    PORTC->PCR[13] = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    PTC->PDDR &= ~((1u << 12) | (1u << 13));
    old1 = (PTC->PDIR >> 12) & 1u;
    old2 = (PTC->PDIR >> 13) & 1u;

    UART0_puts("\r\nButton Monitoring (Q to return)\r\n");

    while (1)
    {
        if (UART0->S1 & 0x20)
        {
            char command = UART0->D;
            if (command == 'Q' || command == 'q') return;
            UART0_puts("\r\nInvalid command.\r\n");
        }

        button1 = (PTC->PDIR >> 12) & 1u;
        button2 = (PTC->PDIR >> 13) & 1u;

        if (button1 != old1)
        {
            UART0_puts(button1 ? "Button 1: RELEASED\r\n" :
                                 "Button 1: PRESSED\r\n");
            old1 = button1;
        }
        if (button2 != old2)
        {
            UART0_puts(button2 ? "Button 2: RELEASED\r\n" :
                                 "Button 2: PRESSED\r\n");
            old2 = button2;
        }
    }
}

/* -------------------------------------------------
 * COMMAND PROCESSING
 * ------------------------------------------------- */

void processCommand(char option)
{
    if (option == 'L' || option == 'l') optionLED();
    else if (option == 'A' || option == 'a') optionADC();
    else if (option == 'K' || option == 'k') optionKeypad();
    else if (option == 'B' || option == 'b') optionButtons();
    else if (option != '\r' && option != '\n')
        UART0_puts("\r\nInvalid command.\r\n");

    showMenu();
}

/* -------------------------------------------------
 * MAIN
 * ------------------------------------------------- */

int main(void)
{
    char option;

    /* Initialize peripherals */
    UART0_init();

    /* Show menu */
    showMenu();

    while (1)
    {
        option = UART0_getc();

        processCommand(option);
    }
}
