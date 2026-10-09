#include "rtos_support.h"

#if (defined(RTOS_STAGE1_SMOKE) && RTOS_STAGE1_SMOKE) || \
    (defined(RTOS_STAGE2_PLAYER) && RTOS_STAGE2_PLAYER)

#if defined(RTOS_STAGE1_SMOKE) && RTOS_STAGE1_SMOKE && \
    defined(RTOS_STAGE2_PLAYER) && RTOS_STAGE2_PLAYER
#error Select either Stage 1 smoke mode or Stage 2 player mode, not both.
#endif

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <cstdio>
#include <cstring>

/* The GCC port defines this handler but does not expose it in a header. */
extern "C" void xPortSysTickHandler(void);

namespace {
UART_HandleTypeDef *smokeUart = nullptr;
#if defined(RTOS_STAGE1_SMOKE) && RTOS_STAGE1_SMOKE
SemaphoreHandle_t diagnosticUartMutex = nullptr;
constexpr uint16_t smokeStackWords = 768;
constexpr UBaseType_t smokePriority = 1;
volatile uint32_t smokeCountA = 0;
volatile uint32_t smokeCountB = 0;
#endif
volatile bool failureActive = false;

struct FailureRecord {
    const char *reason;
    const char *file;
    const char *task;
    uint32_t line;
    uint32_t cfsr;
    uint32_t hfsr;
};

/* Remains available to a debugger even if UART cannot report the failure. */
volatile FailureRecord failureRecord = {};

void failureCharacter(char value)
{
    if (smokeUart == nullptr || smokeUart->Instance != USART1 ||
        (USART1->CR1 & USART_CR1_UE) == 0) return;

    // Bounded polling: no HAL tick, formatting, allocation or RTOS lock needed.
    for (uint32_t attempts = 0; attempts < 100000U; ++attempts) {
        if ((USART1->SR & USART_SR_TXE) != 0) {
            USART1->DR = static_cast<uint8_t>(value);
            return;
        }
    }
}

void failureText(const char *text)
{
    if (text == nullptr) return;
    for (uint32_t i = 0; i < 192U && text[i] != '\0'; ++i)
        failureCharacter(text[i]);
}

void failureNumber(uint32_t number)
{
    uint32_t divisor = 1000000000U;
    while (divisor > 1U && number < divisor) divisor /= 10U;
    do {
        failureCharacter(static_cast<char>('0' + number / divisor));
        number %= divisor;
        divisor /= 10U;
    } while (divisor != 0U);
}

[[noreturn]] void fail(const char *reason, const char *file = nullptr,
                      uint32_t line = 0, const char *task = nullptr)
{
    __disable_irq();
    if (!failureActive) {
        failureActive = true;
        failureRecord.reason = reason;
        failureRecord.file = file;
        failureRecord.task = task;
        failureRecord.line = line;
        failureRecord.cfsr = SCB->CFSR;
        failureRecord.hfsr = SCB->HFSR;
        failureText("\r\nFATAL: ");
        failureText(reason);
        if (task != nullptr) {
            failureText(" TASK=");
            failureText(task);
        }
        if (file != nullptr) {
            failureText(" FILE=");
            failureText(file);
            failureText(" LINE=");
            failureNumber(line);
        }
        failureText("\r\nHALTED - RESET REQUIRED\r\n");
    }
    for (;;) __NOP();
}

#if defined(RTOS_STAGE1_SMOKE) && RTOS_STAGE1_SMOKE
void uartWrite(const char *text)
{
    if (HAL_UART_Transmit(smokeUart,
                          reinterpret_cast<uint8_t *>(const_cast<char *>(text)),
                          static_cast<uint16_t>(std::strlen(text)), 1000U) != HAL_OK)
        fail("UART TRANSMIT FAILED");
}

void taskStarted(bool taskA)
{
    configASSERT(xSemaphoreTake(diagnosticUartMutex, portMAX_DELAY) == pdTRUE);
    uartWrite(taskA ? "RTOS SMOKE TASK A STARTED\r\n"
                    : "RTOS SMOKE TASK B STARTED\r\n");
    if (taskA) {
        configASSERT(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
        configASSERT(NVIC_GetPriority(SysTick_IRQn) == 15U);
        configASSERT(NVIC_GetPriority(PendSV_IRQn) == 15U);
        uartWrite("SCHEDULER RUNNING\r\n"
                  "KERNEL IRQ CHECK: SYSTICK=15 PENDSV=15 PASS\r\n");
    }
    configASSERT(xSemaphoreGive(diagnosticUartMutex) == pdTRUE);
}

void smokeTask(void *argument)
{
    const bool taskA = argument == nullptr;
    taskStarted(taskA);
    if (!taskA) vTaskDelay(pdMS_TO_TICKS(500));

    uint32_t previousHal = HAL_GetTick();
    TickType_t previousRtos = xTaskGetTickCount();
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        configASSERT(xSemaphoreTake(diagnosticUartMutex, portMAX_DELAY) == pdTRUE);
        // Formatting and transmit share the lock; Stage 1 uses newlib=0.
        const uint32_t halNow = HAL_GetTick();
        const TickType_t rtosNow = xTaskGetTickCount();
        volatile uint32_t &count = taskA ? smokeCountA : smokeCountB;
        const uint32_t other = taskA ? smokeCountB : smokeCountA;
        ++count;
        char message[256];
        std::snprintf(message, sizeof(message),
                      "TASK %c: count=%lu HAL=%lu (+%lu) RTOS=%lu (+%lu) "
                      "OTHER=%lu STACK=%luw HEAP=%lu MIN_HEAP=%lu\r\n",
                      taskA ? 'A' : 'B', static_cast<unsigned long>(count),
                      static_cast<unsigned long>(halNow),
                      static_cast<unsigned long>(halNow - previousHal),
                      static_cast<unsigned long>(rtosNow),
                      static_cast<unsigned long>(rtosNow - previousRtos),
                      static_cast<unsigned long>(other),
                      static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr)),
                      static_cast<unsigned long>(xPortGetFreeHeapSize()),
                      static_cast<unsigned long>(xPortGetMinimumEverFreeHeapSize()));
        uartWrite(message);
        previousHal = halNow;
        previousRtos = rtosNow;
        configASSERT(xSemaphoreGive(diagnosticUartMutex) == pdTRUE);
    }
}
#endif
} // namespace

