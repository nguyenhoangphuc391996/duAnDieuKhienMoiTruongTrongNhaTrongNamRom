/*
 * itm.c
 *
 *  Created on: Apr 24, 2026
 *      Author: embedded
 */


#include "itm.h"

void itm_print(const char* str){	while(*str)	{		ITM_SendChar(*str++);	}}

// Hàm in số nguyên không dùng sprintf
void itm_put_int(int num) {
    char buf[12]; // Đủ cho số nguyên 32-bit (kể cả dấu trừ)
    int i = 0;

    if (num == 0) {
        ITM_SendChar('0');
        return;
    }

    if (num < 0) {
        ITM_SendChar('-');
        num = -num;
    }

    // Chuyển số thành chuỗi (ngược)
    while (num > 0) {
        buf[i++] = (num % 10) + '0';
        num /= 10;
    }

    // In ngược lại chuỗi trong buffer
    while (i > 0) {
        ITM_SendChar(buf[--i]);
    }
}

void Error_Handler_Debug(char *file, int line) {
    __disable_irq();

    itm_print("\r\n[!] CRITICAL ERROR\r\n");
    itm_print("File: ");
    itm_print(file);
    itm_print("\r\nLine: ");

    // Sử dụng hàm mới ở đây thay vì sprintf
    itm_put_int(line);

    itm_print("\r\n------------------\r\n");

    while (1) {
        // Có thể thêm nháy LED PC13 (Black Pill/Blue Pill) để báo hiệu
        // HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        // for(int i=0; i<500000; i++);
    }
}
