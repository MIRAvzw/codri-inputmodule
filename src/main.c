//
// Configuration
//

// Includes
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "usbdrv.h"

// Utility macro's
#define UTIL_BIN4(x)        (uchar)((0##x & 01000)/64 + (0##x & 0100)/16 + (0##x & 010)/4 + (0##x & 1))
#define UTIL_BIN8(hi, lo)   (uchar)(UTIL_BIN4(hi) * 16 + UTIL_BIN4(lo))

// NULL definition
#ifndef NULL
#define NULL    ((void *)0)
#endif

// Static data
static uchar    reportBuffer[2];    /* buffer for HID reports */
static uchar    idleRate;           /* in 4 ms units */


//
// Key handling
//

static void keysInit(void)
{
    PORTB = (1<<PB1) | (1<<PB3) | (1<<PB4); /* active necessary pull-ups*/
    //DDRB = (1<DDB0) | (1<<DDB2) | (1<DDB5); /* all pins input, except USB lines */
}

/* Keyboard usage values, see usb.org's HID-usage-tables document, chapter
 * 10 Keyboard/Keypad Page for more codes.
 */
#define MOD_CONTROL_LEFT    (1<<0)
#define MOD_SHIFT_LEFT      (1<<1)
#define MOD_ALT_LEFT        (1<<2)
#define MOD_GUI_LEFT        (1<<3)
#define MOD_CONTROL_RIGHT   (1<<4)
#define MOD_SHIFT_RIGHT     (1<<5)
#define MOD_ALT_RIGHT       (1<<6)
#define MOD_GUI_RIGHT       (1<<7)

#define KEY_1       30
#define KEY_2       31
#define KEY_3       32
#define KEY_4       33
#define KEY_5       34
#define KEY_6       35
#define KEY_7       36
#define KEY_8       37
#define KEY_9       38
#define KEY_0       39
#define KEY_RETURN  40
#define KEY_RIGHT   79
#define KEY_LEFT    80
#define KEY_DOWN    81
#define KEY_UP      82

#define NUM_KEYS    7

static const uchar keyReport[NUM_KEYS + 1][2] PROGMEM = {
/* none */  {0, 0},                     /* no key pressed */
/*  1 */    {0, KEY_LEFT},
/*  2 */    {0, KEY_RIGHT},
/*  6 */    {0, 0},             // broken
/*  4 */    {0, KEY_DOWN},
/*  5 */    {0, 0},             // broken
/*  3 */    {0, KEY_UP},
/*  7 */    {0, KEY_RETURN},
};

static uchar keyPoll(void)
{
    uchar x, key = 0;
    
    // Read out the three multiplexed input lines
    x = PINB;
    if ((x & (1<<PB1)) == 0)
        key += 1<<0;
    if ((x & (1<<PB3)) == 0)        
        key += 1<<1;
    if ((x & (1<<PB4)) == 0)        
        key += 1<<2;
    
    return key;
}


//
// Timer handling
//

static void timerInit(void)
{
    TCCR1 = 0x0b;           /* select clock: 16.5M/1k -> overflow rate = 16.5M/256k = 62.94 Hz */
}


//
// Oscillator calibration
//

/* Calibrate the RC oscillator to 8.25 MHz. The core clock of 16.5 MHz is
 * derived from the 66 MHz peripheral clock by dividing. Our timing reference
 * is the Start Of Frame signal (a single SE0 bit) available immediately after
 * a USB RESET. We first do a binary search for the OSCCAL value and then
 * optimize this value with a neighboorhod search.
 * This algorithm may also be used to calibrate the RC oscillator directly to
 * 12 MHz (no PLL involved, can therefore be used on almost ALL AVRs), but this
 * is wide outside the spec for the OSCCAL value and the required precision for
 * the 12 MHz clock! Use the RC oscillator calibrated to 12 MHz for
 * experimental purposes only!
 */
static void calibrateOscillator(void)
{
    uchar step = 128;
    uchar trialValue = 0, optimumValue;
    int x, optimumDev, targetValue = (unsigned)(1499 * (double)F_CPU / 10.5e6 + 0.5);

    /* do a binary search: */
    do {
        OSCCAL = trialValue + step;
        x = usbMeasureFrameLength();    /* proportional to current real frequency */
        if (x < targetValue)             /* frequency still too low */
            trialValue += step;
        step >>= 1;
    } while(step > 0);
    /* We have a precision of +/- 1 for optimum OSCCAL here */
    /* now do a neighborhood search for optimum value */
    optimumValue = trialValue;
    optimumDev = x; /* this is certainly far away from optimum */
    for(OSCCAL = trialValue - 1; OSCCAL <= trialValue + 1; OSCCAL++)
    {
        x = usbMeasureFrameLength() - targetValue;
        if (x < 0)
            x = -x;
        if (x < optimumDev)
        {
            optimumDev = x;
            optimumValue = OSCCAL;
        }
    }
    OSCCAL = optimumValue;
}
/*
Note: This calibration algorithm may try OSCCAL values of up to 192 even if
the optimum value is far below 192. It may therefore exceed the allowed clock
frequency of the CPU in low voltage designs!
You may replace this search algorithm with any other algorithm you like if
you have additional constraints such as a maximum CPU clock.
For version 5.x RC oscillators (those with a split range of 2x128 steps, e.g.
ATTiny25, ATTiny45, ATTiny85), it may be useful to search for the optimum in
both regions.
*/


