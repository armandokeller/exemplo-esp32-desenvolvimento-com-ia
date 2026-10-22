#pragma once

#include "esp_err.h"

typedef void (*captive_portal_cred_cb_t)(const char *ssid, const char *password);

/**
 * @brief Inicia o servidor HTTP do Captive Portal e o servidor DNS redirecionador.
 *
 * @param on_credentials Callback chamado quando o usuário submeter SSID e senha válidos.
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t captive_portal_start(captive_portal_cred_cb_t on_credentials);

/**
 * @brief Para o servidor HTTP e encerra a tarefa do DNS do Captive Portal.
 */
void captive_portal_stop(void);

/**
 * @brief Executa varredura de redes Wi-Fi e formata resultado em JSON.
 *
 * @param json_buf Buffer de saída para o JSON.
 * @param max_len Tamanho máximo do buffer.
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t captive_portal_scan_networks_json(char *json_buf, size_t max_len);
