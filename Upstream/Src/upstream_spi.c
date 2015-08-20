/*
 * upstream_spi.c
 *
 *  Created on: 21/06/2015
 *      Author: Robert Fisk
 */

#include <upstream_interface_def.h>
#include <upstream_spi.h>
#include "stm32f4xx_hal.h"
#include "usbd_def.h"
#include "board_config.h"



SPI_HandleTypeDef			Hspi1;
UpstreamPacketTypeDef		UpstreamPacket0;
UpstreamPacketTypeDef		UpstreamPacket1;
UpstreamPacketTypeDef*		CurrentWorkingPacket;
UpstreamPacketTypeDef*		NextTxPacket;			//Indicates we have a pending TX packet

InterfaceStateTypeDef				UpstreamInterfaceState;
FreePacketCallbackTypeDef			PendingFreePacketCallback;	//Indicates someone is waiting for a packet buffer to become available
SpiPacketReceivedCallbackTypeDef	ReceivePacketCallback;		//Indicates someone is waiting for a received packet

uint8_t						SentCommandClass;
uint8_t						SentCommand;


static void SPI1_Init(void);
static HAL_StatusTypeDef Upstream_CheckBeginPacketReception(void);
static void Upstream_BeginPacketReception(UpstreamPacketTypeDef* freePacket);



void Upstream_InitSPI(void)
{
	UpstreamInterfaceState = UPSTREAM_INTERFACE_RESET;

	SPI1_Init();
	UpstreamPacket0.Busy = NOT_BUSY;
	UpstreamPacket1.Busy = NOT_BUSY;
	NextTxPacket = NULL;
	PendingFreePacketCallback = NULL;
	ReceivePacketCallback = NULL;

	//Todo: check connection to downstream, await client USB insertion

	while (!DOWNSTREAM_TX_OK_ACTIVE);
	UpstreamInterfaceState = UPSTREAM_INTERFACE_IDLE;
}


void SPI1_Init(void)
{
	Hspi1.Instance = SPI1;
	Hspi1.State = HAL_SPI_STATE_RESET;
	Hspi1.Init.Mode = SPI_MODE_MASTER;
	Hspi1.Init.Direction = SPI_DIRECTION_2LINES;
	Hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
	Hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
	Hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
	Hspi1.Init.NSS = SPI_NSS_SOFT;
	Hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;	//42MHz APB2 / 32 = 1.3Mbaud
	Hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
	Hspi1.Init.TIMode = SPI_TIMODE_DISABLED;
	Hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_ENABLED;
	Hspi1.Init.CRCPolynomial = SPI_CRC_DEFAULTPOLYNOMIAL;
	HAL_SPI_Init(&Hspi1);
}



//Used by USB interface classes, and by our internal RX code.
HAL_StatusTypeDef Upstream_GetFreePacket(FreePacketCallbackTypeDef callback)
{
	//Sanity checks
	if 	((UpstreamInterfaceState < UPSTREAM_INTERFACE_IDLE) ||
		 (UpstreamInterfaceState > UPSTREAM_INTERFACE_RX_PACKET))
	{
		SPI_INTERFACE_FREAKOUT_RETURN_HAL_ERROR;
	}

	//Do we already have a queued callback?
	if (PendingFreePacketCallback != NULL)
	{
		SPI_INTERFACE_FREAKOUT_RETURN_HAL_ERROR;
	}

	//Check if there is a free buffer now
	if (UpstreamPacket0.Busy == NOT_BUSY)
	{
		UpstreamPacket0.Busy = BUSY;
		callback(&UpstreamPacket0);
		return HAL_OK;
	}
	if (UpstreamPacket1.Busy == NOT_BUSY)
	{
		UpstreamPacket1.Busy = BUSY;
		callback(&UpstreamPacket1);
		return HAL_OK;
	}

	//Otherwise save requested address for when a buffer becomes free in the future
	PendingFreePacketCallback = callback;
	return HAL_OK;
}


