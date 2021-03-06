/*
 * downstream_msc.h
 *
 *  Created on: 8/08/2015
 *      Author: Robert Fisk
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef INC_DOWNSTREAM_MSC_H_
#define INC_DOWNSTREAM_MSC_H_


#include "downstream_interface_def.h"
#include "downstream_spi.h"


#define MSC_SUPPORTED_BLOCK_SIZE    512
#define MSC_FIXED_LUN               0


typedef void (*DownstreamMSCCallbackPacketTypeDef)(DownstreamPacketTypeDef* receivedPacket,
                                                   uint16_t dataLength8);


InterfaceCommandClassTypeDef Downstream_MSC_ApproveConnectedDevice(void);
void Downstream_MSC_PacketProcessor(DownstreamPacketTypeDef* receivedPacket);
HAL_StatusTypeDef Downstream_MSC_PutStreamDataPacket(DownstreamPacketTypeDef* packetToSend,
                                                     uint32_t dataLength8);
HAL_StatusTypeDef Downstream_MSC_GetStreamDataPacket(DownstreamMSCCallbackPacketTypeDef callback);



#endif /* INC_DOWNSTREAM_MSC_H_ */
