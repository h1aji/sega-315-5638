/*
 * Sega Mega Drive / Genesis controller
 * 3-button + proper 6-button extension
 *
 * MCU: ATmega8 / ATmega88
 * Clock: 8 MHz (internal)
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdint.h>

/* ================= Sega DB9 mapping =================
 *
 * PD2  UP / X
 * PD3  DOWN / Y
 * PD4  LEFT / Z
 * PD5  RIGHT / MODE
 * PD6  A / B
 * PD7  START / C
 *
 * PB0  TH / SELECT (input from console)
 *
 * All outputs are ACTIVE LOW
 */

/* ================= Button source =================
 * Replace these macros with your real button inputs
 */
#define BTN_UP()     PB0
#define BTN_RIGHT()  PB1
#define BTN_DOWN()   PB2
#define BTN_LEFT()   PB3
#define BTN_START()  PB4
#define BTN_A()      PB5
#define BTN_B()      PC0
#define BTN_C()      PC4
#define BTN_MODE()   PC5
#define BTN_X()      PC3
#define BTN_Y()      PC2
#define BTN_Z()      PC1


/* ================= Internal state ================= */

static volatile uint8_t phase = 0;
static volatile uint8_t isSix = 0;
static volatile uint8_t prevTH = 1;
static volatile uint8_t idleTicks = 0;

/* ================= Helpers ================= */

static inline void writeLo(uint8_t pin)
{
    PORTD &= ~(1 << pin);
}

static inline void writeHi(uint8_t pin)
{
    PORTD |= (1 << pin);
}

static inline uint8_t th_level(void)
{
    return (PINB & (1 << PB0)) ? 1 : 0;
}

/* ================= Timer0 overflow (~1 ms) ================= */

ISR(TIMER0_OVF_vect)
{
    if (++idleTicks > 12) {
        phase = 0;
        isSix = 0;     /* fallback to 3-button */
    }
}

/* ================= TH polling ================= */

static inline void sega_poll_th(void)
{
    uint8_t th = th_level();

    if (th != prevTH) {
        prevTH = th;
        idleTicks = 0;

        if (++phase >= 8) {
            phase = 0;
            isSix = 1;   /* successful 6-button detect */
        }
    }
}

/* ================= Sega output ================= */

static inline void sega_output(void)
{
    uint8_t th = th_level();

    /* release all lines */
    writeHi(PD2); writeHi(PD3); writeHi(PD4);
    writeHi(PD5); writeHi(PD6); writeHi(PD7);

    /* ---------- 3 BUTTON MODE ---------- */
    if (!isSix) {
        if (th) {
            BTN_B() ? writeLo(PD6) : writeHi(PD6);
            BTN_C() ? writeLo(PD7) : writeHi(PD7);
        } else {
            BTN_UP()    ? writeLo(PD2) : writeHi(PD2);
            BTN_DOWN()  ? writeLo(PD3) : writeHi(PD3);
            BTN_LEFT()  ? writeLo(PD4) : writeHi(PD4);
            BTN_RIGHT() ? writeLo(PD5) : writeHi(PD5);
            BTN_A()     ? writeLo(PD6) : writeHi(PD6);
            BTN_START() ? writeLo(PD7) : writeHi(PD7);
        }
        return;
    }

    /* ---------- 6 BUTTON MODE ---------- */
    switch (phase) {

    case 0:
    case 1:
    case 2:
        if (!th) {
            BTN_UP()    ? writeLo(PD2) : writeHi(PD2);
            BTN_DOWN()  ? writeLo(PD3) : writeHi(PD3);
            BTN_LEFT()  ? writeLo(PD4) : writeHi(PD4);
            BTN_RIGHT() ? writeLo(PD5) : writeHi(PD5);
        }
        break;

    case 3:
        if (th) {
            BTN_B() ? writeLo(PD6) : writeHi(PD6);
            BTN_C() ? writeLo(PD7) : writeHi(PD7);
        } else {
            BTN_A()     ? writeLo(PD6) : writeHi(PD6);
            BTN_START() ? writeLo(PD7) : writeHi(PD7);
        }
        break;

    case 4:
        /* 6-button ID phase */
        writeLo(PD2); writeLo(PD3);
        writeLo(PD4); writeLo(PD5);
        break;

    case 5:
        if (!th) {
            BTN_X()    ? writeLo(PD2) : writeHi(PD2);
            BTN_Y()    ? writeLo(PD3) : writeHi(PD3);
            BTN_Z()    ? writeLo(PD4) : writeHi(PD4);
            BTN_MODE() ? writeLo(PD5) : writeHi(PD5);
        }
        break;
    }
}

/* ================= Main ================= */

int main(void)
{
    /* Disable ADC (digital I/O only) */
    ADCSRA &= ~(1 << ADEN);

    /* PD2–PD7 outputs */
    DDRD |= (1 << PD2) | (1 << PD3) | (1 << PD4) |
            (1 << PD5) | (1 << PD6) | (1 << PD7);

    /* TH input */
    DDRB &= ~(1 << PB0);

#if defined(__AVR_ATmega8__)
    /* ---------- ATmega8 : Timer0 overflow ---------- */
    /* F_CPU = 8 MHz
    * Prescaler = 64
    * Overflow period ≈ 2.048 ms
    */

    TCCR0 = (1 << CS01) | (1 << CS00);   /* clk/64 */
    TIMSK |= (1 << TOIE0);              /* enable overflow interrupt */
#elif defined(__AVR_ATmega88__) || defined(__AVR_ATmega88A__) || defined(__AVR_ATmega88PA__)
    /* ---------- ATmega88 / ATmega88A / ATmega88PA / ATmega88PB ---------- */
    /* F_CPU = 8 MHz
    * Prescaler = 64
    * OCR0A = 124 → 1 ms
    */

    TCCR0A = (1 << WGM01);               /* CTC */
    TCCR0B = (1 << CS01) | (1 << CS00);  /* clk/64 */
    OCR0A  = 124;
    TIMSK0 = (1 << OCIE0A);
#endif

    sei();

    while (1) {
        sega_poll_th();
        sega_output();
    }
}