UpstreamPacketTypeDef* Upstream_GetFreePacketImmediately(void)
{
	//Sanity checks
	if 	((UpstreamInterfaceState < UPSTREAM_INTERFACE_IDLE) ||
		 (UpstreamInterfaceState > UPSTREAM_INTERFACE_RX_PACKET))
	{
		SPI_INTERFACE_FREAKOUT_RETURN_HAL_ERROR;
	}

	//We are expecting a free buffer now
	if (UpstreamPacket0.Busy == NOT_BUSY)
	{
		UpstreamPacket0.Busy = BUSY;
		return &UpstreamPacket0;
	}
	if (UpstreamPacket1.Busy == NOT_BUSY)
	{
		UpstreamPacket1.Busy = BUSY;
		return &UpstreamPacket1;
	}

	//Should not happen:
	SPI_INTERFACE_FREAKOUT_NO_RETURN;
}


//Used by USB interface classes, and by our internal RX code.
void Upstream_ReleasePacket(UpstreamPacketTypeDef* packetToRelease)
{
	FreePacketCallbackTypeDef tempCallback;

	if ((packetToRelease != &UpstreamPacket0) &&
		(packetToRelease != &UpstreamPacket1))
	{
		SPI_INTERFACE_FREAKOUT_RETURN_HAL_ERROR;
	}

	if (PendingFreePacketCallback != NULL)
	{
		tempCallback = PendingFreePacketCallback;	//In extreme situations, running this callback can trigger another request for a free packet,
		PendingFreePacketCallback = NULL;			//thereby causing GetFreePacket to freak out. So we need to clear the callback indicator first.
		tempCallback(packetToRelease);
	}
	else
	{
		packetToRelease->Busy = NOT_BUSY;
	}
}


//Used by USB interface classes only.
//OK to call when still transmitting another packet.
//Not OK to call when receiving or waiting for downstream reply,
//as we can't let the size/packet sequence get out of sync.
HAL_StatusTypeDef Upstream_TransmitPacket(UpstreamPacketTypeDef* packetToWrite)
{
	//Sanity checks
	if ((packetToWrite != &UpstreamPacket0) &&
		(packetToWrite != &UpstreamPacket1))
	{
		SPI_INTERFACE_FREAKOUT_RETURN_HAL_ERROR;
	}
	if ((packetToWrite->Busy != BUSY) ||
		(packetToWrite->Length < UPSTREAM_PACKET_LEN_MIN) ||
		(packetToWrite->Length > UPSTREAM_PACKET_LEN))
	{
		SPI_INTERFACE_FREAKOUT_RETURN_HAL_ERROR;
	}
	if (NextTxPacket != NULL)
	{
		SPI_INTERFACE_FREAKOUT_RETURN_HAL_ERROR;
	}

	switch (UpstreamInterfaceState)
	{
	case UPSTREAM_INTERFACE_TX_SIZE_WAIT:
	case UPSTREAM_INTERFACE_TX_SIZE:
	case UPSTREAM_INTERFACE_TX_PACKET_WAIT:
	case UPSTREAM_INTERFACE_TX_PACKET:
		NextTxPacket = packetToWrite;
		break;

	case UPSTREAM_INTERFACE_IDLE:
		UpstreamInterfaceState = UPSTREAM_INTERFACE_TX_SIZE_WAIT;
		CurrentWorkingPacket = packetToWrite;
		SentCommandClass = CurrentWorkingPacket->CommandClass;
		SentCommand = CurrentWorkingPacket->Command;
		break;

	default:
		SPI_INTERFACE_FREAKOUT_RETURN_HAL_ERROR;
	}
	return HAL_OK;
}



