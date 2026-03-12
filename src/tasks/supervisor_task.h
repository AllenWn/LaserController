#pragma once

#include <stdbool.h>
#include <stdint.h>

void supervisor_task_start(void);

// Operator intent (from UI task).
void supervisor_set_trigger(bool trigger);
