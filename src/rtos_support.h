#ifndef RT_SPARK_RTOS_SUPPORT_H
#define RT_SPARK_RTOS_SUPPORT_H

#if (defined(RTOS_STAGE1_SMOKE) && RTOS_STAGE1_SMOKE) || \
    (defined(RTOS_STAGE2_PLAYER) && RTOS_STAGE2_PLAYER)
#include "stm32f4xx_hal.h"

void RtosSupport_Fatal(const char *reason) __attribute__((noreturn));

#if defined(RTOS_STAGE1_SMOKE) && RTOS_STAGE1_SMOKE
/* Diagnostic entry point only; it starts the scheduler and never returns. */
void RtosSupport_StartSmokeTest(UART_HandleTypeDef *uart)
    __attribute__((noreturn));
#endif

#if defined(RTOS_STAGE2_PLAYER) && RTOS_STAGE2_PLAYER
/* Register UART for the existing failure hooks before player initialization. */
void RtosSupport_PreparePlayerMode(UART_HandleTypeDef *uart);
#endif

#endif

#endif
