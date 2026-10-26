/*
 * Sega Mega Drive / Genesis controller
 * 3-button + proper 6-button extension
 * Emulates SEGA 315-5638 chip
 *
 * MCU: ATmega8 / ATmega48 / ATmega88 / ATmega168 / ATmega328
 * Clock: 8 MHz (internal)
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdint.h>

/* ================= Sega DB9 mapping =================
 *
 * PD2  UP / Z
 * PD3  DOWN / Y
 * PD4  LEFT / X
 * PD5  RIGHT / MODE
 * PD6  A / B
 * PD7  START / C
 *
 * PB7  TH / SELECT (input from console)
 *
 * All outputs are ACTIVE LOW
 */

/* Mask of the six Sega data lines on PORTD (PD2..PD7).
 * PD0/PD1 (RXD/TXD) are left untouched so we never disturb them. */
#define SEGA_OUT_MASK ((1 << PD2) | (1 << PD3) | (1 << PD4) | \
                       (1 << PD5) | (1 << PD6) | (1 << PD7))

/* ================= Button source =================
 * Replace these macros with your real button inputs
 * Note: Buttons are assumed to be active-low (pressed = LOW)
 */
#define BTN_UP()     ((PINB & (1 << PB0)) == 0)
#define BTN_RIGHT()  ((PINB & (1 << PB1)) == 0)
#define BTN_DOWN()   ((PINB & (1 << PB2)) == 0)
#define BTN_LEFT()   ((PINB & (1 << PB3)) == 0)
#define BTN_START()  ((PINB & (1 << PB4)) == 0)
#define BTN_A()      ((PINB & (1 << PB5)) == 0)
#define BTN_B()      ((PINC & (1 << PC0)) == 0)
#define BTN_C()      ((PINC & (1 << PC4)) == 0)
#define BTN_MODE()   ((PINC & (1 << PC5)) == 0)
#define BTN_X()      ((PINC & (1 << PC3)) == 0)
#define BTN_Y()      ((PINC & (1 << PC2)) == 0)
#define BTN_Z()      ((PINC & (1 << PC1)) == 0)


/* ================= Internal state ================= */

static volatile uint8_t phase = 7;
static volatile uint8_t prevTH = 1;
static volatile uint8_t idleTicks = 0;

/* ================= Timeout configuration =================
 * The real 315-5638 resets its phase counter to the idle (3-button
 * compatible) state after TH has been held high for ~1.5 ms. This MUST be
 * shorter than the console's inter-poll gap (16.6 ms @ 60 Hz, 20 ms @ 50 Hz),
 * otherwise a 3-button game that pulses TH once per frame drifts the phase
 * counter and eventually feeds it 6-button identification / X-Y-Z-MODE data.
 *
 * All supported MCUs are configured for a ~0.25 ms timer tick, so
 * IDLE_TIMEOUT_TICKS gives the SAME real-world timeout (~1.5 ms) on every
 * supported part.
 */
#define IDLE_TIMEOUT_TICKS 6   /* 6 x ~0.25 ms ~= 1.5 ms */

/* ================= Helper ================= */

static inline uint8_t th_level(void)
{
    return (PINB & (1 << PB7)) ? 1 : 0;
}

/* ================= Timer0 overflow / compare (~0.25 ms) ================= */

#if defined(__AVR_ATmega8__)
ISR(TIMER0_OVF_vect)
#else
ISR(TIMER0_COMPA_vect)
#endif
{
    uint8_t ticks = idleTicks;
    if (ticks < 255) {          /* saturate instead of wrapping */
        ticks++;
    }
    idleTicks = ticks;

    if (ticks >= IDLE_TIMEOUT_TICKS) {
        /* Return to idle/high compatible state after inactivity timeout */
        phase = 7;
    }
}

/* ================= TH polling ================= */

static inline void sega_poll_th(uint8_t th)
{
    if (th != prevTH) {
        prevTH = th;
        idleTicks = 0;
        phase = (phase + 1) & 7;
    }
}

/* ================= Sega output ================= */

/* Drive the six data lines for the current (TH, phase).
 * The full PORTD value is computed in a register and written ONCE, so there
 * is no transient "all released" glitch and no intermediate port states for
 * the console to sample. Active-LOW: a pressed button clears its bit. */
