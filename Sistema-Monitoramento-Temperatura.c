#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "aht10.h"

// Definição dos pinos I2C
#define I2C_PORT i2c0
#define I2C_SDA_PIN 0
#define I2C_SCL_PIN 1

int main() {
    // Inicializa a E/S padrão (para printf via USB)
    stdio_init_all();

    // Aguarda um pouco para dar tempo de conectar o terminal serial
    sleep_ms(2000);
    printf("Iniciando leitor AHT10...\n");

    // Inicializa o I2C
    i2c_init(I2C_PORT, 400 * 1000); // 400kHz

    // Configura as funções dos pinos para I2C
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    // Inicializa o sensor AHT10
    aht10_init(I2C_PORT);

    aht10_data_t sensor_data;

    while (1) {
        // Lê os dados do sensor
        aht10_read_data(I2C_PORT, &sensor_data);

        // Imprime os dados no terminal serial
        printf("Temperatura: %.2f C\n", sensor_data.temperature);
        printf("Umidade:     %.2f %%\n", sensor_data.humidity);
        printf("---------------------\n");

        // Aguarda 5 segundos para a próxima leitura
        sleep_ms(5000);
    }

    return 0;
}