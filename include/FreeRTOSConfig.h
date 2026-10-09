#ifndef RT_SPARK_FREERTOS_CONFIG_H
#define RT_SPARK_FREERTOS_CONFIG_H

#include <stdint.h>
#include "stm32f4xx.h"

/* Use the detected clock, not the board manifest's nominal F_CPU. */
#define configCPU_CLOCK_HZ                       (SystemCoreClock)
#define configTICK_RATE_HZ                       1000U
#define configUSE_16_BIT_TICKS                   0
#define configUSE_PREEMPTION                     1
#define configUSE_TIME_SLICING                   1
#define configMAX_PRIORITIES                     5
#define configMINIMAL_STACK_SIZE                 256U
#define configMAX_TASK_NAME_LEN                  16
#define configIDLE_SHOULD_YIELD                  1

#define configSUPPORT_DYNAMIC_ALLOCATION         1
#define configSUPPORT_STATIC_ALLOCATION          0
#define configTOTAL_HEAP_SIZE                    (32U * 1024U)
#define configAPPLICATION_ALLOCATED_HEAP         0
#define configUSE_MALLOC_FAILED_HOOK             1
#define configCHECK_FOR_STACK_OVERFLOW           2

#define configUSE_MUTEXES                        1
#define configUSE_RECURSIVE_MUTEXES              0
#define configUSE_COUNTING_SEMAPHORES            0
#define configUSE_TASK_NOTIFICATIONS             1
#define configQUEUE_REGISTRY_SIZE                0
#define configUSE_CO_ROUTINES                    0
#define configUSE_TRACE_FACILITY                 0
#define configUSE_STATS_FORMATTING_FUNCTIONS     0
#define configGENERATE_RUN_TIME_STATS            0

/* Explicit Stage 1 safeguards: no timer daemon, sleep hook or newlib changes. */
#define configUSE_TIMERS                         0
#define configUSE_TICKLESS_IDLE                  0
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configUSE_NEWLIB_REENTRANT               0

#define configPRIO_BITS                          __NVIC_PRIO_BITS
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY  15U
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 6U
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))

#if configPRIO_BITS != 4
#error Stage 1 expects four implemented STM32F407 interrupt priority bits.
#endif
#if configMAX_SYSCALL_INTERRUPT_PRIORITY != 0x60
#error Stage 1 must keep priority-5 audio above the RTOS syscall threshold.
#endif

/* SysTick is deliberately NOT aliased: its wrapper also advances HAL time. */
#define vPortSVCHandler                          SVC_Handler
#define xPortPendSVHandler                       PendSV_Handler

#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelete                     0
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_uxTaskGetStackHighWaterMark      1

#ifdef __cplusplus
extern "C" {
#endif
void RtosSupport_AssertFailed(const char *file, uint32_t line)
    __attribute__((noreturn));
#ifdef __cplusplus
}
#endif

#define configASSERT(condition) \
    do { if (!(condition)) RtosSupport_AssertFailed(__FILE__, __LINE__); } while (0)

#endif
