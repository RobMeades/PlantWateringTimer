/*
 * File:   main.c
 * Author: Rob Meades
 *
 * Created on 05 March 2016, 13:25
 */

#include <xc.h>
#include "usb\usb_device.h"
#include "usb\usb_device_cdc.h"

/********************************************************
 * MACROS
 *******************************************************/

// 3 days at a watchdog timer of 256 seconds
#define WATCHDOG_COUNT_MAX    1000

#define MOTOR_PIN_LAT         LATAbits.LATA5 
#define SWITCH_PIN_LAT        LATAbits.LATA4 
#define MOTOR_PIN_PORT        PORTAbits.RA5 
#define SWITCH_PIN_PORT       PORTAbits.RA4 
#define MOTOR_PIN_TRIS        TRISAbits.TRISA5 
#define SWITCH_PIN_TRIS       TRISAbits.TRISA4 
#define SWITCH_PIN_WPU        WPUAbits.WPUA4
#define SWITCH_PIN_INT_EN_POS IOCAPbits.IOCAP4 
#define SWITCH_PIN_INT_EN_NEG IOCANbits.IOCAN4
// This is deliberately all bits so that we clear
// all IO interrupt sources, in case of spurious ones
#define SWITCH_PIN_INT_FLAG   IOCAF

#define DEBOUNCE_PERIOD_MS    100

/********************************************************
 * PRIVATE VARIABLES
 *******************************************************/

static uint8_t readBuffer[CDC_DATA_OUT_EP_SIZE];
static uint8_t writeBuffer[CDC_DATA_IN_EP_SIZE];

/********************************************************
 * STATIC FUNCTION PROTOTYPES
 *******************************************************/

static void waitMs(uint32_t milliseconds);
static void waitMsForSwitch(uint32_t milliseconds);
static void appInit();
static void appMain(void);

/********************************************************
 * STATIC FUNCTIONS
 *******************************************************/

/* Wait for a number of milliseconds */
static void waitMs(uint32_t milliseconds)
{
    /* Set up Timer0 to run from instruction cycles with 64 prescaler */
    OPTION_REGbits.TMR0CS = 0;
    OPTION_REGbits.PS = 0x5;
    OPTION_REGbits.PSA = 0;

    for (uint32_t x = 0; x < milliseconds; x++)
    {
        TMR0bits.TMR0 = 0;
        INTCONbits.TMR0IF = 0;
        while (!INTCONbits.TMR0IF) {};
        INTCONbits.TMR0IF = 0;
    }
}

/* Wait for a number of milliseconds (approximate) or until 
 * SWITCH_PIN_INT_FLAG is set */
static void waitMsForSwitch(uint32_t milliseconds)
{
    /* Set up Timer0 to run from instruction cycles with 64 x prescaler */
    OPTION_REGbits.TMR0CS = 0;
    OPTION_REGbits.PS = 0x5;
    OPTION_REGbits.PSA = 0;

    /* Clear the switch interrupt flag and enable the interrupt */
    SWITCH_PIN_INT_FLAG = 0;
    INTCONbits.IOCIE = 1;
    for (uint32_t x = 0; (x < milliseconds) && !SWITCH_PIN_INT_FLAG; x++)
    {
        TMR0bits.TMR0 = 0;
        INTCONbits.TMR0IF = 0;
        while (!INTCONbits.TMR0IF) {};
        INTCONbits.TMR0IF = 0;
    }
    
    /* Disable the interrupt */
    INTCONbits.IOCIE = 0;   
}

/* Initialise the application code */
static void appInit()
{
    CDCInitEP();
}

/* The application entry point */
static void appMain(void)
{
    if (USBUSARTIsTxTrfReady())
    {
        uint8_t i;
        uint8_t numBytesRead;

        numBytesRead = getsUSBUSART(readBuffer, sizeof(readBuffer));

        /* For every byte that was read... */
        for (i = 0; i < numBytesRead; i++)
        {
            switch(readBuffer[i])
            {
                /* If we receive new line or line feed commands, just echo
                 * them direct.
                 */
                case 0x0A:
                case 0x0D:
                    writeBuffer[i] = readBuffer[i];
                break;

                /* If we receive something else, then echo it plus one
                 * so that if we receive 'a', we echo 'b' so that the
                 * user knows that it isn't the echo enabled on their
                 * terminal program.
                 */
                default:
                    writeBuffer[i] = readBuffer[i] + 1;
                break;
            }
        }

        if (numBytesRead > 0)
        {
            /* After processing all of the received data, we need to send out
             * the "echo" data now.
             */
            putUSBUSART(writeBuffer, numBytesRead);
        }
    }
    
    CDCTxService();
}

/********************************************************
 * PUBLIC FUNCTIONS
 *******************************************************/

/* This function is called from the USB stack to notify a user application
 * that a USB event occurred.  This callback is in interrupt context
 * when USB_INTERRUPT is defined. */
