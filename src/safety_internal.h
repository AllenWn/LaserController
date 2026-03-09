#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Internal hook used by the safety task to receive ISR notifications.
void safety_bind_task_handle(TaskHandle_t task);

