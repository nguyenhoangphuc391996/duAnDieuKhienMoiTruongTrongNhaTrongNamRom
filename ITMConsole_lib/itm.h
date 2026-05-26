/*
 * itm.h
 *
 *  Created on: Apr 24, 2026
 *      Author: embedded
 */

#ifndef ITM_H_
#define ITM_H_

#include "stm32f1xx.h"


void itm_print(const char* str);

void itm_put_int(int num); // Hàm in số nguyên mới
void Error_Handler_Debug(char *file, int line);

#define Error_Handler() Error_Handler_Debug(__FILE__, __LINE__)

#endif /* ITM_H_ */
