#include "captive_portal.h"
#include "captive_portal_html.h"

#include <string.h>
#include <sys/param.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

static const char *TAG = "captive_portal";

#define DNS_PORT 53
#define DNS_MAX_LEN 256
#define AP_IP_RAW 0xC0A80401 // 192.168.4.1 em hexadecimal

static httpd_handle_t s_http_server = NULL;
static TaskHandle_t s_dns_task = NULL;
static volatile bool s_dns_running = false;
static captive_portal_cred_cb_t s_on_credentials_cb = NULL;

// Decodificação URL simples (para formulários application/x-www-form-urlencoded)
static void url_decode(char *dst, const char *src, size_t dst_len)
{
    char a, b;
    size_t d_idx = 0;
    while (*src && d_idx < dst_len - 1) {
        if ((*src == '%') && ((a = src[1]) && (b = src[2])) && (isxdigit((int)a) && isxdigit((int)b))) {
            if (a >= 'a') a -= 'a' - 'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a' - 'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            dst[d_idx++] = 16 * a + b;
            src += 3;
        } else if (*src == '+') {
            dst[d_idx++] = ' ';
            src++;
        } else {
            dst[d_idx++] = *src++;
        }
    }
    dst[d_idx] = '\0';
}

// Tarefa do servidor DNS Redirecionador (Captive Portal DNS)
static int dns_build_response(const uint8_t *req, int req_len, uint8_t *resp, int resp_max, uint32_t redirect_ip)
{
    if (req_len < 12 || resp_max < req_len + 16) {
        return -1;
    }
    memcpy(resp, req, req_len);
    resp[2] = 0x81; // Flags: Response, Opcode 0
    resp[3] = 0x80;
    resp[6] = 0x00; // Answer count = 1
    resp[7] = 0x01;

    int pos = req_len;
    resp[pos++] = 0xC0; resp[pos++] = 0x0C; // Pointer para o nome da questão
    resp[pos++] = 0x00; resp[pos++] = 0x01; // Tipo A
    resp[pos++] = 0x00; resp[pos++] = 0x01; // Classe IN
    resp[pos++] = 0x00; resp[pos++] = 0x00; resp[pos++] = 0x00; resp[pos++] = 0x3C; // TTL: 60s
    resp[pos++] = 0x00; resp[pos++] = 0x04; // Tamanho do IP: 4 bytes
    resp[pos++] = (redirect_ip >> 24) & 0xFF;
    resp[pos++] = (redirect_ip >> 16) & 0xFF;
    resp[pos++] = (redirect_ip >>  8) & 0xFF;
    resp[pos++] = (redirect_ip      ) & 0xFF;
    return pos;
}

static void dns_server_task(void *pvParameters)
{
    uint8_t rx_buffer[DNS_MAX_LEN];
    uint8_t tx_buffer[DNS_MAX_LEN + 16];
    struct sockaddr_in dest_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons(DNS_PORT),
    };

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Falha ao criar socket DNS: errno %d", errno);
        s_dns_running = false;
        vTaskDelete(NULL);
        return;
    }

    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        ESP_LOGE(TAG, "Falha no bind da porta DNS 53: errno %d", errno);
        close(sock);
        s_dns_running = false;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Servidor DNS Captive iniciado na porta 53");

    while (s_dns_running) {
        struct sockaddr_in source_addr;
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer), 0, (struct sockaddr *)&source_addr, &socklen);

        if (len > 0) {
            int resp_len = dns_build_response(rx_buffer, len, tx_buffer, sizeof(tx_buffer), AP_IP_RAW);
            if (resp_len > 0) {
                sendto(sock, tx_buffer, resp_len, 0, (struct sockaddr *)&source_addr, socklen);
            }
        }
    }

    close(sock);
    ESP_LOGI(TAG, "Servidor DNS encerrado");
    vTaskDelete(NULL);
}

