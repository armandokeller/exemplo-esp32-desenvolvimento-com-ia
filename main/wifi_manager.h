#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

typedef enum {
    WIFI_MGR_EVENT_STA_CONNECTED = 0,
    WIFI_MGR_EVENT_STA_DISCONNECTED,
    WIFI_MGR_EVENT_PORTAL_STARTED
} wifi_mgr_event_t;

typedef void (*wifi_mgr_event_cb_t)(wifi_mgr_event_t event);

/**
 * @brief Inicializa a stack de rede e os drivers Wi-Fi.
 *
 * @param event_cb Callback para notificação de eventos de conectividade.
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t wifi_manager_init(wifi_mgr_event_cb_t event_cb);

/**
 * @brief Inicia o ciclo de conexão. Tenta STA se houver credenciais na NVS,
 * ou inicia o SoftAP com Captive Portal caso contrário.
 *
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t wifi_manager_start(void);

/**
 * @brief Obtém o SSID gerado para o SoftAP (ex: ESP32-Botoes-XXXX).
 *
 * @param out_ssid Buffer para escrita do SSID.
 * @param max_len Tamanho máximo do buffer.
 */
void wifi_manager_get_ap_ssid(char *out_ssid, size_t max_len);

/**
 * @brief Retorna se o ESP32 está atualmente conectado e com IP atribuído na rede STA.
 *
 * @return true se conectado, false caso contrário.
 */
bool wifi_manager_is_connected(void);
