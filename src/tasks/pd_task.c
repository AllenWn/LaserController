#include "tasks/pd_task.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "drivers/stusb4500_i2c.h"
#include "modules/pd_i2c_bus.h"
#include "system_status.h"

static const char *TAG = "pd_task";

#ifndef PD_TASK_PERIOD_MS
#define PD_TASK_PERIOD_MS 200
#endif

#ifndef PD_STATUS_LOG_PERIOD_MS
#define PD_STATUS_LOG_PERIOD_MS 2000
#endif

#ifndef STUSB4500_I2C_ADDR_7BIT
#define STUSB4500_I2C_ADDR_7BIT 0x28
#endif

// STUSB4500 runtime policy:
// - Program three fixed Sink PDOs in descending priority order.
// - Re-negotiate automatically after runtime PDO programming.
// - MCU only validates whether the resulting contract matches one of these
//   approved profiles and therefore satisfies the >30 W bring-up requirement.
#ifndef STUSB4500_REQ_MIN_POWER_MW
#define STUSB4500_REQ_MIN_POWER_MW 30000u
#endif

#ifndef STUSB4500_SNK_PDO1_MV
#define STUSB4500_SNK_PDO1_MV 12000u
#endif

#ifndef STUSB4500_SNK_PDO1_MA
#define STUSB4500_SNK_PDO1_MA 3000u
#endif

#ifndef STUSB4500_SNK_PDO2_MV
#define STUSB4500_SNK_PDO2_MV 15000u
#endif

#ifndef STUSB4500_SNK_PDO2_MA
#define STUSB4500_SNK_PDO2_MA 3000u
#endif

#ifndef STUSB4500_SNK_PDO3_MV
#define STUSB4500_SNK_PDO3_MV 20000u
#endif

#ifndef STUSB4500_SNK_PDO3_MA
#define STUSB4500_SNK_PDO3_MA 3000u
#endif

// These register/bit values are treated as provisional until the exact
// STUSB4500 register map is confirmed against the final datasheet revision
// used by this project.
#ifndef STUSB4500_REG_ALERT_STATUS_1_TBD
#define STUSB4500_REG_ALERT_STATUS_1_TBD 0x0B
#endif

#ifndef STUSB4500_REG_PORT_STATUS_0_TBD
#define STUSB4500_REG_PORT_STATUS_0_TBD 0x0D
#endif

#ifndef STUSB4500_REG_PORT_STATUS_1_TBD
#define STUSB4500_REG_PORT_STATUS_1_TBD 0x0E
#endif

#ifndef STUSB4500_REG_TX_HEADER_LOW
#define STUSB4500_REG_TX_HEADER_LOW 0x51
#endif

#ifndef STUSB4500_REG_TX_HEADER_HIGH
#define STUSB4500_REG_TX_HEADER_HIGH 0x52
#endif

#ifndef STUSB4500_REG_DPM_PDO_NUMB
#define STUSB4500_REG_DPM_PDO_NUMB 0x70
#endif

#ifndef STUSB4500_REG_DPM_SNK_PDO1_0
#define STUSB4500_REG_DPM_SNK_PDO1_0 0x85
#endif

#ifndef STUSB4500_REG_DPM_SNK_PDO2_0
#define STUSB4500_REG_DPM_SNK_PDO2_0 0x89
#endif

#ifndef STUSB4500_REG_DPM_SNK_PDO3_0
#define STUSB4500_REG_DPM_SNK_PDO3_0 0x8D
#endif

#ifndef STUSB4500_REG_RDO_STATUS_0
#define STUSB4500_REG_RDO_STATUS_0 0x91
#endif

#ifndef STUSB4500_REG_RX_DATA_OBJ1_0
#define STUSB4500_REG_RX_DATA_OBJ1_0 0x33
#endif

#ifndef STUSB4500_REG_PD_COMMAND_CTRL
#define STUSB4500_REG_PD_COMMAND_CTRL 0x1A
#endif

#ifndef STUSB4500_PD_CMD_SOFT_RESET
#define STUSB4500_PD_CMD_SOFT_RESET 0x0Du
#endif

#ifndef STUSB4500_ALERT_FAULT_MASK_TBD
#define STUSB4500_ALERT_FAULT_MASK_TBD 0x1Fu
#endif

#ifndef STUSB4500_PORT_STATUS_ATTACHED_MASK_TBD
#define STUSB4500_PORT_STATUS_ATTACHED_MASK_TBD 0x01u
#endif

