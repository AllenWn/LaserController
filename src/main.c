#include "app_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "fault_isr.h"
#include "safety.h"
#include "safety_internal.h"
#include "sensor_tasks.h"
#include "state_machine.h"

static const char *TAG = "main";

static void safety_task(void *arg)
{
  (void)arg;
  safety_bind_task_handle(xTaskGetCurrentTaskHandle());

  for (;;)
  {
    // Wake periodically or sooner if a fault ISR notified us.
    (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(SAFETY_TASK_PERIOD_MS));

    const bool permit = safety_get_laser_permit();
    state_machine_step(permit);
    safety_apply_outputs();
  }
}

static void ui_task(void *arg)
{
  (void)arg;

  // Placeholder: until button GPIOs are defined, keep OFF.
  // Integrate debounced arm/trigger buttons here.
  state_machine_set_arm_request(false);
  state_machine_set_trigger_request(false);

  for (;;)
  {
    vTaskDelay(pdMS_TO_TICKS(UI_TASK_PERIOD_MS));
  }
}

static void bringup_default_fail_safe_conditions(void)
{
  // Fail-safe: start with all conditions false. Individual tasks will set true when verified.
  safety_set_condition(SAFETY_BIT_TEC_READY, false);
  safety_set_condition(SAFETY_BIT_TEC_OK, false);
  safety_set_condition(SAFETY_BIT_LD_OK, false);
  safety_set_condition(SAFETY_BIT_ORIENTATION_OK, false);
  safety_set_condition(SAFETY_BIT_DISTANCE_OK, false);
  safety_set_condition(SAFETY_BIT_INTERLOCK_OK, false);
}

void app_main(void)
{
  ESP_LOGI(TAG, "LaserDriver firmware starting");

  safety_init();
  state_machine_init();
  bringup_default_fail_safe_conditions();
  fault_isr_init();
  sensor_tasks_start();

  xTaskCreatePinnedToCore(safety_task, "safety", 4096, NULL, configMAX_PRIORITIES - 1, NULL, 0);
  xTaskCreatePinnedToCore(ui_task, "ui", 4096, NULL, 5, NULL, 0);

  ESP_LOGI(TAG, "Tasks started");
}
