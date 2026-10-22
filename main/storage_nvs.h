#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#define STORAGE_NVS_NAMESPACE "wifi_cfg"
#define STORAGE_NVS_KEY_SSID  "ssid"
#define STORAGE_NVS_KEY_PASS  "pass"

#define STORAGE_MAX_SSID_LEN 32
#define STORAGE_MAX_PASS_LEN 64

/**
 * @brief Inicializa a partição NVS padrão.
 * Em caso de partição cheia ou truncada, formata e reinicializa.
 *
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t storage_nvs_init(void);

/**
 * @brief Grava credenciais Wi-Fi na NVS.
 *
 * @param ssid Nome da rede (null-terminated string).
 * @param password Senha da rede (null-terminated string).
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t storage_nvs_save_wifi_credentials(const char *ssid, const char *password);

/**
 * @brief Carrega as credenciais Wi-Fi armazenadas na NVS.
 *
 * @param ssid Buffer de saída para o SSID.
 * @param max_ssid_len Tamanho máximo do buffer do SSID.
 * @param password Buffer de saída para a senha.
 * @param max_pass_len Tamanho máximo do buffer de senha.
 * @return ESP_OK se carregado com sucesso, ESP_ERR_NVS_NOT_FOUND se não existirem.
 */
esp_err_t storage_nvs_load_wifi_credentials(char *ssid, size_t max_ssid_len,
                                            char *password, size_t max_pass_len);

/**
 * @brief Verifica se há credenciais Wi-Fi válidas salvas.
 *
 * @return true se existirem credenciais com SSID não vazio, false caso contrário.
 */
bool storage_nvs_has_wifi_credentials(void);

/**
 * @brief Remove as credenciais Wi-Fi armazenadas na NVS.
 *
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t storage_nvs_clear_wifi_credentials(void);
