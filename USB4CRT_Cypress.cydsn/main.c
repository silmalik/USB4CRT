/*******************************************************************************
* File Name: main.c
*
* Version: 2.0
*
* Description:
*   The component is enumerated as a Virtual Com port. Receives data from the 
*   hyper terminal, then sends back the received data.
*   For PSoC3/PSoC5LP.
*
* Related Document:
*  Universal Serial Bus Specification Revision 2.0
*  Universal Serial Bus Class Definitions for Communications Devices
*  Revision 1.2
*
********************************************************************************
* Copyright 2015, Cypress Semiconductor Corporation. All rights reserved.
* This software is owned by Cypress Semiconductor Corporation and is protected
* by and subject to worldwide patent and copyright laws and treaties.
* Therefore, you may use this software only as provided in the license agreement
* accompanying the software package from which you obtained this software.
* CYPRESS AND ITS SUPPLIERS MAKE NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* WITH REGARD TO THIS SOFTWARE, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT,
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
*******************************************************************************
* V3
* Mono command - ok at 80%
*/


#include <project.h>
#include "stdio.h"

#if defined (__GNUC__)
    /* Add an explicit reference to the floating point printf library */
    /* to allow usage of the floating point conversion specifiers. */
    /* This is not linked in by default with the newlib-nano library. */
    asm (".global _printf_float");
#endif

#define USBFS_DEVICE    (0u)

/* The buffer size is equal to the maximum packet size of the IN and OUT bulk
* endpoints.
*/
#define USBUART_BUFFER_SIZE (64u)
#define LINE_STR_LENGTH     (20u)

char8* parity[] = {"None", "Odd", "Even", "Mark", "Space"};
char8* stop[]   = {"1", "1.5", "2"};

/* PARAM: maximum attempt to write on I2C bus */
const uint8 MaxTry = 8u;

/* initialize I2C bus lock status */
uint8 i2lockstate = 0x00;

/*******************************************************************************
* Functions
********************************************************************************/

// LED BLINK
void blink(int delay,int repeat)
{
    int i;
    for(i = 0; i < repeat; i++){
        LED_Write(1u);
        CyDelay(delay);
        LED_Write(0u);
        CyDelay(delay);
    }
}

// Sent data to PC
void UART_WriteLine(uint8 * buffer,uint8 count,uint8 lineFeed)
{
    while (0u == USBUART_CDCIsReady()){}
    USBUART_PutData(buffer, count); /* Send data to host. */
    if(lineFeed)
    {    
        while (0u == USBUART_CDCIsReady()){}
        USBUART_PutCRLF();
    }
}

// I2C Operation status - START and STOP
void UART_WriteI2startStatus(uint8 status)
{
    switch(status)
    {
        case I2C_MSTR_NO_ERROR:
            UART_WriteLine((uint8 *)" OK",3u,1u);
            return;
        case I2C_MSTR_BUS_BUSY:
            UART_WriteLine((uint8 *)" BUS BUSY",9u,1u);
            return;
        case I2C_MSTR_NOT_READY:
            UART_WriteLine((uint8 *)" NOT READY",10u,1u);
            return;
        case I2C_MSTR_ERR_LB_NAK:
            UART_WriteLine((uint8 *)" LAST BYTE NAK",14u,1u);
            return;
        case I2C_MSTR_ERR_ARB_LOST:
            UART_WriteLine((uint8 *)" ARBITRATION LOST",17u,1u);
            return;
        case I2C_MSTR_ERR_ABORT_START_GEN:
            UART_WriteLine((uint8 *)" ABORDED SLAVE START",20u,1u);
            return;
        default:
            UART_WriteLine((uint8 *)" UNKNOWN ERROR",14u,1u);
    }
}

