/*
 * terminal.c
 *
 *  Created on: Jan 2, 2021
 *      Author: danie
 */
#include "FreeRTOS.h"
#include "../terminal.h"
#include "../utils.h"
#include "usart.h"
#include "cmsis_os2.h"
#include "string.h"

/*--------private exportet prototypes---------*/

extern osMessageQueueId_t myRxQueueHandle;

extern osMessageQueueId_t myTxQueueHandle;

extern osSemaphoreId_t myFlagNewStringHandle;

extern void term_lol_parse(TD_LINEOBJ *line);


/*-------private local prototypes-------------*/

void
term_lol_setCallback	(const char* command,
			const char *help,
			const char *arg_names,
			void(*cbf)(int argc,const char **argv));

BaseType_t
dBase_StoreQueue	(osMessageQueueId_t QueueHandle,
			TD_LINEOBJ *line);


/*----------private local code----------------*/
BaseType_t
    dBase_StoreQueue
			(osMessageQueueId_t QueueHandle,
			TD_LINEOBJ *line)
    {
    return xQueueSend(QueueHandle, line, 10);
    }
/*___________________________________________________________*/
void
    dbase_LoadQueue
			(osMessageQueueId_t QueueHandle,
			TD_LINEOBJ *line)
    {
    xQueueReceive(QueueHandle, line,  ( portTickType ) 10);
    }
/*-----------------------------------------------------------*/
void
    term_qPrintf 	(osMessageQueueId_t QueueHandle, char *fmt, ...)
	{
	int ItemsLeft = uxQueueSpacesAvailable(QueueHandle);

	BaseType_t xStatus;

	if ( ItemsLeft)
	    {
	    va_list argp;
	   va_start( argp, fmt);
	   uint8_t pbuffer[ UART_PRINTBUFFER];
	   int bytesWrote = 0;


	   bytesWrote = vsnprintf( pbuffer, UART_PRINTBUFFER, fmt, argp);

	   va_end(argp);

	   utils_truncate_number_int(&bytesWrote, 0, ItemsLeft);


	    for (int var = 0; var < bytesWrote; ++var)
		{
		xStatus = xQueueSendToBack( QueueHandle, &pbuffer[var], 0);
		}
	    }
	else
	    {
	    xQueueReset(QueueHandle);

	    xStatus = xQueueSendToBack(QueueHandle, "F", 0);

	    }
       }
/*-----------------------------------------------------------*/
void
    dbase_Make		(TD_LINEOBJ *line,
			char* filename,
			const char* string,
			char* header,
			char* postfix,
			uint16_t linenr,
		   	char 	*fmt,
			... )
    {
    char pbuffer[TD_LINEOBJ_MAX_SSIZE];

    int bytesWrote;

    if (fmt == 0)
	{
	   bytesWrote = snprintf( pbuffer, TD_LINEOBJ_MAX_SSIZE, string);
	   strcpy( line->string, pbuffer);


	}
    else
	{
	memset	( pbuffer, 1, TD_LINEOBJ_MAX_SSIZE);

	va_list argp;
	va_start( argp, fmt );
	bytesWrote = vsnprintf( pbuffer, UART_PRINTBUFFER, fmt, argp );
	strcpy( line->string, pbuffer );
	va_end( argp );
	}

    line->linenr = linenr;

    bytesWrote = snprintf( pbuffer, UART_PRINTBUFFER, filename);
    strcpy( line->filename, pbuffer);

    bytesWrote = snprintf(pbuffer, UART_PRINTBUFFER, header);
    strcpy( line->header, pbuffer);

    bytesWrote = snprintf(pbuffer, UART_PRINTBUFFER, postfix);
    strcpy( line->postfix, pbuffer);

    }
/*-----------------------------------------------------------*/
void
    term_vprintLineObj	(osMessageQueueId_t QueueHandle,TD_LINEOBJ *line)
    {
    term_qPrintf(QueueHandle, "\r<%s/%s> %s [%s]", line->filename, line->header, line->string, line->postfix);
    }
/*-----------------------------------------------------------*/
void
    term_lol_sendQueue	(osMessageQueueId_t QueueHandle)
    {
    UBaseType_t ItemsLeft = uxQueueMessagesWaiting(QueueHandle);
    if (ItemsLeft)
	{
	//limits the numbers of bytes to send
	utils_truncate_number_int((int*)ItemsLeft, 0, TX_BYTES_AT_ONCE);
	//
	uint8_t dmaBuff[ItemsLeft];
	//copy bytes that are send at once to temp buffer
	for (int var = 0; var < ItemsLeft; ++var)
	    {
	    //should receive from front
	    uint8_t lReceivedValue;

	    xQueueReceive(QueueHandle, &lReceivedValue, 0);

	    dmaBuff[var] = lReceivedValue;
	    }
	//transmission time[s]: 10 bit / n baud

	HAL_UART_Transmit_DMA(&huart1, dmaBuff, ItemsLeft);

	uint32_t transmission_time= 10 * ItemsLeft * 1000 / huart1.Init.BaudRate;

	osDelay(transmission_time+1);

	}
    }
/*-----------------------------------------------------------*/
void
    HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
    {
    uint8_t byte_received = huart->Instance->DR;

    xQueueSendToBackFromISR(myRxQueueHandle, &byte_received, 0);

    if (byte_received == (uint8_t)13)
	xSemaphoreGiveFromISR(myFlagNewStringHandle, 0);

    HAL_UART_Receive_DMA(&huart1, (uint8_t*) &readbyte, 1);
    }


TD_TERMINAL btTerm;
