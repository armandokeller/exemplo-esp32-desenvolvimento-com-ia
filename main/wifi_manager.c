#include "wifi_manager.h"
#include "captive_portal.h"
#include "storage_nvs.h"

#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "lwip/inet.h"

static const char *TAG = "wifi_manager";

#define WIFI_MAX_RETRIES 3
#define WIFI_RETRY_INTERVAL_MS 5000

typedef enum {
    WIFI_STATE_UNINITIALIZED = 0,
    WIFI_STATE_CONNECTING_STA,
    WIFI_STATE_CONNECTED_STA,
    WIFI_STATE_SOFTAP_PORTAL
} wifi_state_t;

static wifi_state_t s_current_state = WIFI_STATE_UNINITIALIZED;
static int s_retry_count = 0;
static wifi_mgr_event_cb_t s_event_cb = NULL;
static esp_netif_t *s_netif_sta = NULL;
static esp_netif_t *s_netif_ap = NULL;
static TimerHandle_t s_retry_timer = NULL;
static char s_ap_ssid[32] = "ESP32-Botoes";

static void start_sta_connection(void);
static void start_softap_portal(void);

static void retry_timer_callback(TimerHandle_t xTimer)
{
    if (s_current_state == WIFI_STATE_CONNECTING_STA) {
        ESP_LOGI(TAG, "Executando tentativa %d de %d para conectar ao Wi-Fi...", s_retry_count + 1, WIFI_MAX_RETRIES);
        s_retry_count++;
        esp_wifi_connect();
    }
}

static void on_portal_credentials_received(const char *ssid, const char *password)
{
    ESP_LOGI(TAG, "Novas credenciais recebidas pelo Captive Portal. Salvando na NVS...");
    storage_nvs_save_wifi_credentials(ssid, password);

    ESP_LOGI(TAG, "Encerrando Captive Portal e alternando para modo Station...");
    captive_portal_stop();

    s_retry_count = 0;
    start_sta_connection();
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            if (s_current_state == WIFI_STATE_CONNECTING_STA) {
                ESP_LOGI(TAG, "Driver Wi-Fi STA iniciado. Conectando...");
                esp_wifi_connect();
            }
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            if (s_current_state != WIFI_STATE_CONNECTING_STA && s_current_state != WIFI_STATE_CONNECTED_STA) {
                // Se estivermos em modo SoftAP/Portal, ignora desconexão do STA
                break;
            }
            ESP_LOGW(TAG, "Wi-Fi STA desconectado.");
            if (s_current_state == WIFI_STATE_CONNECTED_STA) {
                if (s_event_cb != NULL) {
                    s_event_cb(WIFI_MGR_EVENT_STA_DISCONNECTED);
                }
            }

            s_current_state = WIFI_STATE_CONNECTING_STA;

            if (s_retry_count < WIFI_MAX_RETRIES) {
                ESP_LOGI(TAG, "Tentativa %d/%d falhou. Nova tentativa em 5 segundos...", s_retry_count, WIFI_MAX_RETRIES);
                xTimerStart(s_retry_timer, 0);
            } else {
                ESP_LOGE(TAG, "Limite de %d tentativas excedido. Alternando para modo Captive Portal...", WIFI_MAX_RETRIES);
                start_softap_portal();
            }
            break;

        case WIFI_EVENT_AP_STACONNECTED: {
            wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
            ESP_LOGI(TAG, "Dispositivo conectado ao SoftAP (" MACSTR "), AID=%d", MAC2STR(event->mac), event->aid);
            break;
        }

        case WIFI_EVENT_AP_STADISCONNECTED: {
            wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
            ESP_LOGI(TAG, "Dispositivo desconectado do SoftAP (" MACSTR ")", MAC2STR(event->mac));
            break;
        }

        default:
            break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "Conexão Wi-Fi estabelecida! Endereço IP: " IPSTR, IP2STR(&event->ip_info.ip));
            s_retry_count = 0;
            s_current_state = WIFI_STATE_CONNECTED_STA;
            xTimerStop(s_retry_timer, 0);

            // Configurar servidores DNS públicos confiáveis (Google e Cloudflare)
            esp_netif_dns_info_t dns_main = {0};
            dns_main.ip.type = ESP_IPADDR_TYPE_V4;
            dns_main.ip.u_addr.ip4.addr = ipaddr_addr("8.8.8.8");
            esp_netif_set_dns_info(s_netif_sta, ESP_NETIF_DNS_MAIN, &dns_main);

            esp_netif_dns_info_t dns_backup = {0};
            dns_backup.ip.type = ESP_IPADDR_TYPE_V4;
            dns_backup.ip.u_addr.ip4.addr = ipaddr_addr("1.1.1.1");
            esp_netif_set_dns_info(s_netif_sta, ESP_NETIF_DNS_BACKUP, &dns_backup);

            esp_netif_dns_info_t dns_fallback = {0};
            dns_fallback.ip.type = ESP_IPADDR_TYPE_V4;
            dns_fallback.ip.u_addr.ip4.addr = ipaddr_addr("8.8.4.4");
            esp_netif_set_dns_info(s_netif_sta, ESP_NETIF_DNS_FALLBACK, &dns_fallback);

            if (s_event_cb != NULL) {
                s_event_cb(WIFI_MGR_EVENT_STA_CONNECTED);
            }
        }
    }
}

