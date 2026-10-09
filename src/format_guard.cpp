#if defined(RTOS_STAGE5_PLAYER) && RTOS_STAGE5_PLAYER
#include "FreeRTOS.h"
#include "task.h"
#include <cstdarg>
#include <cstdio>

// Stage-5 link wrapping serializes existing bounded UI/UART formatting calls.
// No C-library configuration or malloc hook changes are needed. Suspension
// prevents task overlap; interrupts (including audio DMA) remain enabled.
extern "C" int __wrap_snprintf(char *buffer, size_t size, const char *format, ...)
{
    configASSERT(__get_IPSR() == 0U);
    const bool scheduled = xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED;
    if (scheduled) vTaskSuspendAll();
    va_list arguments;
    va_start(arguments, format);
    const int result = std::vsnprintf(buffer, size, format, arguments);
    va_end(arguments);
    if (scheduled) (void)xTaskResumeAll();
    return result;
}
#endif
