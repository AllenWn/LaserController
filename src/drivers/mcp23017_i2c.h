#pragma once

#include <stdint.h>

#include "driver/i2c.h"
#include "esp_err.h"

typedef struct
{
  i2c_port_t port;
  uint8_t addr_7bit;
} mcp23017_i2c_t;

typedef enum
{
  MCP23017_REG_IODIRA = 0x00,
  MCP23017_REG_IODIRB = 0x01,
  MCP23017_REG_IPOLA = 0x02,
  MCP23017_REG_IPOLB = 0x03,
  MCP23017_REG_GPINTENA = 0x04,
  MCP23017_REG_GPINTENB = 0x05,
  MCP23017_REG_DEFVALA = 0x06,
  MCP23017_REG_DEFVALB = 0x07,
  MCP23017_REG_INTCONA = 0x08,
  MCP23017_REG_INTCONB = 0x09,
  MCP23017_REG_IOCON = 0x0A,
  MCP23017_REG_GPPUA = 0x0C,
  MCP23017_REG_GPPUB = 0x0D,
  MCP23017_REG_INTFA = 0x0E,
  MCP23017_REG_INTFB = 0x0F,
  MCP23017_REG_INTCAPA = 0x10,
  MCP23017_REG_INTCAPB = 0x11,
  MCP23017_REG_GPIOA = 0x12,
  MCP23017_REG_GPIOB = 0x13,
  MCP23017_REG_OLATA = 0x14,
  MCP23017_REG_OLATB = 0x15,
} mcp23017_reg_t;

esp_err_t mcp23017_i2c_init(mcp23017_i2c_t *dev, i2c_port_t port, uint8_t addr_7bit);
esp_err_t mcp23017_i2c_write_reg8(const mcp23017_i2c_t *dev, mcp23017_reg_t reg, uint8_t value);
esp_err_t mcp23017_i2c_read_reg8(const mcp23017_i2c_t *dev, mcp23017_reg_t reg, uint8_t *out_value);