//
// USB interface
//

PROGMEM char const usbHidReportDescriptor[USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH] = { /* USB report descriptor */
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,                    // USAGE (Keyboard)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
    0x19, 0xe0,                    //   USAGE_MINIMUM (Keyboard LeftControl)
    0x29, 0xe7,                    //   USAGE_MAXIMUM (Keyboard Right GUI)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x25, 0x65,                    //   LOGICAL_MAXIMUM (101)
    0x19, 0x00,                    //   USAGE_MINIMUM (Reserved (no event indicated))
    0x29, 0x65,                    //   USAGE_MAXIMUM (Keyboard Application)
    0x81, 0x00,                    //   INPUT (Data,Ary,Abs)
    0xc0                           // END_COLLECTION
};
/* We use a simplifed keyboard report descriptor which does not support the
 * boot protocol. We don't allow setting status LEDs and we only allow one
 * simultaneous key press (except modifiers). We can therefore use short
 * 2 byte input reports.
 * The report descriptor has been created with usb.org's "HID Descriptor Tool"
 * which can be downloaded from http://www.usb.org/developers/hidpage/.
 * Redundant entries (such as LOGICAL_MINIMUM and USAGE_PAGE) have been omitted
 * for the second INPUT item.
 */

static void buildReport(uchar key)
{
    // This (not so elegant) cast saves us 10 bytes of program memory
    *(int *)reportBuffer = pgm_read_word(keyReport[key]);
}

uchar	usbFunctionSetup(uchar data[8])
{
    usbRequest_t *rq = (void *)data;

    usbMsgPtr = reportBuffer;
    if ((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS)    // class request type
    {
        if (rq->bRequest == USBRQ_HID_GET_REPORT)  // wValue: ReportType (highbyte), ReportID (lowbyte)
        {
            // we only have one report type, so don't look at wValue
            buildReport(keyPoll());
            return sizeof(reportBuffer);
        }
        else if (rq->bRequest == USBRQ_HID_GET_IDLE)
        {
            usbMsgPtr = &idleRate;
            return 1;
        }
        else if (rq->bRequest == USBRQ_HID_SET_IDLE)
        {
            idleRate = rq->wValue.bytes[1];
        }
    }
    else
    {
        // no vendor specific requests implemented
    }
    return 0;
}

void usbEventResetReady(void)
{
    calibrateOscillator();
    eeprom_write_byte(0, OSCCAL);   // store the calibrated value in EEPROM
}


//
// Main
//

int main(void)
{
    uchar i;
    uchar calibrationValue;
    uchar key, lastKey = 0, keyDidChange = 0;
    uchar idleCounter = 0;

    // Calibrate the clock
    calibrationValue = eeprom_read_byte(0); // calibration value from last time
    if (calibrationValue != 0xff)
    {
        OSCCAL = calibrationValue;
    }

    // Connect the USB
    usbDeviceDisconnect();
    for (i=0;i<20;i++)  // 300 ms disconnect
    {
        _delay_ms(15);
    }
    usbDeviceConnect();
    wdt_enable(WDTO_1S);

    // Initialize hardware perhiperals
    timerInit();
    keysInit();
    usbInit();
    sei();

    // Main loop
    for (;;)
    {
        wdt_reset();
        usbPoll();
        
        key = keyPoll();
        if (lastKey != key)
        {
            lastKey = key;
            keyDidChange = 1;
        }
        
        if (TIFR & (1 << TOV1)) // 16 ms timer
        {
            TIFR = (1 << TOV1); // clear overflow
            keyPoll();
            if (idleRate != 0)
            {
                if (idleCounter > 3)
                {
                    idleCounter -= 4;   // 16 ms in units of 4 ms
                }
                else
                {
                    idleCounter = idleRate;
                    keyDidChange = 1;
                }
            }
        }
        
        if (keyDidChange && usbInterruptIsReady())
        {
            keyDidChange = 0;
            // use last key and not current key status in order to avoid lost changes in key status.
            buildReport(lastKey);
            usbSetInterrupt(reportBuffer, sizeof(reportBuffer));
        }
    }
    return 0;
}
