#include "aht10.h"
#include <stdio.h>

// Envia um comando para o AHT10
static void aht10_send_command(i2c_inst_t *i2c_instance, uint8_t command) {
    uint8_t buf[] = {command, 0x00, 0x00};
    i2c_write_blocking(i2c_instance, AHT10_I2C_ADDR, buf, 3, false);
}

// Inicializa o sensor
void aht10_init(i2c_inst_t *i2c_instance) {
    sleep_ms(40); // Aguarda o sensor estabilizar após ligar
    aht10_reset(i2c_instance);
    sleep_ms(20);
    // Comando de calibração
    uint8_t init_cmd[] = {0xE1, 0x08, 0x00};
    i2c_write_blocking(i2c_instance, AHT10_I2C_ADDR, init_cmd, 3, false);
    sleep_ms(350);
}

// Reseta o sensor
void aht10_reset(i2c_inst_t *i2c_instance) {
    uint8_t cmd = 0xBA;
    i2c_write_blocking(i2c_instance, AHT10_I2C_ADDR, &cmd, 1, false);
    sleep_ms(20);
}

// Lê os dados de temperatura e umidade
void aht10_read_data(i2c_inst_t *i2c_instance, aht10_data_t *data) {
    // Dispara a medição
    aht10_send_command(i2c_instance, 0xAC);
    sleep_ms(80); // Aguarda a medição

    uint8_t raw_data[6];
    i2c_read_blocking(i2c_instance, AHT10_I2C_ADDR, raw_data, 6, false);

    // Converte os dados brutos
    uint32_t raw_humidity = ((uint32_t)raw_data[1] << 12) | ((uint32_t)raw_data[2] << 4) | ((raw_data[3] >> 4) & 0x0F);
    data->humidity = ((float)raw_humidity / (1 << 20)) * 100.0f;

    uint32_t raw_temp = (((uint32_t)raw_data[3] & 0x0F) << 16) | ((uint32_t)raw_data[4] << 8) | raw_data[5];
    data->temperature = (((float)raw_temp / (1 << 20)) * 200.0f) - 50.0f;
}