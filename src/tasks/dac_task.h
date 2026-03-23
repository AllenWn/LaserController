#pragma once

void dac_task_start(void);

// Wake the DAC task to apply the latest targets/clamp state.
// Safe to call from any task context.
void dac_task_notify_update(void);
