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
 * PRIVATE VARIABLES
 *******************************************************/

static uint8_t readBuffer[CDC_DATA_OUT_EP_SIZE];
static uint8_t writeBuffer[CDC_DATA_IN_EP_SIZE];

/********************************************************
 * STATIC FUNCTION PROTOTYPES
 *******************************************************/

static void appInit();
static void appMain(void);

/********************************************************
 * STATIC FUNCTIONS
 *******************************************************/

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
        for(i=0; i<numBytesRead; i++)
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

        if(numBytesRead > 0)
        {
            /* After processing all of the received data, we need to send out
             * the "echo" data now.
             */
            putUSBUSART(writeBuffer,numBytesRead);
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
    switch( (int) event )
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
    SYSTEM_Initialize(SYSTEM_STATE_USB_START);

    USBDeviceInit();
    USBDeviceAttach();
    
    while(1)
    {
        SYSTEM_Tasks();

#if defined(USB_POLLING)
        USBDeviceTasks();
#endif

        /* If the USB device is configured and not suspended then do
         * application stuff */
        if( USBGetDeviceState() >= CONFIGURED_STATE && !USBIsDeviceSuspended())
        {
            appMain();
        }
    }
}