static inline void sega_output(uint8_t th)
{
    uint8_t ph  = phase;        /* snapshot; the ISR may change phase */
    uint8_t out = SEGA_OUT_MASK; /* all lines released (HIGH) */

    /* Phase counter increments on every TH edge change:
     * Phase 0: Pulse 1 LOW
     * Phase 1: Pulse 1 HIGH
     * Phase 2: Pulse 2 LOW
     * Phase 3: Pulse 2 HIGH
     * Phase 4: Pulse 3 LOW  - IDENTIFICATION (all 4 directional pins LOW)
     * Phase 5: Pulse 3 HIGH - X/Y/Z/MODE readable
     * Phase 6: Pulse 4 LOW  - Reset phase
     * Phase 7: Pulse 4 HIGH - Back to idle
     */
    if (!th) {
        /* TH LOW: UP/DOWN always present, A/START on pins 6/9 */
        if (ph == 4) {
            /* Identification phase: all four direction lines LOW */
            out &= ~((1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5));
        } else {
            if (BTN_UP())   out &= ~(1 << PD2);
            if (BTN_DOWN()) out &= ~(1 << PD3);
            out &= ~(1 << PD4); /* Ground on pin 3 */
            out &= ~(1 << PD5); /* Ground on pin 4 */
        }

        if (BTN_A())     out &= ~(1 << PD6);
        if (BTN_START()) out &= ~(1 << PD7);
    } else {
        /* TH HIGH: B/C always on pins 6/9 */
        if (ph == 5) {
            /* Extended 6-button read */
            if (BTN_Z())    out &= ~(1 << PD2);  /* Z on pin 1 */
            if (BTN_Y())    out &= ~(1 << PD3);  /* Y on pin 2 */
            if (BTN_X())    out &= ~(1 << PD4);  /* X on pin 3 */
            if (BTN_MODE()) out &= ~(1 << PD5);  /* MODE on pin 4 */
        } else {
            /* Normal directional read */
            if (BTN_UP())    out &= ~(1 << PD2);
            if (BTN_DOWN())  out &= ~(1 << PD3);
            if (BTN_LEFT())  out &= ~(1 << PD4);
            if (BTN_RIGHT()) out &= ~(1 << PD5);
        }

        if (BTN_B()) out &= ~(1 << PD6);
        if (BTN_C()) out &= ~(1 << PD7);
    }

    /* Single, glitch-free update of the six data lines */
    PORTD = (PORTD & ~SEGA_OUT_MASK) | out;
}

/* ================= Main ================= */

int main(void)
{
    /* Digital I/O only: disable ADC and the analog comparator */
    ADCSRA &= ~(1 << ADEN);
    ACSR   |=  (1 << ACD);

    /* PD2–PD7 outputs, initialised to released (HIGH) */
    DDRD  |= SEGA_OUT_MASK;
    PORTD |= SEGA_OUT_MASK;

    /* TH input (PB7) - no pull-up needed, console drives it */
    DDRB &= ~(1 << PB7);

    /* Button inputs: PB0-PB5 */
    DDRB &= ~((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB3) | (1 << PB4) | (1 << PB5));
    PORTB |= (1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB3) | (1 << PB4) | (1 << PB5);  /* Enable pull-ups */

    /* Button inputs: PC0-PC5 */
    DDRC &= ~((1 << PC0) | (1 << PC1) | (1 << PC2) | (1 << PC3) | (1 << PC4) | (1 << PC5));
    PORTC |= (1 << PC0) | (1 << PC1) | (1 << PC2) | (1 << PC3) | (1 << PC4) | (1 << PC5);  /* Enable pull-ups */

#if defined(__AVR_ATmega8__)
    /* ---------- ATmega8 : Timer0 overflow ---------- */
    /* F_CPU = 8 MHz, prescaler = 8 -> 1 us/count
     * Overflow period = 256 counts = 256 us (~0.25 ms)
     * IDLE_TIMEOUT_TICKS x 256 us ~= 1.5 ms timeout
     */
    TCCR0 = (1 << CS01);                 /* clk/8 */
    TCNT0 = 0;                           /* initialize counter */
    TIMSK |= (1 << TOIE0);               /* enable overflow interrupt */
#elif defined(__AVR_ATmega48__)   || defined(__AVR_ATmega48A__)  || \
      defined(__AVR_ATmega48P__)  || defined(__AVR_ATmega48PA__) || \
      defined(__AVR_ATmega88__)   || defined(__AVR_ATmega88A__)  || \
      defined(__AVR_ATmega88P__)  || defined(__AVR_ATmega88PA__) || \
      defined(__AVR_ATmega168__)  || defined(__AVR_ATmega168A__) || \
      defined(__AVR_ATmega168P__) || defined(__AVR_ATmega168PA__) || \
      defined(__AVR_ATmega328__)  || defined(__AVR_ATmega328P__)
    /* ---------- ATmega48/88/168/328 family (and A/P/PA variants) ----------
     * These share the same Timer0 CTC peripheral (OCR0A/TIMSK0/OCIE0A) and
     * port layout, so one code path covers the whole family.
     * F_CPU = 8 MHz, prescaler = 8, CTC
     * OCR0A = 249 -> (249 + 1) * 1 us = 250 us (~0.25 ms) per tick
     * IDLE_TIMEOUT_TICKS x 250 us = 1.5 ms timeout
     */
    TCCR0A = (1 << WGM01);               /* CTC */
    TCCR0B = (1 << CS01);                /* clk/8 */
    TCNT0  = 0;                          /* initialize counter */
    OCR0A  = 249;
    TIMSK0 = (1 << OCIE0A);
#else
#  error "Unsupported MCU: expected ATmega8 or ATmega48/88/168/328 (incl. A/P/PA variants)"
#endif

    sei();

    while (1) {
        /* Sample TH exactly once per iteration so the phase advance and the
         * data output always correspond to the same instant (no torn read
         * across a fast TH edge). */
        uint8_t th = th_level();
        sega_poll_th(th);
        sega_output(th);
    }
}
