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

#define LED_PIN_LAT     LATAbits.LATA5 
#define BUTTON_PIN_LAT  LATAbits.LATA4 
#define LED_PIN_PORT     PORTAbits.RA5 
#define BUTTON_PIN_PORT  PORTAbits.RA4 
#define LED_PIN_TRIS     TRISAbits.TRISA5 
#define BUTTON_PIN_TRIS  TRISAbits.TRISA4 
#define BUTTON_PIN_WPU  WPUAbits.WPUA4

/********************************************************
 * PRIVATE VARIABLES
 *******************************************************/

static uint8_t readBuffer[CDC_DATA_OUT_EP_SIZE];
static uint8_t writeBuffer[CDC_DATA_IN_EP_SIZE];
static uint32_t wakeupCount = 0;

/********************************************************
 * STATIC FUNCTION PROTOTYPES
 *******************************************************/

static void waitMs(uint32_t milliseconds);
static void appInit();
static void appMain(void);

/********************************************************
 * STATIC FUNCTIONS
 *******************************************************/

/* Wait for a number of milliseconds (approximate) */
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
    
    /* Set unused pins to be inputs */
    LATCbits.LATC2 = 0;
    TRISCbits.TRISC2 = 1;
    LATCbits.LATC3 = 0;
    TRISCbits.TRISC4 = 1;
    LATCbits.LATC4 = 0;
    TRISCbits.TRISC4 = 1;
    LATCbits.LATC5 = 0;
    TRISCbits.TRISC5 = 1;
    
    /* Enable pull-ups */
    OPTION_REGbits.nWPUEN = 0;
    
    /* LED pin is an output */
    LED_PIN_PORT = 0;
    LED_PIN_LAT = 0;
    LED_PIN_TRIS = 0;
    
    /* Button pin is an input with weak pull-up */
    BUTTON_PIN_PORT = 0;
    BUTTON_PIN_LAT = 0;
    BUTTON_PIN_WPU = 1;
    BUTTON_PIN_TRIS = 1;
    
    /* Set up the watchdog timer to be software controlled */
    WDTCONbits.SWDTEN = 0;

    while (1)
    {
        WDTCONbits.WDTPS = 0x0c;
        WDTCONbits.SWDTEN = 1;

        SLEEP();
        WDTCONbits.SWDTEN = 0;
        wakeupCount++;

        SYSTEM_Initialize(SYSTEM_STATE_USB_START);

        if (BUTTON_PIN_PORT)
        {
            LED_PIN_LAT = 1;
            waitMs(150);
            LED_PIN_LAT = 0;
        }
    }
    
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