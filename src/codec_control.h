#ifndef RT_SPARK_CODEC_CONTROL_H
#define RT_SPARK_CODEC_CONTROL_H

#if defined(RTOS_STAGE4_PLAYER) && RTOS_STAGE4_PLAYER
// Create after hardware startup, before task creation/scheduler startup.
void CodecControl_Init();
// Task-context only. Before scheduling, single-owner bootstrap needs no lock.
void CodecControl_Lock();
void CodecControl_Unlock();
#endif

#endif