extern "C" void SysTick_Handler(void)
{
    HAL_IncTick();
    // Also tick a suspended scheduler so FreeRTOS can account for pended ticks.
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
        xPortSysTickHandler();
}

extern "C" void RtosSupport_AssertFailed(const char *file, uint32_t line)
{
    fail("FREERTOS ASSERT", file, line);
}

extern "C" void vApplicationStackOverflowHook(TaskHandle_t, char *taskName)
{
    fail("FREERTOS STACK OVERFLOW", nullptr, 0, taskName);
}

extern "C" void vApplicationMallocFailedHook(void)
{
    fail("FREERTOS HEAP ALLOCATION FAILED");
}

extern "C" void HardFault_Handler(void)
{
    fail("HARDFAULT");
}

void RtosSupport_Fatal(const char *reason)
{
    fail(reason);
}

#if defined(RTOS_STAGE1_SMOKE) && RTOS_STAGE1_SMOKE
void RtosSupport_StartSmokeTest(UART_HandleTypeDef *uart)
{
    smokeUart = uart;
    configASSERT(smokeUart != nullptr);
    SystemCoreClockUpdate(); // Read the existing clock tree; do not change it.
    configASSERT(SystemCoreClock >= configTICK_RATE_HZ);
    configASSERT(SystemCoreClock / configTICK_RATE_HZ - 1U <= SysTick_LOAD_RELOAD_Msk);
    configASSERT(HAL_NVIC_GetPriorityGrouping() == NVIC_PRIORITYGROUP_4);
    configASSERT(NVIC_GetPriority(SysTick_IRQn) == 15U);

    uartWrite("\r\nFREERTOS STAGE 1\r\n");
    char message[128];
    std::snprintf(message, sizeof(message), "SystemCoreClock: %lu Hz\r\n",
                  static_cast<unsigned long>(SystemCoreClock));
    uartWrite(message);
    uartWrite("KERNEL: STM32CUBE FREERTOS " tskKERNEL_VERSION_NUMBER "\r\n"
              "IRQ CHECK: BITS=4 GROUP=4 KERNEL=15 MAX_SYSCALL=6 (0x60) PASS\r\n"
              "SMOKE ONLY: PLAYER PERIPHERALS NOT INITIALIZED\r\n");

    diagnosticUartMutex = xSemaphoreCreateMutex();
    configASSERT(diagnosticUartMutex != nullptr);
    configASSERT(xTaskCreate(smokeTask, "SmokeA", smokeStackWords, nullptr,
                             smokePriority, nullptr) == pdPASS);
    // A stable, non-null token identifies task B without any shared mutable data.
    static const uint8_t taskBToken = 0;
    configASSERT(xTaskCreate(smokeTask, "SmokeB", smokeStackWords,
                             const_cast<uint8_t *>(&taskBToken),
                             smokePriority, nullptr) == pdPASS);
    uartWrite("Scheduler starting...\r\n");
    vTaskStartScheduler();
    fail("SCHEDULER RETURNED");
}
#endif

#if defined(RTOS_STAGE2_PLAYER) && RTOS_STAGE2_PLAYER
void RtosSupport_PreparePlayerMode(UART_HandleTypeDef *uart)
{
    smokeUart = uart;
    configASSERT(smokeUart != nullptr);
    configASSERT(SystemCoreClock >= configTICK_RATE_HZ);
    configASSERT(SystemCoreClock / configTICK_RATE_HZ - 1U <= SysTick_LOAD_RELOAD_Msk);
    configASSERT(HAL_NVIC_GetPriorityGrouping() == NVIC_PRIORITYGROUP_4);
    configASSERT(NVIC_GetPriority(SysTick_IRQn) == 15U);
}
#endif

#endif // RTOS_STAGE1_SMOKE or RTOS_STAGE2_PLAYER
