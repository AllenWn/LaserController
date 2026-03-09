#include "sensor_tasks.h"

#include "app_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "safety.h"
#include "sensor_inputs.h"

static const char *TAG = "sensor_tasks";

static void tec_monitor_task(void *arg)
{
  (void)arg;

  for (;;)
  {
    tec_status_t tec = {0};
    const bool valid = tec_read_status(&tec);

    safety_set_condition(SAFETY_BIT_TEC_READY, valid && tec.tec_ready);
    safety_set_condition(SAFETY_BIT_TEC_OK, valid && tec.tec_thermal_ok);

    vTaskDelay(pdMS_TO_TICKS(TEC_MONITOR_PERIOD_MS));
  }
}

static void imu_monitor_task(void *arg)
{
  (void)arg;

  for (;;)
  {
    imu_status_t imu = {0};
    const bool valid = imu_read_status(&imu);

    safety_set_condition(SAFETY_BIT_ORIENTATION_OK, valid && imu.orientation_ok);
    vTaskDelay(pdMS_TO_TICKS(IMU_MONITOR_PERIOD_MS));
  }
}

static void distance_monitor_task(void *arg)
{
  (void)arg;

  for (;;)
  {
    distance_status_t distance = {0};
    const bool valid = distance_read_status(&distance);

    safety_set_condition(SAFETY_BIT_DISTANCE_OK, valid && distance.distance_ok);
    vTaskDelay(pdMS_TO_TICKS(DISTANCE_MONITOR_PERIOD_MS));
  }
}

void sensor_tasks_start(void)
{
  xTaskCreatePinnedToCore(tec_monitor_task, "tec_mon", 3072, NULL, 8, NULL, 0);
  xTaskCreatePinnedToCore(imu_monitor_task, "imu_mon", 3072, NULL, 7, NULL, 0);
  xTaskCreatePinnedToCore(distance_monitor_task, "dist_mon", 3072, NULL, 7, NULL, 0);

  ESP_LOGI(TAG, "Sensor monitor tasks started (TEC/IMU/Distance)");
}