#ifndef STUSB4500_PORT_STATUS_CONTRACT_VALID_MASK_TBD
#define STUSB4500_PORT_STATUS_CONTRACT_VALID_MASK_TBD 0x08u
#endif

#ifndef STUSB4500_RDO_OBJECT_POS_SHIFT
#define STUSB4500_RDO_OBJECT_POS_SHIFT 28u
#endif

#ifndef STUSB4500_RDO_OBJECT_POS_MASK
#define STUSB4500_RDO_OBJECT_POS_MASK 0x7u
#endif

#ifndef STUSB4500_RDO_CAP_MISMATCH_MASK
#define STUSB4500_RDO_CAP_MISMATCH_MASK (1u << 26)
#endif

typedef struct
{
  uint32_t raw;
  uint16_t mv;
  uint16_t ma;
} stusb4500_sink_pdo_t;

typedef struct
{
  bool valid;
  uint16_t mv;
  uint16_t ma;
} stusb4500_fixed_pdo_info_t;

static bool pd_status_equal(const pd_status_t *a, const pd_status_t *b)
{
  return a->device_present == b->device_present && a->i2c_ok == b->i2c_ok && a->config_applied == b->config_applied &&
         a->attached_known == b->attached_known && a->attached == b->attached &&
         a->contract_valid_known == b->contract_valid_known && a->contract_valid == b->contract_valid &&
         a->contract_mismatch_known == b->contract_mismatch_known && a->contract_mismatch == b->contract_mismatch &&
         a->power_ready == b->power_ready && a->active_rdo_object_pos == b->active_rdo_object_pos &&
         a->fault_known == b->fault_known && a->fault == b->fault;
}

static bool read_reg_locked(const stusb4500_i2c_t *dev, uint8_t reg, uint8_t *value)
{
  return stusb4500_i2c_read_reg8(dev, reg, value) == ESP_OK;
}

static uint32_t encode_fixed_sink_pdo(uint16_t mv, uint16_t ma)
{
  const uint32_t voltage_units_50mv = (uint32_t)(mv / 50u) & 0x3FFu;
  const uint32_t current_units_10ma = (uint32_t)(ma / 10u) & 0x3FFu;
  return (voltage_units_50mv << 10) | current_units_10ma;
}

static stusb4500_fixed_pdo_info_t decode_fixed_pdo(uint32_t raw)
{
  stusb4500_fixed_pdo_info_t out = {0};
  const uint8_t supply_type = (uint8_t)((raw >> 30) & 0x3u);
  if (supply_type != 0u)
  {
    return out;
  }

  const uint16_t mv = (uint16_t)(((raw >> 10) & 0x3FFu) * 50u);
  const uint16_t ma = (uint16_t)((raw & 0x3FFu) * 10u);

  if (mv < 5000u || mv > 20000u || ma < 500u || ma > 5000u)
  {
    return out;
  }

  out.valid = true;
  out.mv = mv;
  out.ma = ma;
  return out;
}