// I2C Operation status - MASTER
void UART_WriteI2masterStatus(uint8 status)
{
    switch(status)
    {
        case 0x00:
            UART_WriteLine((uint8 *)" OK",3u,1u);
            return;            
        case I2C_MSTAT_RD_CMPLT:
            UART_WriteLine((uint8 *)" READ COMPLETE",14u,1u);
            return;
        case I2C_MSTAT_WR_CMPLT :
            UART_WriteLine((uint8 *)" WRITE COMPLETE",15u,1u);
            return;
        case I2C_MSTAT_XFER_INP:
            UART_WriteLine((uint8 *)" TRANSFER IN PROGRESS",21u,1u);
            return;
        case I2C_MSTAT_XFER_HALT:
            UART_WriteLine((uint8 *)" TRANSFER HALTED",14u,1u);
            return;
        case I2C_MSTAT_ERR_SHORT_XFER:
            UART_WriteLine((uint8 *)" TRANSFER INCOMPLETE",17u,1u);
            return;
        case I2C_MSTAT_ERR_ADDR_NAK:
            UART_WriteLine((uint8 *)" NO SLAVE ACK",20u,1u);
            return;
        case I2C_MSTAT_ERR_ARB_LOST:
            UART_WriteLine((uint8 *)" ARBITRATION LOST",20u,1u);
            return;
        case I2C_MSTAT_ERR_XFER:
            UART_WriteLine((uint8 *)" ABORDED SLAVE START",20u,1u);
            return;
        default:
            UART_WriteLine((uint8 *)" MASTER TRANSFER ERROR",22u,1u);
    }
}

// I2C Operation status - WRITE
void UART_WriteI2writeStatus(uint8 status)
{
    switch(status)
    {
        case I2C_MSTR_NO_ERROR :
            UART_WriteLine((uint8 *)" OK",3u,1u);
            return;
        case I2C_MSTR_NOT_READY :
            UART_WriteLine((uint8 *)" NOT READY",10u,1u);
            return;
        case I2C_MSTR_ERR_LB_NAK:
            UART_WriteLine((uint8 *)" LAST BYTE NACK",15u,1u);
            return;
        case I2C_MSTR_ERR_ARB_LOST:
            UART_WriteLine((uint8 *)" ARBITRATION LOST",17u,1u);
            return;
        default:
            UART_WriteLine((uint8 *)" UNKNOWN ERROR",14u,1u);
    }
}


// ASCII BYTES to INT8
uint8 ascii_value(char8 tmp)
{
        if((tmp >= 0x30) && (tmp <= 0x39)) return (tmp - 0x30);
        if((tmp >= 0x41) && (tmp <= 0x46)) return (tmp - 0x37);
        if((tmp >= 0x61) && (tmp <= 0x66)) return (tmp - 0x57);
        return 0;
}
uint8 ascii_to_int(char8 pfo,char8 pfa){
    uint8 result;
    pfo = ascii_value(pfo);
    pfa = ascii_value(pfa);
    result = ((pfo << 4) + pfa);
    return result;
}



