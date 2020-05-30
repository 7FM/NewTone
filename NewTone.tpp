// ---------------------------------------------------------------------------
// Created by Tim Eckel - teckel@leethost.com
// Copyright 2013 License: GNU GPL v3 http://www.gnu.org/licenses/gpl-3.0.html
//
// See "NewTone.h" for purpose, syntax, version history, links, and more.
// ---------------------------------------------------------------------------

#include <FastPin.h>

#ifndef NEWTONE_USE_TIMER2
#define TIMER_EN_REG TIMSK1
#define TIMER_COMPARE_REG ICR1
#define TIMER_COUNT_REG TCNT1
#define TIMER_PRESCALE_REG TCCR1B
#define TIMER_MODE_REG TCCR1A

#define TIMER_MODE _BV(COM1B0)
#define TIMER_MODE_2 _BV(WGM13)
#define TIMER_EN_MASK _BV(OCIE1A)

#define TIMER_PRESCALE_RESET _BV(CS11) | _BV(CS10)
#define TIMER_MODE_RESET _BV(WGM10)

#define TIMER_INTERRUPT_VECTOR TIMER1_COMPA_vect
#else
#define TIMER_EN_REG TIMSK2
#define TIMER_MODE_REG TCCR2A
#define TIMER_COMPARE_REG OCR2A
#define TIMER_PRESCALE_REG TCCR2B
#define TIMER_COUNT_REG TCNT2

#define TIMER_MODE _BV(WGM20)
#define TIMER_MODE_2 _BV(WGM22)
#define TIMER_EN_MASK _BV(OCIE2A)

#define TIMER_PRESCALE_RESET _BV(CS22)
#define TIMER_MODE_RESET _BV(WGM20)

#define TIMER_INTERRUPT_VECTOR TIMER2_COMPA_vect
#endif

unsigned long _nt_time; // Time note should end.
#ifndef NEWTONE_STATIC_PIN
uint8_t _pinMask = 0;         // Pin bitmask.
volatile uint8_t *_pinOutput; // Output port register

template <uint8_t NEWTONE_PIN>
#endif
void NewTone(unsigned long frequency, unsigned long length) {
    uint8_t prescaler;
    uint32_t top;
#ifndef NEWTONE_USE_TIMER2
    prescaler = _BV(CS10);       // Try using prescaler 1 first.
    top = F_CPU / frequency / 4; // Calculate the top.
    if (top > 65536) {           // If not in the range for prescaler 1, use prescaler 256 (61 Hz and lower @ 16 MHz).
        prescaler = _BV(CS12);   // Set the 256 prescaler bit.
        top = top / 256;         // Calculate the top using prescaler 256.
    }

    --top;
#else
    // if we are using an 8 bit timer, scan through prescalars to find the best fit
    top = F_CPU / frequency / 4;
    prescaler = 0b001; // ck/1: same for both timers
    if (top > 256) {
        top >>= 3;         // try prescale /8
        prescaler = 0b010; // ck/8: same for both timers

        if (top > 256) {
            top >>= 2; // try prescale /32 (8*4)
            prescaler = 0b011;

            if (top > 256) {
                top >>= 1; // try prescale /64 (32*2)
                prescaler = 0b100;

                if (top > 256) {
                    top >>= 1; // try prescale /128 (64*2)
                    prescaler = 0b101;

                    if (top > 256) {
                        top >>= 1; // try prescale /256 (128*2)
                        prescaler = 0b110;
                        if (top > 256) {
                            // can't do any better than /1024
                            top >>= 2; // try prescale /1024 (256*4)
                            prescaler = 0b111;

                            if (top > 256) {
                                top = 256; // closes fit
                            }
                        }
                    }
                }
            }
        }
    }

    --top;
#endif

    if (length > 0)
        _nt_time = millis() + length;
    else
        _nt_time = 0xFFFFFFFF; // Set when the note should end, or play "forever".

    FastPin<NEWTONE_PIN>::setOutput();
#ifndef NEWTONE_STATIC_PIN
    _pinMask = FastPin<NEWTONE_PIN>::mask();   // Get the port register bitmask for the pin.
    _pinOutput = FastPin<NEWTONE_PIN>::port(); // Get the output port register for the pin.
#endif

    TIMER_COMPARE_REG = top; // Set the top.
    if (TIMER_COUNT_REG > top)
        TIMER_COUNT_REG = top;                     // Counter over the top, put within range.
    TIMER_PRESCALE_REG = TIMER_MODE_2 | prescaler; // Set PWM, phase and frequency corrected (ICR1) and prescaler.
    TIMER_MODE_REG = TIMER_MODE;
    TIMER_EN_REG |= TIMER_EN_MASK; // Activate the timer interrupt.
}

void noNewTone() {
    TIMER_EN_REG &= ~TIMER_EN_MASK;            // Remove the timer interrupt.
    TIMER_PRESCALE_REG = TIMER_PRESCALE_RESET; // Default clock prescaler of 8.
    TIMER_MODE_REG = TIMER_MODE_RESET;         // Set to defaults so PWM can work like normal (PWM, phase corrected, 8bit).

#ifndef NEWTONE_STATIC_PIN
    *_pinOutput &= ~_pinMask; // Set pin to LOW.
    _pinMask = 0;             // Flag so we know note is no longer playing.
#else
    FastPin<NEWTONE_PIN>::lo();
#endif
}

ISR(TIMER_INTERRUPT_VECTOR) { // Timer interrupt vector.
    if (millis() >= _nt_time)
        noNewTone(); // Check to see if it's time for the note to end.
#ifndef NEWTONE_STATIC_PIN
    *_pinOutput ^= _pinMask; // Toggle the pin state.
#else
    *FastPin<NEWTONE_PIN>::port() ^= FastPin<NEWTONE_PIN>::mask(); // Toggle the pin state.
#endif
}