static void pdo_to_le32(uint32_t value, uint8_t out[4])
{
  out[0] = (uint8_t)(value & 0xFFu);
  out[1] = (uint8_t)((value >> 8) & 0xFFu);
  out[2] = (uint8_t)((value >> 16) & 0xFFu);
  out[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static bool program_sink_pdo_locked(const stusb4500_i2c_t *dev, uint8_t reg_base, stusb4500_sink_pdo_t pdo)
{
  uint8_t payload[4];
  pdo_to_le32(pdo.raw, payload);
  return stusb4500_i2c_write_block(dev, reg_base, payload, sizeof(payload)) == ESP_OK;
}

static bool stusb4500_program_policy_locked(const stusb4500_i2c_t *dev, bool attached)
{
  stusb4500_sink_pdo_t cfg1 = {
      .raw = encode_fixed_sink_pdo(STUSB4500_SNK_PDO1_MV, STUSB4500_SNK_PDO1_MA),
      .mv = STUSB4500_SNK_PDO1_MV,
      .ma = STUSB4500_SNK_PDO1_MA,
  };
  stusb4500_sink_pdo_t cfg2 = {
      .raw = encode_fixed_sink_pdo(STUSB4500_SNK_PDO2_MV, STUSB4500_SNK_PDO2_MA),
      .mv = STUSB4500_SNK_PDO2_MV,
      .ma = STUSB4500_SNK_PDO2_MA,
  };
  stusb4500_sink_pdo_t cfg3 = {
      .raw = encode_fixed_sink_pdo(STUSB4500_SNK_PDO3_MV, STUSB4500_SNK_PDO3_MA),
      .mv = STUSB4500_SNK_PDO3_MV,
      .ma = STUSB4500_SNK_PDO3_MA,
  };

  if (stusb4500_i2c_write_reg8(dev, STUSB4500_REG_DPM_PDO_NUMB, 3u) != ESP_OK)
  {
    return false;
  }
  if (!program_sink_pdo_locked(dev, STUSB4500_REG_DPM_SNK_PDO1_0, cfg1))
  {
    return false;
  }
  if (!program_sink_pdo_locked(dev, STUSB4500_REG_DPM_SNK_PDO2_0, cfg2))
  {
    return false;
  }
  if (!program_sink_pdo_locked(dev, STUSB4500_REG_DPM_SNK_PDO3_0, cfg3))
  {
    return false;
  }

  if (attached)
  {
    // Runtime PDO edits do not take effect on the active contract until the
    // STUSB4500 is asked to renegotiate. The software programming guide uses a
    // USB-PD soft reset for that purpose.
    if (stusb4500_i2c_write_reg8(dev, STUSB4500_REG_TX_HEADER_LOW, 0x0Du) != ESP_OK)
    {
      return false;
    }
    if (stusb4500_i2c_write_reg8(dev, STUSB4500_REG_TX_HEADER_HIGH, 0x00u) != ESP_OK)
    {
      return false;
    }
    if (stusb4500_i2c_write_reg8(dev, STUSB4500_REG_PD_COMMAND_CTRL, STUSB4500_PD_CMD_SOFT_RESET) != ESP_OK)
    {
      return false;
    }
  }

  ESP_LOGI(TAG, "pd policy applied: PDO3=%umV/%umA PDO2=%umV/%umA PDO1=%umV/%umA", cfg3.mv, cfg3.ma, cfg2.mv, cfg2.ma,
           cfg1.mv, cfg1.ma);
  return true;
}

static bool read_source_pdos_locked(const stusb4500_i2c_t *dev, stusb4500_fixed_pdo_info_t out_pdos[4])
{
  uint8_t raw_buf[16] = {0};
  if (stusb4500_i2c_read_block(dev, STUSB4500_REG_RX_DATA_OBJ1_0, raw_buf, sizeof(raw_buf)) != ESP_OK)
  {
    return false;
  }

  bool any_valid = false;
  for (size_t i = 0; i < 4; ++i)
  {
    const size_t base = i * 4u;
    const uint32_t raw = (uint32_t)raw_buf[base] | ((uint32_t)raw_buf[base + 1u] << 8) |
                         ((uint32_t)raw_buf[base + 2u] << 16) | ((uint32_t)raw_buf[base + 3u] << 24);
    out_pdos[i] = decode_fixed_pdo(raw);
    any_valid |= out_pdos[i].valid;
  }
  return any_valid;
}

static void pd_task(void *arg)
{
  (void)arg;

  if (!pd_i2c_bus_config_is_valid())
  {
    ESP_LOGW(TAG, "PD task disabled: PD I2C pins not valid");
    vTaskDelete(NULL);
    return;
  }

  ESP_ERROR_CHECK(pd_i2c_bus_init());

  stusb4500_i2c_t dev = {0};
  ESP_ERROR_CHECK(stusb4500_i2c_init(&dev, pd_i2c_bus_port(), STUSB4500_I2C_ADDR_7BIT));

  TickType_t last_wake = xTaskGetTickCount();
  pd_status_t last_status = {0};
  bool have_last_status = false;
  bool policy_applied = false;
  bool source_logged = false;

  for (;;)
  {
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(PD_TASK_PERIOD_MS));

    pd_status_t st;
    memset(&st, 0, sizeof(st));

    if (pd_i2c_bus_lock(pdMS_TO_TICKS(50)))
    {
      const esp_err_t err = stusb4500_i2c_probe(&dev);
      st.i2c_ok = (err == ESP_OK);
      st.device_present = st.i2c_ok;

      if (st.i2c_ok)
      {
        uint8_t alert_status_1 = 0;
        uint8_t port_status_0 = 0;
        uint8_t port_status_1 = 0;
        uint8_t rdo_bytes[4] = {0};

        const bool have_alert = read_reg_locked(&dev, STUSB4500_REG_ALERT_STATUS_1_TBD, &alert_status_1);
        const bool have_port0 = read_reg_locked(&dev, STUSB4500_REG_PORT_STATUS_0_TBD, &port_status_0);
        const bool have_port1 = read_reg_locked(&dev, STUSB4500_REG_PORT_STATUS_1_TBD, &port_status_1);
        const bool have_rdo = (stusb4500_i2c_read_block(&dev, STUSB4500_REG_RDO_STATUS_0, rdo_bytes, sizeof(rdo_bytes)) == ESP_OK);

        st.fault_known = have_alert;
        if (have_alert)
        {
          st.fault = (alert_status_1 & STUSB4500_ALERT_FAULT_MASK_TBD) != 0u;
        }

        st.attached_known = have_port0;
        if (have_port0)
        {
          st.attached = (port_status_0 & STUSB4500_PORT_STATUS_ATTACHED_MASK_TBD) != 0u;
        }

        st.contract_valid_known = have_port1;
        if (have_port1)
        {
          st.contract_valid = (port_status_1 & STUSB4500_PORT_STATUS_CONTRACT_VALID_MASK_TBD) != 0u;
        }

        if (!policy_applied)
        {
          policy_applied = stusb4500_program_policy_locked(&dev, st.attached_known && st.attached);
        }
        st.config_applied = policy_applied;

        if (st.attached_known && st.attached && !source_logged)
        {
          stusb4500_fixed_pdo_info_t src_pdos[4] = {0};
          if (read_source_pdos_locked(&dev, src_pdos))
          {
            char buf[160];
            int off = snprintf(buf, sizeof(buf), "pd src caps:");
            for (size_t i = 0; i < 4 && off > 0 && off < (int)sizeof(buf); ++i)
            {
              if (!src_pdos[i].valid)
              {
                continue;
              }
              off += snprintf(&buf[off], sizeof(buf) - (size_t)off, " src%u=%umV/%umA", (unsigned)(i + 1u), src_pdos[i].mv,
                              src_pdos[i].ma);
            }
            ESP_LOGI(TAG, "%s", buf);
            source_logged = true;
          }
        }

        if (have_rdo)
        {
          const uint32_t rdo = (uint32_t)rdo_bytes[0] | ((uint32_t)rdo_bytes[1] << 8) |
                               ((uint32_t)rdo_bytes[2] << 16) | ((uint32_t)rdo_bytes[3] << 24);
          st.active_rdo_object_pos = (uint8_t)((rdo >> STUSB4500_RDO_OBJECT_POS_SHIFT) & STUSB4500_RDO_OBJECT_POS_MASK);
          st.contract_mismatch_known = true;
          st.contract_mismatch = (rdo & STUSB4500_RDO_CAP_MISMATCH_MASK) != 0u;
          st.power_ready = st.contract_valid && st.active_rdo_object_pos != 0u && !st.contract_mismatch;
        }
      }
      else
      {
        policy_applied = false;
        source_logged = false;
      }

      pd_i2c_bus_unlock();
    }
    else
    {
      st.i2c_ok = false;
      st.device_present = false;
    }

    const TickType_t now = xTaskGetTickCount();
    system_status_update_pd(&st, now);

    if (!have_last_status || !pd_status_equal(&st, &last_status))
    {
      const char *selected = "none";
      switch (st.active_rdo_object_pos)
      {
        case 1:
          selected = "12V/3A";
          break;
        case 2:
          selected = "15V/3A";
          break;
        case 3:
          selected = "20V/3A";
          break;
        default:
          break;
      }
      ESP_LOGI(TAG,
               "pd present=%d i2c_ok=%d cfg=%d attach=%d/%d contract=%d/%d mismatch=%d/%d rdo=%u sel=%s power_ok=%d fault=%d/%d",
               st.device_present ? 1 : 0, st.i2c_ok ? 1 : 0, st.attached_known ? 1 : 0, st.attached ? 1 : 0,
               st.config_applied ? 1 : 0, st.contract_valid_known ? 1 : 0, st.contract_valid ? 1 : 0,
               st.contract_mismatch_known ? 1 : 0, st.contract_mismatch ? 1 : 0, (unsigned)st.active_rdo_object_pos,
               selected, st.power_ready ? 1 : 0, st.fault_known ? 1 : 0, st.fault ? 1 : 0);
      last_status = st;
      have_last_status = true;
    }
  }
}

void pd_task_start(void)
{
  xTaskCreatePinnedToCore(pd_task, "pd_task", 4096, NULL, 5, NULL, 0);
  ESP_LOGI(TAG, "PD task started");
}