// I2C - Send command data
//     slaveaddress: 7bit right justified address
//     data: array, register followed by data
//     i2stop: if a stop bit should be sent, 0 will lock I2C bus, 1 close communication and free I2C bus
uint8 i2sendcommand(uint16 slaveaddress, uint8 data[64], uint8 datacount, uint8 i2stop,uint8 infoBuffer[64])
{
    uint8 i;
    uint8 infocount = 1u;
    uint8 status;
    I2C_DisableInt(); /* Disable interrupt */
    
    // command STOP only
    if(0u == datacount)
    {
        UART_WriteLine((uint8 *)"I2C - Stop:",12u,0u);
        status = I2C_MasterSendStop();
        UART_WriteI2startStatus(status);
        i2lockstate = 0u;
        return 0u;
    }
    
    // RESET of previous I2C lock state
    if(1u == i2lockstate)
    { //status = I2C_MasterSendStop();
        UART_WriteLine((uint8 *)"I2C - unStop:",14u,0u);
        status = I2C_MasterSendStop();
        UART_WriteI2startStatus(status);
        i2lockstate = 0u;
    }

    // Try to generate start condition 
    uint8 try;
    for(try = 0;try < MaxTry;try++){
        UART_WriteLine((uint8 *)"I2C - Master status:",20u,0u);
        status = I2C_MasterClearStatus();
        UART_WriteI2masterStatus(status);
        
        if(0x00 == status)
        {
            UART_WriteLine((uint8 *)"I2C - Start:",13u,0u);
            status = I2C_MasterSendStart(slaveaddress, I2C_WRITE_XFER_MODE);
            UART_WriteI2startStatus(status);
            if(I2C_MSTR_NO_ERROR == status) break;
        }
        CyDelay(10);
    }
    
    if(I2C_MSTR_NO_ERROR != status)
    {
        UART_WriteLine((uint8 *)"I2C - Command failed!\r\n",23u,0u);
        return 1;
    }

    if (I2C_MSTR_NO_ERROR == status)
    {
        for (i=0; i<datacount; i++)    
        {
            UART_WriteLine((uint8 *)"I2C - Write: ",13u,0u);
            UART_WriteLine(&infoBuffer[infocount+2],1u,0u);
            UART_WriteLine(&infoBuffer[infocount+3],1u,0u);infocount++;infocount++;

            status = I2C_MasterWriteByte(data[i]);
            
            UART_WriteI2writeStatus(status);
        }
    }

    if(i2stop && (I2C_MSTR_NO_ERROR == status))
    {
        UART_WriteLine((uint8 *)"I2C - Stop:",12u,0u);        
        status = I2C_MasterSendStop();
        UART_WriteI2startStatus(status);
        i2lockstate = 0x00;
    }
    else
    {
        i2lockstate = 0x01;
    }
    //I2C_EnableInt(); // Enable interrupt, if it was enabled before
    return 0; 
}

// 2 - PARSE BUFFER for I2C MESSAGES - format and send I2C data
int parseI2Cmessage(uint8 buffer[64],uint8 count)
{
    uint8 i2buffer[64] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    uint8 i2buffercount = 0x00; // i2c data buffer
    uint8 i2slave;              // slave address
    for(uint8 i = 0;i < count;i++)
    {
       switch(buffer[i]){
            case 0x5B:   // [ START
                i2slave = ascii_to_int(buffer[i+1],buffer[i+2]) >> 1;  // Get I2C slave address
                i = i + 0x02;
                break;
            case 0x5D:  // ] STOP
                //if(0u == i2buffercount)
                i2sendcommand(i2slave,i2buffer,i2buffercount,1,buffer);   // send message
                i2buffercount = 0x00; // reset I2C data buffer
                break;
            default:
                i2buffer[i2buffercount] = ascii_to_int(buffer[i],buffer[i + 0x01]);
                i2buffercount++;
                i++;
        }
    }
    if(i2buffercount > 0)  // SEND unstopped message
    {
        i2sendcommand(i2slave,i2buffer,i2buffercount,0u,buffer);
    }
    return 0x00;
}

// 1 - FILTER I2C DATA BUFFER - keep only: 0-9, A-F, [, ]
uint8 processbuffer(uint8 * ibuffer, uint8 icounter)
{
    uint8 obuffer[64];
    uint8 ocounter = 0x00;
    for(uint8 i = 0; i < icounter;i++)
    {
        if((ibuffer[i] >= 0x30) && (ibuffer[i] <= 0x39))
        {
            obuffer[ocounter] = ibuffer[i];
            ocounter++;
        }
        if((ibuffer[i] >= 0x41) && (ibuffer[i] <= 0x46))
        {
            obuffer[ocounter] = ibuffer[i];
            ocounter++;
        }
        if((ibuffer[i] >= 0x61) && (ibuffer[i] <= 0x66))
        {
            obuffer[ocounter] = ibuffer[i];
            ocounter++;
        }
        if(ibuffer[i] == 0x5B || ibuffer[i] == 0x5D)
        {
            obuffer[ocounter] = ibuffer[i];
            ocounter++;
        }
    }
    return parseI2Cmessage(obuffer,ocounter);
}



