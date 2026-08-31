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

#include "power_lifecycle.h"
#include <string.h>

static void power_lifecycle_memory_barrier(void)
{
#if defined(__PSP__) || defined(__psp__)
  __asm__ volatile ("sync" ::: "memory");
#else
  __sync_synchronize();
#endif
}

void power_lifecycle_init(PowerLifecycle *lifecycle)
{
  if(lifecycle == NULL)
    return;
  memset(lifecycle, 0, sizeof(*lifecycle));
}

void power_lifecycle_suspend(PowerLifecycle *lifecycle, int request_save)
{
  if(lifecycle == NULL || lifecycle->suspended)
    return;

  if(request_save)
    lifecycle->save_sequence++;
  power_lifecycle_memory_barrier();
  lifecycle->suspended = 1;
}

void power_lifecycle_resume(PowerLifecycle *lifecycle, int audio_result)
{
  if(lifecycle == NULL || !lifecycle->suspended)
    return;

  lifecycle->audio_result = audio_result;
  lifecycle->resume_sequence++;
  power_lifecycle_memory_barrier();
  lifecycle->suspended = 0;
}

PowerLifecycleState power_lifecycle_state(const PowerLifecycle *lifecycle)
{
  if(lifecycle == NULL)
    return POWER_LIFECYCLE_RUNNING;

  power_lifecycle_memory_barrier();
  if(lifecycle->suspended)
    return POWER_LIFECYCLE_SUSPENDED;
  if(lifecycle->handled_resume_sequence != lifecycle->resume_sequence)
    return POWER_LIFECYCLE_RESUME_PENDING;
  return POWER_LIFECYCLE_RUNNING;
}

int power_lifecycle_take_save(PowerLifecycle *lifecycle)
{
  uint32_t sequence;

  if(lifecycle == NULL)
    return 0;

  power_lifecycle_memory_barrier();
  sequence = lifecycle->save_sequence;
  if(lifecycle->handled_save_sequence == sequence)
    return 0;

  lifecycle->handled_save_sequence = sequence;
  return 1;
}

int power_lifecycle_take_resume(PowerLifecycle *lifecycle, int *audio_result)
{
  uint32_t sequence;

  if(lifecycle == NULL)
    return 0;

  power_lifecycle_memory_barrier();
  sequence = lifecycle->resume_sequence;
  if(lifecycle->handled_resume_sequence == sequence)
    return 0;

  if(audio_result != NULL)
    *audio_result = lifecycle->audio_result;
  lifecycle->handled_resume_sequence = sequence;
  return 1;
}