//Called at the end of the SPI TX DMA transfer,
//at DMA2 interrupt priority. Assume *hspi points to our hspi1.
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
	SPI1_NSS_DEASSERT;

	if ((UpstreamInterfaceState != UPSTREAM_INTERFACE_TX_SIZE) &&
		(UpstreamInterfaceState != UPSTREAM_INTERFACE_TX_PACKET))
	{
		SPI_INTERFACE_FREAKOUT_RETURN_VOID;
	}

	if (UpstreamInterfaceState == UPSTREAM_INTERFACE_TX_SIZE)
	{
		UpstreamInterfaceState = UPSTREAM_INTERFACE_TX_PACKET_WAIT;
		return;
	}

	if (UpstreamInterfaceState == UPSTREAM_INTERFACE_TX_PACKET)
	{
		if ((PendingFreePacketCallback != NULL) && (NextTxPacket == NULL))
		{
			SPI_INTERFACE_FREAKOUT_RETURN_VOID;
		}

		Upstream_ReleasePacket(CurrentWorkingPacket);
		if (NextTxPacket != NULL)
		{
			//NextTxPacket has already passed the checks in Upstream_TransmitPacket.
			//So we just need to pass it to HAL_SPI_Transmit_DMA.
			UpstreamInterfaceState = UPSTREAM_INTERFACE_TX_SIZE_WAIT;
			CurrentWorkingPacket = NextTxPacket;
			NextTxPacket = NULL;
			return;
		}

		UpstreamInterfaceState = UPSTREAM_INTERFACE_IDLE;
		if (ReceivePacketCallback != NULL)
		{
			Upstream_CheckBeginPacketReception();
		}
	}
}


//Used by USB interface classes.
//Ok to call when idle or transmitting.
//Not OK to call when receiving or waiting for downstream reply.
HAL_StatusTypeDef Upstream_ReceivePacket(SpiPacketReceivedCallbackTypeDef callback)
{
	if (ReceivePacketCallback != NULL)
	{
		SPI_INTERFACE_FREAKOUT_RETURN_HAL_ERROR;
	}

	ReceivePacketCallback = callback;
	return Upstream_CheckBeginPacketReception();
}


//Internal use only.
HAL_StatusTypeDef Upstream_CheckBeginPacketReception(void)
{
	if ((UpstreamInterfaceState < UPSTREAM_INTERFACE_IDLE) ||
		(UpstreamInterfaceState > UPSTREAM_INTERFACE_RX_SIZE_WAIT))
	{
		SPI_INTERFACE_FREAKOUT_RETURN_HAL_ERROR;
	}

	if (UpstreamInterfaceState == UPSTREAM_INTERFACE_IDLE)
	{
		UpstreamInterfaceState = UPSTREAM_INTERFACE_RX_SIZE_WAIT;
	}
	return HAL_OK;
}


//This is called by EXTI3 falling edge interrupt,
//indicating that downstream is ready for the next transaction.
void Upstream_TxOkInterrupt(void)
{
	switch (UpstreamInterfaceState)
	{
	case UPSTREAM_INTERFACE_TX_SIZE_WAIT:
		UpstreamInterfaceState = UPSTREAM_INTERFACE_TX_SIZE;
		SPI1_NSS_ASSERT;
		if (HAL_SPI_Transmit_DMA(&Hspi1,
								 (uint8_t*)&CurrentWorkingPacket->Length,
								 2) != HAL_OK)
		{
			SPI_INTERFACE_FREAKOUT_RETURN_VOID;
		}
		break;

	case UPSTREAM_INTERFACE_TX_PACKET_WAIT:
		UpstreamInterfaceState = UPSTREAM_INTERFACE_TX_PACKET;
		SPI1_NSS_ASSERT;
		if ((HAL_SPI_Transmit_DMA(&Hspi1,
								  &CurrentWorkingPacket->CommandClass,
								  CurrentWorkingPacket->Length)) != HAL_OK)
		{
			SPI_INTERFACE_FREAKOUT_RETURN_VOID;
		}
		break;

	case UPSTREAM_INTERFACE_RX_SIZE_WAIT:
		Upstream_GetFreePacket(Upstream_BeginPacketReception);
		break;

	case UPSTREAM_INTERFACE_RX_PACKET_WAIT:
		UpstreamInterfaceState = UPSTREAM_INTERFACE_RX_PACKET;
		SPI1_NSS_ASSERT;
		if ((HAL_SPI_Receive_DMA(&Hspi1,
								 &CurrentWorkingPacket->CommandClass,
								 (CurrentWorkingPacket->Length + 1))) != HAL_OK)	//"When the CRC feature is enabled the pData Length must be Size + 1"
		{
			SPI_INTERFACE_FREAKOUT_RETURN_VOID;
		}
		break;

	default:
		SPI_INTERFACE_FREAKOUT_RETURN_VOID;
	}
}


