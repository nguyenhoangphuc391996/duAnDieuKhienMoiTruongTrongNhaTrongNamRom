#ifndef DS18B20_APP_H
#define DS18B20_APP_H

#include "cmsis_os.h"
#include <OneWireUart.h>
#include "itm.h"

typedef struct {
  uint8_t sensorIndex;
  int16_t tempDeciC;
  uint32_t tick;
} Ds18b20QueueItem;

typedef void (*Ds18b20FaultCallback)(uint8_t sensorIndex, int16_t tempDeciC);

void Ds18b20Api_DefaultOnWireFault(uint8_t sensorIndex, int16_t tempDeciC);
void Ds18b20Api_PrintItem(const Ds18b20QueueItem *item);

HAL_StatusTypeDef Ds18b20Api_Init(OneWire_Config *cfg, OneWire_Context *ctx);
HAL_StatusTypeDef Ds18b20Api_RequestRelearn(OneWire_Config *cfg, OneWire_Context *ctx);
HAL_StatusTypeDef Ds18b20Api_EnsureReady(OneWire_Config *cfg, OneWire_Context *ctx);
HAL_StatusTypeDef Ds18b20Api_ReadAndQueue(OneWire_Config *cfg, OneWire_Context *ctx, osMessageQueueId_t queueId);
HAL_StatusTypeDef Ds18b20Api_Service(OneWire_Config *cfg, OneWire_Context *ctx, osMessageQueueId_t queueId, Ds18b20FaultCallback onFault);

#endif