static void start_sta_connection(void)
{
    char ssid[STORAGE_MAX_SSID_LEN + 1] = {0};
    char pass[STORAGE_MAX_PASS_LEN + 1] = {0};

    esp_err_t err = storage_nvs_load_wifi_credentials(ssid, sizeof(ssid), pass, sizeof(pass));
    if (err != ESP_OK || strlen(ssid) == 0) {
        ESP_LOGW(TAG, "Nenhuma credencial Wi-Fi encontrada na NVS. Iniciando Captive Portal...");
        start_softap_portal();
        return;
    }

    ESP_LOGI(TAG, "Iniciando conexão Wi-Fi Station com SSID: '%s'", ssid);

    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_STA);

    wifi_config_t sta_config = {0};
    strncpy((char *)sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid) - 1);
    strncpy((char *)sta_config.sta.password, pass, sizeof(sta_config.sta.password) - 1);
    if (strlen(pass) == 0) {
        sta_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    } else {
        sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }
    sta_config.sta.pmf_cfg.capable = true;
    sta_config.sta.pmf_cfg.required = false;

    s_current_state = WIFI_STATE_CONNECTING_STA;
    s_retry_count = 1;

    esp_wifi_set_config(WIFI_IF_STA, &sta_config);
    esp_wifi_start();
}

static void start_softap_portal(void)
{
    s_current_state = WIFI_STATE_SOFTAP_PORTAL;
    xTimerStop(s_retry_timer, 0);

    ESP_LOGI(TAG, "Configurando SoftAP '%s' (IP: 192.168.4.1)...", s_ap_ssid);

    esp_wifi_stop();
    // Modo APSTA permite continuar escaneando redes enquanto o AP está ativo!
    esp_wifi_set_mode(WIFI_MODE_APSTA);

    wifi_config_t ap_config = {
        .ap = {
            .ssid_len = strlen(s_ap_ssid),
            .channel = 1,
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
        },
    };
    strncpy((char *)ap_config.ap.ssid, s_ap_ssid, sizeof(ap_config.ap.ssid) - 1);

    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();

    // Iniciar serviços do Captive Portal (DNS e HTTP Server)
    captive_portal_start(on_portal_credentials_received);

    if (s_event_cb != NULL) {
        s_event_cb(WIFI_MGR_EVENT_PORTAL_STARTED);
    }
}

esp_err_t wifi_manager_init(wifi_mgr_event_cb_t event_cb)
{
    s_event_cb = event_cb;

    // Gerar SSID único baseado nos últimos 2 bytes do MAC
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(s_ap_ssid, sizeof(s_ap_ssid), "ESP32-Botoes-%02X%02X", mac[4], mac[5]);
    ESP_LOGI(TAG, "Identificador SoftAP definido: %s", s_ap_ssid);

    s_netif_sta = esp_netif_create_default_wifi_sta();
    s_netif_ap = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    s_retry_timer = xTimerCreate("wifi_retry_tmr",
                                 pdMS_TO_TICKS(WIFI_RETRY_INTERVAL_MS),
                                 pdFALSE,
                                 NULL,
                                 retry_timer_callback);

    return ESP_OK;
}

esp_err_t wifi_manager_start(void)
{
    if (storage_nvs_has_wifi_credentials()) {
        s_retry_count = 0;
        start_sta_connection();
    } else {
        ESP_LOGI(TAG, "Iniciando diretamente em modo SoftAP Captive Portal");
        start_softap_portal();
    }
    return ESP_OK;
}

void wifi_manager_get_ap_ssid(char *out_ssid, size_t max_len)
{
    if (out_ssid != NULL && max_len > 0) {
        strncpy(out_ssid, s_ap_ssid, max_len - 1);
        out_ssid[max_len - 1] = '\0';
    }
}

bool wifi_manager_is_connected(void)
{
    return (s_current_state == WIFI_STATE_CONNECTED_STA);
}
