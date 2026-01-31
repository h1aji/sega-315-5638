/*
 * Sega Mega Drive / Genesis controller
 * 3-button + proper 6-button extension
 * Emulates SEGA 315-5638 chip
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
    return (PINB & (1 << PB7)) ? 1 : 0;
}

/* ================= Timer0 overflow (~1 ms) ================= */

#if defined(__AVR_ATmega8__)
ISR(TIMER0_OVF_vect)
#else
ISR(TIMER0_COMPA_vect)
#endif
{
    /* Increment idleTicks atomically */
    uint8_t ticks = idleTicks;
    ticks++;
    idleTicks = ticks;

    if (ticks > 12) {
        /* Disable interrupts to prevent race condition with main loop */
        cli();
        phase = 0;
        isSix = 0;     /* fallback to 3-button */
        sei();
    }
}

/* ================= TH polling ================= */

static inline void sega_poll_th(void)
{
    uint8_t th = th_level();

    if (th != prevTH) {
        prevTH = th;
        idleTicks = 0;

        /* Disable interrupts to prevent race condition with ISR */
        cli();
        uint8_t p = phase;
        if (++p >= 8) {
            p = 0;
            isSix = 1;   /* successful 6-button detect */
        }
        phase = p;
        sei();
    }
}

/* ================= Sega output ================= */

static inline void sega_output(void)
{
    uint8_t th = th_level();

    /* Release all lines first */
    writeHi(PD2); writeHi(PD3); writeHi(PD4);
    writeHi(PD5); writeHi(PD6); writeHi(PD7);

    /* ---------- 3 BUTTON MODE ---------- */
    if (!isSix) {
        /* UP and DOWN are ALWAYS available */
        BTN_UP()   ? writeLo(PD2) : writeHi(PD2);
        BTN_DOWN() ? writeLo(PD3) : writeHi(PD3);
        
        if (th) {
            /* TH HIGH: directional left/right + B/C */
            BTN_LEFT()  ? writeLo(PD4) : writeHi(PD4);
            BTN_RIGHT() ? writeLo(PD5) : writeHi(PD5);
            BTN_B() ? writeLo(PD6) : writeHi(PD6);
            BTN_C() ? writeLo(PD7) : writeHi(PD7);
        } else {
            /* TH LOW: ground on pins 3-4 + A/START */
            writeLo(PD4); /* Ground on pin 3 */
            writeLo(PD5); /* Ground on pin 4 */
            BTN_A()     ? writeLo(PD6) : writeHi(PD6);
            BTN_START() ? writeLo(PD7) : writeHi(PD7);
        }
        return;
    }

    /* ---------- 6 BUTTON MODE ---------- */
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
    
    switch (phase) {

    case 0: // Pulse 1 LOW
    case 2: // Pulse 2 LOW
        /* UP and DOWN always available */
        BTN_UP()   ? writeLo(PD2) : writeHi(PD2);
        BTN_DOWN() ? writeLo(PD3) : writeHi(PD3);
        /* LEFT/RIGHT grounded for 3-button compatibility */
        writeLo(PD4);
        writeLo(PD5);
        /* A and START available */
        BTN_A()     ? writeLo(PD6) : writeHi(PD6);
        BTN_START() ? writeLo(PD7) : writeHi(PD7);
        break;

    case 1: // Pulse 1 HIGH
    case 3: // Pulse 2 HIGH
        /* Normal directional + B/C */
        BTN_UP()    ? writeLo(PD2) : writeHi(PD2);
        BTN_DOWN()  ? writeLo(PD3) : writeHi(PD3);
        BTN_LEFT()  ? writeLo(PD4) : writeHi(PD4);
        BTN_RIGHT() ? writeLo(PD5) : writeHi(PD5);
        BTN_B()     ? writeLo(PD6) : writeHi(PD6);
        BTN_C()     ? writeLo(PD7) : writeHi(PD7);
        break;

    case 4: // Pulse 3 LOW - IDENTIFICATION PHASE
        /* ALL FOUR directional pins LOW for 6-button identification
         * This tells the console we are a 6-button controller */
        writeLo(PD2); // UP = LOW
        writeLo(PD3); // DOWN = LOW
        writeLo(PD4); // LEFT = LOW
        writeLo(PD5); // RIGHT = LOW
        /* A and START still available */
        BTN_A()     ? writeLo(PD6) : writeHi(PD6);
        BTN_START() ? writeLo(PD7) : writeHi(PD7);
        break;

    case 5: // Pulse 3 HIGH - X/Y/Z/MODE READABLE
        /* X/Y/Z on directional pins, MODE on RIGHT pin */
        BTN_Z()    ? writeLo(PD2) : writeHi(PD2);  // Z on UP pin (PIN 1)
        BTN_Y()    ? writeLo(PD3) : writeHi(PD3);  // Y on DOWN pin (PIN 2)
        BTN_X()    ? writeLo(PD4) : writeHi(PD4);  // X on LEFT pin (PIN 3)
        BTN_MODE() ? writeLo(PD5) : writeHi(PD5);  // MODE on RIGHT pin (PIN 4)
        BTN_B()    ? writeLo(PD6) : writeHi(PD6);  // B still available (PIN 6)
        BTN_C()    ? writeLo(PD7) : writeHi(PD7);  // C still available (PIN 9)
        break;

    case 6: // Pulse 4 LOW - Reset phase
        /* UP and DOWN available again */
        BTN_UP()   ? writeLo(PD2) : writeHi(PD2);
        BTN_DOWN() ? writeLo(PD3) : writeHi(PD3);
        /* LEFT/RIGHT grounded */
        writeLo(PD4);
        writeLo(PD5);
        /* A and START available */
        BTN_A()     ? writeLo(PD6) : writeHi(PD6);
        BTN_START() ? writeLo(PD7) : writeHi(PD7);
        break;

    case 7: // Pulse 4 HIGH - Back to idle/normal
        /* Back to normal state - same as phases 1 and 3 */
        BTN_UP()    ? writeLo(PD2) : writeHi(PD2);
        BTN_DOWN()  ? writeLo(PD3) : writeHi(PD3);
        BTN_LEFT()  ? writeLo(PD4) : writeHi(PD4);
        BTN_RIGHT() ? writeLo(PD5) : writeHi(PD5);
        BTN_B()     ? writeLo(PD6) : writeHi(PD6);
        BTN_C()     ? writeLo(PD7) : writeHi(PD7);
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
    /* F_CPU = 8 MHz
     * Prescaler = 64
     * Overflow period ≈ 2.048 ms
     * idleTicks > 12 gives ~24.6 ms timeout
     */

    TCCR0 = (1 << CS01) | (1 << CS00);   /* clk/64 */
    TCNT0 = 0;                           /* initialize counter */
    TIMSK |= (1 << TOIE0);               /* enable overflow interrupt */
#elif defined(__AVR_ATmega88__) || defined(__AVR_ATmega88A__) || defined(__AVR_ATmega88PA__)
    /* ---------- ATmega88 / ATmega88A / ATmega88PA / ATmega88PB ---------- */
    /* F_CPU = 8 MHz
    * Prescaler = 64
    * OCR0A = 124 → 1 ms
    */

    TCCR0A = (1 << WGM01);               /* CTC */
    TCCR0B = (1 << CS01) | (1 << CS00);  /* clk/64 */
    TCNT0  = 0;                          /* initialize counter */
    OCR0A  = 124;
    TIMSK0 = (1 << OCIE0A);
#endif

    sei();

    while (1) {
        sega_poll_th();
        sega_output();
    }
}
