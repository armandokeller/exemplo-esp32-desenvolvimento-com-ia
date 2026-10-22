#include "storage_nvs.h"

#include <string.h>
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "storage_nvs";

esp_err_t storage_nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Partição NVS cheia ou formato desatualizado. Formatando...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "NVS inicializada com sucesso");
    } else {
        ESP_LOGE(TAG, "Falha ao inicializar NVS: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t storage_nvs_save_wifi_credentials(const char *ssid, const char *password)
{
    if (ssid == NULL || password == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(STORAGE_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao abrir namespace NVS '%s': %s", STORAGE_NVS_NAMESPACE, esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(handle, STORAGE_NVS_KEY_SSID, ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, STORAGE_NVS_KEY_PASS, password);
    }

    if (err == ESP_OK) {
        err = nvs_commit(handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Credenciais Wi-Fi salvas na NVS (SSID: %s)", ssid);
        }
    }

    nvs_close(handle);
    return err;
}

esp_err_t storage_nvs_load_wifi_credentials(char *ssid, size_t max_ssid_len,
                                            char *password, size_t max_pass_len)
{
    if (ssid == NULL || password == NULL || max_ssid_len == 0 || max_pass_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(STORAGE_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    size_t req_ssid_len = max_ssid_len;
    err = nvs_get_str(handle, STORAGE_NVS_KEY_SSID, ssid, &req_ssid_len);
    if (err == ESP_OK) {
        size_t req_pass_len = max_pass_len;
        err = nvs_get_str(handle, STORAGE_NVS_KEY_PASS, password, &req_pass_len);
    }

    nvs_close(handle);
    return err;
}

bool storage_nvs_has_wifi_credentials(void)
{
    char ssid[STORAGE_MAX_SSID_LEN + 1] = {0};
    char pass[STORAGE_MAX_PASS_LEN + 1] = {0};
    esp_err_t err = storage_nvs_load_wifi_credentials(ssid, sizeof(ssid), pass, sizeof(pass));
    return (err == ESP_OK && strlen(ssid) > 0);
}

esp_err_t storage_nvs_clear_wifi_credentials(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(STORAGE_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    nvs_erase_key(handle, STORAGE_NVS_KEY_SSID);
    nvs_erase_key(handle, STORAGE_NVS_KEY_PASS);
    err = nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGI(TAG, "Credenciais Wi-Fi removidas da NVS");
    return err;
}