bool USER_USB_CALLBACK_EVENT_HANDLER(USB_EVENT event, void *pdata, uint16_t size)
{
    switch ((int) event)
    {
        case EVENT_TRANSFER:
        break;

        case EVENT_SOF:
        break;

        case EVENT_SUSPEND:
            /* Call the hardware platform specific handler for suspend events for
             * possible further action (like optionally going reconfiguring the application
             * for lower power states and going to sleep during the suspend event).  This
             * would normally be done in USB compliant bus powered applications, although
             * no further processing is needed for purely self powered applications that
             * don't consume power from the host.
             */
            SYSTEM_Initialize(SYSTEM_STATE_USB_SUSPEND);
        break;

        case EVENT_RESUME:
            /* Call the hardware platform specific resume from suspend handler (ex: to
             * restore I/O pins to higher power states if they were changed during the 
             * preceding SYSTEM_Initialize(SYSTEM_STATE_USB_SUSPEND) call at the start
             * of the suspend condition.
             */
            SYSTEM_Initialize(SYSTEM_STATE_USB_RESUME);
        break;

        case EVENT_CONFIGURED:
            /* When the device is configured, we can (re)initialize the 
             * demo code. */
            appInit();
        break;

        case EVENT_SET_DESCRIPTOR:
        break;

        case EVENT_EP0_REQUEST:
            /* We have received a non-standard USB request.  The HID driver
             * needs to check to see if the request was for it. */
            USBCheckCDCRequest();
        break;

        case EVENT_BUS_ERROR:
        break;

        case EVENT_TRANSFER_TERMINATED:
        break;

        default:
        break;
    }
    
    return true;
}

/* Main */
void main(void)
{
    /* Make sure RA4/RA5 are used for digital */
    ANSELAbits.ANSELA = 0;
    
    /* Set unused pins to be outputs */
    LATCbits.LATC2 = 0;
    TRISCbits.TRISC2 = 0;
    LATCbits.LATC3 = 0;
    TRISCbits.TRISC4 = 0;
    LATCbits.LATC4 = 0;
    TRISCbits.TRISC4 = 0;
    LATCbits.LATC5 = 0;
    TRISCbits.TRISC5 = 0;
    
    /* Enable pull-ups */
    OPTION_REGbits.nWPUEN = 0;
    
    /* Motor pin is an output */
    MOTOR_PIN_PORT = 0;
    MOTOR_PIN_LAT = 0;
    MOTOR_PIN_TRIS = 0;
    
    /* Microswitch pin is an input with weak pull-up */
    SWITCH_PIN_PORT = 0;
    SWITCH_PIN_LAT = 0;
    SWITCH_PIN_WPU = 1;
    SWITCH_PIN_TRIS = 1;
    
    /* Watchdog 256 seconds */
    WDTCONbits.WDTPS = 0x12;
    
    /* Setup interrupts on the switch changing, both edges
     * but don't enable them yet */
    INTCONbits.INTE = 0;
    INTCONbits.GIE = 1;
    SWITCH_PIN_INT_EN_NEG = 1;
    SWITCH_PIN_INT_EN_POS = 1;

    /* Set up clocks */
    SYSTEM_Initialize(SYSTEM_STATE_USB_START);

    while (1)
    {
        /* Wait for the right number of watchdog expirations */
        for (uint32_t x = 0; x < WATCHDOG_COUNT_MAX; x++)
        {
            WDTCONbits.SWDTEN = 1;
            SLEEP();
            NOP();
            WDTCONbits.SWDTEN = 0;
        }

        /* Switch on the motor for 150 ms or until the switch GPIO goes off */
        MOTOR_PIN_LAT = 1;
        waitMsForSwitch(150);
        MOTOR_PIN_LAT = 0;
        /* Debounce the switch, which should have been pressed by now */
        waitMs(DEBOUNCE_PERIOD_MS);
        
        /* If we get here the switch should have been closed by the motor rotation.
         * The next thing that matters is the interrupt going off when
         * the switch is released after I've watered the plants, which will take
         * us around the loop again */
        /* Clear the switch interrupt flag and enable the interrupt */
        SWITCH_PIN_INT_FLAG = 0;
        INTCONbits.IOCIE = 1;
        /* Go to sleep until interrupted */
        SLEEP();
        NOP();
        /* Disable the interrupt and debounce */
        INTCONbits.IOCIE = 0;   
        waitMs(DEBOUNCE_PERIOD_MS);
    }
    
    /* We don't actually need USB at all, this just kept
     * in the code in case I need to send printf()s for debug at some point */
    
    USBDeviceInit();
    USBDeviceAttach();
    
    while (1)
    {
        /* If the USB device is configured and not suspended then do
         * application stuff */
        if ((USBGetDeviceState() >= CONFIGURED_STATE) && !USBIsDeviceSuspended())
        {
            appMain();
        }
    }
}