/*******************************************************************************
* Function Name: main
********************************************************************************/

int main()
{

    
    uint8 count;                               // nb char in input buffer
    uint8 buffer[USBUART_BUFFER_SIZE];         // input buffer interactive 
    uint8 cmdBuffer[USBUART_BUFFER_SIZE];      // command buffer
    uint8 cmdCount = 0u;                       // command char count
    uint8 RSstate;                             // UART transmition state
    
    //uint8 to_send_buffer[USBUART_BUFFER_SIZE];
    //uint16 to_send_index;
    //char8 uart_input[] = "";
    //uint uart_input_size;
    
    CyGlobalIntEnable;

    /* Start USBFS operation with 5-V operation. */
    USBUART_Start(USBFS_DEVICE, USBUART_5V_OPERATION);

    // I2C init
    I2C_Start();
   
    for(;;)
    {
        /* Host can send double SET_INTERFACE request. */
        if (0u != USBUART_IsConfigurationChanged())
        {
            /* Initialize IN endpoints when device is configured. */
            if (0u != USBUART_GetConfiguration())
            {
                /* Enumeration is done, enable OUT endpoint to receive data 
                 * from host. */
                USBUART_CDC_Init();
            }
        }
        
        /* Service USB CDC when device is configured. */
        if (0u != USBUART_GetConfiguration())
        {
            /* Check for input data from host. */
            if (0u != USBUART_DataIsReady())
            {
                /* Read received data and re-enable OUT endpoint. */
                count = USBUART_GetAll(buffer);
                if (0u != count)
                {
                    if(0x0D == buffer[count - 1u]) // add Line Feed for terminal ENTERed commands
                    {
                        buffer[count] = 0x0A;                        
                        count++;
                    }
                    
                    if(buffer[count-1] == 0x0A)    // If command ENTERed
                    {
                        if(cmdCount > 0u){                                 // Command entered by terminal
                            UART_WriteLine((uint8 *)"\r\nEntered: ",11u,0u);
                            UART_WriteLine(cmdBuffer,cmdCount,1u);
                            processbuffer(cmdBuffer,cmdCount);
                            cmdCount = 0u;
                            // UART_WriteLine((uint8 *)">\r\n",3u);           // prompt
                        }
                        else
                        {
                            // UART_WriteLine(buffer,count);               // Full command received
                            processbuffer(buffer,count-2);
                        }
                        UART_WriteLine((uint8 *)">",1u,0u);
                    }
                    else
                    {
                        cmdBuffer[cmdCount] = buffer[0];
                        cmdCount++;
                        UART_WriteLine(buffer,count,0u);                   // send back buffer to client            
                    }
                    /* Wait until component is ready to send data to host. */
                

                    
                    // ================================================================================= I2C END
                    /* If the last sent packet is exactly the maximum packet 
                    *  size, it is followed by a zero-length packet to assure
                    *  that the end of the segment is properly identified by 
                    *  the terminal.
                    */
                    if (USBUART_BUFFER_SIZE == count)
                    {
                        /* Wait until component is ready to send data to PC. */
                        while (0u == USBUART_CDCIsReady())
                        {
                        }

                        /* Send zero-length packet to PC. */
                        USBUART_PutData(NULL, 0u);
                        //USBUART_PutCRLF();
                        
                    }
                }
            }

            /* Check for Line settings change. */
            RSstate = USBUART_IsLineChanged();
            if (0u != RSstate)
            {
                /* Output on LCD Line Coding settings. */
                if (0u != (RSstate & USBUART_LINE_CODING_CHANGED))
                {                  
                    /* Get string to output. */
                    //blink(500u,2u);
                }

                /* Output on LCD Line Control settings. */
                if (0u != (RSstate & USBUART_LINE_CONTROL_CHANGED))
                {                   
                    //blink(100u,1u);
                }
            }

        }
    }
}

