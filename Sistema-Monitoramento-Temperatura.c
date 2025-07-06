#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/i2c.h"

// Includes para Rede e MQTT
#include "lwip/apps/mqtt.h"
#include "lwip/dns.h" // <-- NECESSÁRIO PARA RESOLVER O HOSTNAME

// --- CONFIGURAÇÕES DO USUÁRIO ---
#define WIFI_SSID       "x"
#define WIFI_PASSWORD   "x"

// ALTERAÇÃO: Voltamos a usar o hostname (o "link")
#define MQTT_SERVER_HOST "mqtt.thingsboard.cloud"

#define MQTT_USERNAME   "x"
#define MQTT_TOPIC      "v1/devices/me/telemetry"

// ... (código do sensor AHT10 permanece o mesmo) ...
bool read_aht10(float *temperature, float *humidity) {
    // (cole aqui a função read_aht10 da resposta anterior)
    uint8_t buffer[6];
    uint8_t trigger_cmd[] = {0xAC, 0x33, 0x00};
    if (i2c_write_blocking(i2c0, 0x38, trigger_cmd, 3, false) != 3) return false;
    sleep_ms(80);
    if (i2c_read_blocking(i2c0, 0x38, buffer, 6, false) != 6) return false;
    if ((buffer[0] & 0x80) != 0) return false;
    uint32_t raw_humidity = ((buffer[1] << 16) | (buffer[2] << 8) | buffer[3]) >> 4;
    uint32_t raw_temp = ((buffer[3] & 0x0F) << 16) | (buffer[4] << 8) | buffer[5];
    *humidity    = ((float)raw_humidity / 1048576.0) * 100.0;
    *temperature = ((float)raw_temp / 1048576.0) * 200.0 - 50.0;
    return true;
}


// --- LÓGICA MQTT E DNS ---
typedef struct MQTT_CLIENT_T_ {
    ip_addr_t remote_ip;
    mqtt_client_t *mqtt_client;
    bool dns_resolved;
} MQTT_CLIENT_T;

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);

// Função chamada quando o DNS encontra o IP
static void dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    MQTT_CLIENT_T *state = (MQTT_CLIENT_T *)callback_arg;
    if (ipaddr != NULL) {
        state->remote_ip = *ipaddr;
        state->dns_resolved = true;
        printf("DNS resolvido para %s: %s\n", name, ip4addr_ntoa(ipaddr));
        
        // AGORA que temos o IP, tentamos conectar ao MQTT
        struct mqtt_connect_client_info_t ci;
        memset(&ci, 0, sizeof(ci));
        ci.client_id = "pico_w_client";
        ci.client_user = MQTT_USERNAME;
        
        mqtt_client_connect(state->mqtt_client, &state->remote_ip, 1883, mqtt_connection_cb, state, &ci);

    } else {
        printf("Falha ao resolver DNS para %s\n", name);
    }
}

// Função chamada quando a conexão MQTT é estabelecida (ou falha)
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf(">>> SUCESSO! Conexão MQTT estabelecida! <<<\n");
    } else {
        printf(">>> FALHA! Falha na conexão MQTT, status: %d <<<\n", status);
    }
}

void publish_data(MQTT_CLIENT_T *state, const char *payload) {
    // ... (função de publicar dados, sem alterações)
    printf("Publicando: %s\n", payload);
    mqtt_publish(state->mqtt_client, MQTT_TOPIC, payload, strlen(payload), 1, 0, NULL, NULL);
}

int main() {
    stdio_init_all();
    
    // ... (inicialização do I2C e Wi-Fi, sem alterações) ...
    i2c_init(i2c0, 100*1000); // init i2c
    gpio_set_function(0, GPIO_FUNC_I2C);
    gpio_set_function(1, GPIO_FUNC_I2C);
    gpio_pull_up(0);
    gpio_pull_up(1);
    
    cyw43_arch_init();
    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi: %s...\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Falha ao conectar ao Wi-Fi.\n");
        return -1;
    }
    printf("Conectado ao Wi-Fi!\n");
    
    // Alocação do estado
    MQTT_CLIENT_T *state = calloc(1, sizeof(MQTT_CLIENT_T));
    state->mqtt_client = mqtt_client_new();

    printf("Iniciando resolução de DNS para %s...\n", MQTT_SERVER_HOST);
    cyw43_arch_lwip_begin();
    dns_gethostbyname(MQTT_SERVER_HOST, &state->remote_ip, dns_found_cb, state);
    cyw43_arch_lwip_end();

    // Loop principal
    char payload[100];
    float temp, hum;
    while (true) {
        if (mqtt_client_is_connected(state->mqtt_client)) {
            if (read_aht10(&temp, &hum)) {
                snprintf(payload, sizeof(payload), "{\"temperature\":%.2f, \"humidity\":%.2f}", temp, hum);
                publish_data(state, payload);
            }
        }
        sleep_ms(60000);
    }
}