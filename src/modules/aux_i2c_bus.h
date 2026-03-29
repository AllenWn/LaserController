#pragma once

#include <stdbool.h>

#include "driver/i2c.h"
#include "esp_err.h"

bool aux_i2c_bus_config_is_valid(void);
esp_err_t aux_i2c_bus_init(void);
i2c_port_t aux_i2c_bus_port(void);
bool aux_i2c_bus_lock(TickType_t timeout_ticks);
void aux_i2c_bus_unlock(void);
