#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "fsm.h"
#include "board_io.h"
#include "system_status.h"
#include "tasks/analog_monitor_task.h"
#include "tasks/dac_task.h"
#include "tasks/digital_monitor_task.h"
#include "tasks/imu_task.h"
#include "tasks/supervisor_task.h"

static const char *TAG = "main";

void app_main(void)
{
  fsm_config_t cfg = {
      .ready_enable_tec_power = true,
      .ready_enable_ld_power = false,
      .ready_ld_mode = LD_MODE_SHUTDOWN,
  };
  fsm_init(&cfg);
  ESP_ERROR_CHECK(board_io_init());
  system_status_init();
  supervisor_task_start();
  digital_monitor_task_start();
  analog_monitor_task_start();
  imu_task_start();
  dac_task_start();

  ESP_LOGI(TAG, "LaserDriver firmware: FSM+Supervisor started (state=%d)", (int)fsm_get_state());
}
