/*
 * GBA Rush PSP
 *
 * Copyright (C) 2026 Piero Carrieri
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef POWER_LIFECYCLE_H
#define POWER_LIFECYCLE_H

#include <stdint.h>

typedef enum
{
  POWER_LIFECYCLE_RUNNING,
  POWER_LIFECYCLE_SUSPENDED,
  POWER_LIFECYCLE_RESUME_PENDING
} PowerLifecycleState;

typedef struct
{
  volatile uint32_t suspended;
  volatile uint32_t save_sequence;
  volatile uint32_t resume_sequence;
  volatile int32_t  audio_result;
  uint32_t handled_save_sequence;
  uint32_t handled_resume_sequence;
} PowerLifecycle;

void power_lifecycle_init(PowerLifecycle *lifecycle);
void power_lifecycle_suspend(PowerLifecycle *lifecycle, int request_save);
void power_lifecycle_resume(PowerLifecycle *lifecycle, int audio_result);
PowerLifecycleState power_lifecycle_state(const PowerLifecycle *lifecycle);
int power_lifecycle_take_save(PowerLifecycle *lifecycle);
int power_lifecycle_take_resume(PowerLifecycle *lifecycle, int *audio_result);

#endif