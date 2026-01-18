#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

/* =========================================================
   MCU auto-detection
   ========================================================= */
#if defined(__AVR_ATmega8__)
    #define MCU_ATMEGA8
    #define TIMER0_ISR_VECTOR TIMER0_OVF_vect
#elif defined(__AVR_ATmega88__)
    #define MCU_ATMEGA88
    #define TIMER0_ISR_VECTOR TIMER0_COMPA_vect
#else
    #error "Unsupported MCU"
#endif

/* =========================================================
   Sega interface (shared pins to console)
   ========================================================= */

/* TH / SELECT input from console */
#define TH_BIT     PD3
#define TH_PINREG  PIND

/* Sega shared lines (open-collector via DDR) */
#define SEGA_PORT  PORTB
#define SEGA_DDR   DDRB

#define PIN_LX     PD0   // LEFT / X
#define PIN_RM     PC3   // RIGHT / MODE
#define PIN_UZ     PC2   // UP / Z
#define PIN_DY     PC1   // DOWN / Y
#define PIN_BA     PC5   // B / A
#define PIN_CS     PC4   // C / START

/* =========================================================
   Button inputs (physical buttons)
   ========================================================= */

#define BTN_PORTC  PINC
#define BTN_UP     PD4
#define BTN_DOWN   PB6
#define BTN_LEFT   PB7
#define BTN_RIGHT  PD5

#define BTN_A      PD7
#define BTN_B      PB0
#define BTN_C      PB1
#define BTN_START  PD6

#define BTN_X      PB4
#define BTN_Y      PB5
#define BTN_Z      PB2
#define BTN_MODE   PC0

/* =========================================================
   Internal state
   ========================================================= */

volatile uint8_t cycle = 0;
volatile uint8_t prev_th = 1;
volatile uint8_t th_timeout = 0;

/* =========================================================
   Helpers
   ========================================================= */

static inline uint8_t pressed(volatile uint8_t *pin, uint8_t bit)
{
    return !(*pin & (1 << bit));
}

#define DRIVE_LOW(bit)   (SEGA_DDR |=  (1 << (bit)))
#define RELEASE(bit)     (SEGA_DDR &= ~(1 << (bit)))

/* =========================================================
   Timer0 — ~1 ms timeout watchdog
   ========================================================= */

ISR(TIMER0_ISR_VECTOR)
{
    if (++th_timeout > 12) {
        cycle = 0;        // reset to 3-button mode
    }
}

/* =========================================================
   TH falling-edge detection
   ========================================================= */

static inline void handle_th(void)
{
    uint8_t th = (TH_PINREG & (1 << TH_BIT)) ? 1 : 0;

    if (prev_th && !th) {      // FALLING edge only
        if (cycle < 7) cycle++;
        th_timeout = 0;
    }
    prev_th = th;
}

/* =========================================================
   Main
   ========================================================= */

int main(void)
{
    /* ---- Disable all analog functions ---- */
    ADCSRA &= ~(1 << ADEN);   // Disable ADC
    ACSR   |=  (1 << ACD);    // Disable analog comparator

#ifdef MCU_ATMEGA88
    DIDR0 = 0x00;             // Keep ADC pins as digital I/O
#endif

    /* ---- Button inputs with pull-ups ---- */
    DDRC = 0x00;
    PORTC = 0x3F;

    DDRD &= ~(1 << TH_BIT);
    PORTD |= (1 << TH_BIT);

    DDRD &= ~((1 << BTN_A) | (1 << BTN_START) |
              (1 << BTN_X) | (1 << BTN_Y) | (1 << BTN_Z));
    PORTD |= (1 << BTN_A) | (1 << BTN_START) |
             (1 << BTN_X) | (1 << BTN_Y) | (1 << BTN_Z);

    DDRB &= ~(1 << BTN_MODE);
    PORTB |= (1 << BTN_MODE);

    /* ---- Sega lines released (Hi-Z) ---- */
    SEGA_DDR  = 0x00;
    SEGA_PORT = 0x00;

#ifdef MCU_ATMEGA8

    /* -------- Timer0: NORMAL MODE, overflow only -------- */

    TCCR0 = (1 << CS01) | (1 << CS00);   /* prescaler clk/64 */
    TCNT0 = 256 - 124;                   /* preload for timing */
    TIMSK |= (1 << TOIE0);               /* enable overflow ISR */

#else

    /* -------- Timer0: CTC mode (ATmega88+) -------- */

    TCCR0A = (1 << WGM01);               /* CTC mode */
    TCCR0B = (1 << CS01) | (1 << CS00);  /* prescaler clk/64 */
    OCR0A  = 124;                        /* compare value */
    TIMSK0 = (1 << OCIE0A);              /* enable compare ISR */

#endif


    sei();

    while (1)
    {
        handle_th();

        /* Release all Sega lines */
        RELEASE(PIN_LX); RELEASE(PIN_RM); RELEASE(PIN_UZ);
        RELEASE(PIN_DY); RELEASE(PIN_BA); RELEASE(PIN_CS);

        switch (cycle)
        {
            case 0:
            case 1:
            case 2:
                if (pressed(&BTN_PORTC, BTN_LEFT))  DRIVE_LOW(PIN_LX);
                if (pressed(&BTN_PORTC, BTN_RIGHT)) DRIVE_LOW(PIN_RM);
                if (pressed(&BTN_PORTC, BTN_UP))    DRIVE_LOW(PIN_UZ);
                if (pressed(&BTN_PORTC, BTN_DOWN))  DRIVE_LOW(PIN_DY);
                break;

            case 3:
                if (pressed(&PIND, BTN_A))     DRIVE_LOW(PIN_BA);
                if (pressed(&PIND, BTN_START)) DRIVE_LOW(PIN_CS);
                if (pressed(&BTN_PORTC, BTN_B)) DRIVE_LOW(PIN_BA);
                if (pressed(&BTN_PORTC, BTN_C)) DRIVE_LOW(PIN_CS);
                break;

            case 4:
                /* 6-button ID phase: all released */
                break;

            case 5:
            case 7:
                if (pressed(&PIND, BTN_X))    DRIVE_LOW(PIN_LX);
                if (pressed(&PINB, BTN_MODE)) DRIVE_LOW(PIN_RM);
                if (pressed(&PIND, BTN_Z))    DRIVE_LOW(PIN_UZ);
                if (pressed(&PIND, BTN_Y))    DRIVE_LOW(PIN_DY);
                break;

            case 6:
                if (pressed(&PIND, BTN_A))     DRIVE_LOW(PIN_BA);
                if (pressed(&PIND, BTN_START)) DRIVE_LOW(PIN_CS);
                break;
        }
    }
}

