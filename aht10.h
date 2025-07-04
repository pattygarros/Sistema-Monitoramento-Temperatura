#ifndef AHT10_H
#define AHT10_H

#include "pico/stdlib.h"
#include "hardware/i2c.h"

// Endereço I2C padrão do AHT10
#define AHT10_I2C_ADDR 0x38

// Estrutura para armazenar os dados do sensor
typedef struct {
    float temperature;
    float humidity;
} aht10_data_t;

// Funções da biblioteca
void aht10_init(i2c_inst_t *i2c_instance);
void aht10_read_data(i2c_inst_t *i2c_instance, aht10_data_t *data);
void aht10_reset(i2c_inst_t *i2c_instance);

#endif