// Handlers HTTP
static esp_err_t http_root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, CAPTIVE_PORTAL_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t http_redirect_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t http_scan_handler(httpd_req_t *req)
{
    char json_buf[1024];
    captive_portal_scan_networks_json(json_buf, sizeof(json_buf));

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json_buf, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

typedef struct {
    char ssid[64];
    char password[64];
} cred_task_arg_t;

static void notify_credentials_task(void *pvParameters)
{
    cred_task_arg_t *arg = (cred_task_arg_t *)pvParameters;
    vTaskDelay(pdMS_TO_TICKS(1000)); // Aguarda envio completo da resposta HTTP
    if (s_on_credentials_cb != NULL) {
        s_on_credentials_cb(arg->ssid, arg->password);
    }
    free(arg);
    vTaskDelete(NULL);
}

static esp_err_t http_connect_handler(httpd_req_t *req)
{
    char content[256];
    int total_len = req->content_len;
    int cur_len = 0;
    int received = 0;

    if (total_len >= sizeof(content)) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    while (cur_len < total_len) {
        received = httpd_req_recv(req, content + cur_len, total_len - cur_len);
        if (received <= 0) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        cur_len += received;
    }
    content[total_len] = '\0';

    char raw_ssid[64] = {0};
    char raw_pass[64] = {0};
    char decoded_ssid[64] = {0};
    char decoded_pass[64] = {0};

    char *ssid_ptr = strstr(content, "ssid=");
    if (ssid_ptr) {
        ssid_ptr += 5;
        char *end = strchr(ssid_ptr, '&');
        if (end) {
            size_t len = MIN((size_t)(end - ssid_ptr), sizeof(raw_ssid) - 1);
            strncpy(raw_ssid, ssid_ptr, len);
        } else {
            strncpy(raw_ssid, ssid_ptr, sizeof(raw_ssid) - 1);
        }
    }

    char *pass_ptr = strstr(content, "password=");
    if (pass_ptr) {
        pass_ptr += 9;
        char *end = strchr(pass_ptr, '&');
        if (end) {
            size_t len = MIN((size_t)(end - pass_ptr), sizeof(raw_pass) - 1);
            strncpy(raw_pass, pass_ptr, len);
        } else {
            strncpy(raw_pass, pass_ptr, sizeof(raw_pass) - 1);
        }
    }

    url_decode(decoded_ssid, raw_ssid, sizeof(decoded_ssid));
    url_decode(decoded_pass, raw_pass, sizeof(decoded_pass));

    ESP_LOGI(TAG, "Credenciais recebidas do portal: SSID='%s'", decoded_ssid);

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "OK");

    cred_task_arg_t *arg = malloc(sizeof(cred_task_arg_t));
    if (arg != NULL) {
        strncpy(arg->ssid, decoded_ssid, sizeof(arg->ssid) - 1);
        strncpy(arg->password, decoded_pass, sizeof(arg->password) - 1);
        xTaskCreate(notify_credentials_task, "cred_task", 3072, arg, 5, NULL);
    }

    return ESP_OK;
}

esp_err_t captive_portal_scan_networks_json(char *json_buf, size_t max_len)
{
    wifi_scan_config_t scan_config = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = true,
    };

    ESP_LOGI(TAG, "Iniciando varredura Wi-Fi...");
    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha na varredura Wi-Fi: %s", esp_err_to_name(err));
        snprintf(json_buf, max_len, "[]");
        return err;
    }

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count > 15) {
        ap_count = 15;
    }

    wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * (ap_count > 0 ? ap_count : 1));
    if (ap_list == NULL) {
        snprintf(json_buf, max_len, "[]");
        return ESP_ERR_NO_MEM;
    }

    esp_wifi_scan_get_ap_records(&ap_count, ap_list);

    size_t offset = snprintf(json_buf, max_len, "[");
    for (int i = 0; i < ap_count; i++) {
        if (strlen((char *)ap_list[i].ssid) == 0) continue;
        offset += snprintf(json_buf + offset, max_len - offset,
                           "%s{\"ssid\":\"%s\",\"rssi\":%d}",
                           (offset > 1) ? "," : "",
                           (char *)ap_list[i].ssid,
                           ap_list[i].rssi);
        if (offset >= max_len - 32) break;
    }
    snprintf(json_buf + offset, max_len - offset, "]");

    free(ap_list);
    ESP_LOGI(TAG, "Varredura concluída, %d redes encontradas", ap_count);
    return ESP_OK;
}

esp_err_t captive_portal_start(captive_portal_cred_cb_t on_credentials)
{
    s_on_credentials_cb = on_credentials;

    // 1. Iniciar servidor DNS
    if (!s_dns_running) {
        s_dns_running = true;
        xTaskCreate(dns_server_task, "dns_task", 3072, NULL, 5, &s_dns_task);
    }

    // 2. Iniciar servidor HTTP
    if (s_http_server == NULL) {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.max_uri_handlers = 12;
        config.stack_size = 4096;

        esp_err_t err = httpd_start(&s_http_server, &config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Falha ao iniciar servidor HTTP: %s", esp_err_to_name(err));
            return err;
        }

        httpd_uri_t uri_root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = http_root_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(s_http_server, &uri_root);

        httpd_uri_t uri_scan = {
            .uri = "/scan",
            .method = HTTP_GET,
            .handler = http_scan_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(s_http_server, &uri_scan);

        httpd_uri_t uri_connect = {
            .uri = "/connect",
            .method = HTTP_POST,
            .handler = http_connect_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(s_http_server, &uri_connect);

        // URIs de redirecionamento automático (Captive Detection)
        const char *redirect_endpoints[] = {
            "/generate_204",
            "/gen_204",
            "/hotspot-detect.html",
            "/ncsi.txt",
            "/connecttest.txt",
            "/redirect"
        };
        for (int i = 0; i < sizeof(redirect_endpoints)/sizeof(redirect_endpoints[0]); i++) {
            httpd_uri_t uri_redirect = {
                .uri = redirect_endpoints[i],
                .method = HTTP_GET,
                .handler = http_redirect_handler,
                .user_ctx = NULL
            };
            httpd_register_uri_handler(s_http_server, &uri_redirect);
        }

        ESP_LOGI(TAG, "Servidor Web Captive Portal iniciado em http://192.168.4.1/");
    }

    return ESP_OK;
}

void captive_portal_stop(void)
{
    if (s_dns_running) {
        s_dns_running = false;
        // O socket tem timeout de 1s e fechará sozinho
    }

    if (s_http_server != NULL) {
        httpd_stop(s_http_server);
        s_http_server = NULL;
        ESP_LOGI(TAG, "Servidor Web Captive Portal parado");
    }
}
