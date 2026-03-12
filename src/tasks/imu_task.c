#include "tasks/imu_task.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "modules/imu_monitor.h"
#include "system_status.h"

static const char *TAG = "imu_task";

#ifndef IMU_TASK_PERIOD_MS
#define IMU_TASK_PERIOD_MS 20
#endif

// TBD: set this based on mechanical integration (which IMU axis is the laser axis).
#ifndef IMU_LASER_AXIS_TBD
#define IMU_LASER_AXIS_TBD IMU_AXIS_Z_NEG
#endif

static void imu_task(void *arg)
{
  (void)arg;

  const imu_monitor_config_t cfg = {
      .host = SPI3_HOST,
      .sclk_gpio = (int)IMU_SCLK_GPIO,
      .mosi_gpio = (int)IMU_SDI_GPIO,
      .miso_gpio = (int)IMU_SDO_GPIO,
      .cs_gpio = (int)IMU_CS_GPIO,
      .spi_mode = 0,
      .clock_hz = 10000000,
      .laser_axis = IMU_LASER_AXIS_TBD,
      .xl_full_scale_g = IMU_XL_FS_2G,
      .accel_mag_min_g = 0.5f,
      .accel_mag_max_g = 1.5f,
      .max_angle_from_down_deg = 60.0f,
      .hysteresis_deg = 5.0f,
      .fault_debounce_samples = 1,
      .recover_debounce_samples = 3,
  };

  const esp_err_t err = imu_monitor_init(&cfg);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "IMU init failed: %d", (int)err);
  }
  else
  {
    ESP_LOGI(TAG, "IMU init OK (axis=%d)", (int)cfg.laser_axis);
  }

  TickType_t last = xTaskGetTickCount();
  for (;;)
  {
    vTaskDelayUntil(&last, pdMS_TO_TICKS(IMU_TASK_PERIOD_MS));
    const TickType_t now = xTaskGetTickCount();

    imu_monitor_status_t imu = {0};
    const esp_err_t e = imu_monitor_sample(&imu);

    imu_status_t st = {
        .valid = (e == ESP_OK) ? imu.valid : false,
        .orientation_ok = (e == ESP_OK) ? imu.orientation_ok : false,
        .angle_from_down_deg = (e == ESP_OK) ? imu.angle_from_down_deg : 0.0f,
        .updated_at = now,
    };
    system_status_update_imu(&st, now);
  }
}

void imu_task_start(void)
{
  xTaskCreatePinnedToCore(imu_task, "imu", 4096, NULL, 9, NULL, 0);
  ESP_LOGI(TAG, "IMU task started (period=%dms)", (int)IMU_TASK_PERIOD_MS);
}