//Internal use only.
//Called when we want to receive downstream packet, and a packet buffer has become free.
void Upstream_BeginPacketReception(UpstreamPacketTypeDef* freePacket)
{
	if (UpstreamInterfaceState != UPSTREAM_INTERFACE_RX_SIZE_WAIT)
	{
		SPI_INTERFACE_FREAKOUT_RETURN_VOID;
	}
	UpstreamInterfaceState = UPSTREAM_INTERFACE_RX_SIZE;
	CurrentWorkingPacket = freePacket;
	CurrentWorkingPacket->Length = 0;		//Our RX buffer is used by HAL_SPI_Receive_DMA as dummy TX data, we set Length to 0 so downstream will know this is a dummy packet.
	SPI1_NSS_ASSERT;
	if (HAL_SPI_Receive_DMA(&Hspi1,
							(uint8_t*)&CurrentWorkingPacket->Length,
							(2 + 1)) != HAL_OK)		//"When the CRC feature is enabled the pData Length must be Size + 1"
	{
		SPI_INTERFACE_FREAKOUT_RETURN_VOID;
	}
}


//Called at the end of the SPI RX DMA transfer,
//at DMA2 interrupt priority. Assume *hspi points to our hspi1.
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
	SpiPacketReceivedCallbackTypeDef tempPacketCallback;

	SPI1_NSS_DEASSERT;

	if ((UpstreamInterfaceState != UPSTREAM_INTERFACE_RX_SIZE) &&
		(UpstreamInterfaceState != UPSTREAM_INTERFACE_RX_PACKET))
	{
		SPI_INTERFACE_FREAKOUT_RETURN_VOID;
	}

	if (UpstreamInterfaceState == UPSTREAM_INTERFACE_RX_SIZE)
	{
		if ((CurrentWorkingPacket->Length < UPSTREAM_PACKET_LEN_MIN) ||
			(CurrentWorkingPacket->Length > UPSTREAM_PACKET_LEN))
		{
			SPI_INTERFACE_FREAKOUT_RETURN_VOID;
		}
		UpstreamInterfaceState = UPSTREAM_INTERFACE_RX_PACKET_WAIT;
		return;
	}

	if (UpstreamInterfaceState == UPSTREAM_INTERFACE_RX_PACKET)
	{
		UpstreamInterfaceState = UPSTREAM_INTERFACE_IDLE;
		if (((SentCommandClass != (CurrentWorkingPacket->CommandClass & COMMAND_CLASS_MASK)) &&
			 (SentCommandClass != COMMAND_CLASS_ERROR)) ||
			(SentCommand != CurrentWorkingPacket->Command))
		{
			SPI_INTERFACE_FREAKOUT_RETURN_VOID;
		}
		if (ReceivePacketCallback == NULL)
		{
			SPI_INTERFACE_FREAKOUT_RETURN_VOID;
		}
		//USB interface may want to receive another packet immediately,
		//so clear ReceivePacketCallback before the call.
		//It is the callback's responsibility to release the packet buffer we are passing to it!
		tempPacketCallback = ReceivePacketCallback;
		ReceivePacketCallback = NULL;
		tempPacketCallback(CurrentWorkingPacket);
	}
}


//Something bad happened! Possibly CRC error...
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
	SPI_INTERFACE_FREAKOUT_RETURN_VOID;
}