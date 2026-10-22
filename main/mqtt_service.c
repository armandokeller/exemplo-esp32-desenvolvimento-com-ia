#include "mqtt_service.h"

#include <string.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "mqtt_client.h"

static const char *TAG = "mqtt_service";

static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static bool s_is_connected = false;
static char s_client_id[48] = {0};

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        s_is_connected = true;
        ESP_LOGI(TAG, "Conexão MQTT estabelecida com %s", MQTT_BROKER_URI);

        esp_mqtt_client_subscribe(client, MQTT_TOPIC_LED_RED, 0);
        esp_mqtt_client_subscribe(client, MQTT_TOPIC_LED_GREEN, 0);
        ESP_LOGI(TAG, "Inscrito nos tópicos: %s e %s", MQTT_TOPIC_LED_RED, MQTT_TOPIC_LED_GREEN);
        break;

    case MQTT_EVENT_DISCONNECTED:
        s_is_connected = false;
        ESP_LOGW(TAG, "Desconectado do broker MQTT. Tentando reconectar automaticamente...");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGD(TAG, "Confirmação de subscrição (msg_id=%d)", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "Mensagem MQTT recebida no tópico '%.*s': '%.*s'",
                 event->topic_len, event->topic, event->data_len, event->data);

        // Processamento do tópico do LED Vermelho
        if (event->topic_len == strlen(MQTT_TOPIC_LED_RED) &&
            strncmp(event->topic, MQTT_TOPIC_LED_RED, event->topic_len) == 0) {
            if (event->data_len == 2 && strncmp(event->data, "ON", 2) == 0) {
                gpio_controller_set_led(LED_RED, LED_STATE_ON);
            } else if (event->data_len == 3 && strncmp(event->data, "OFF", 3) == 0) {
                gpio_controller_set_led(LED_RED, LED_STATE_OFF);
            } else {
                ESP_LOGW(TAG, "Payload desconhecido para LED Vermelho: %.*s", event->data_len, event->data);
            }
        }
        // Processamento do tópico do LED Verde
        else if (event->topic_len == strlen(MQTT_TOPIC_LED_GREEN) &&
                 strncmp(event->topic, MQTT_TOPIC_LED_GREEN, event->topic_len) == 0) {
            if (event->data_len == 2 && strncmp(event->data, "ON", 2) == 0) {
                gpio_controller_set_led(LED_GREEN, LED_STATE_ON);
            } else if (event->data_len == 3 && strncmp(event->data, "OFF", 3) == 0) {
                gpio_controller_set_led(LED_GREEN, LED_STATE_OFF);
            } else {
                ESP_LOGW(TAG, "Payload desconhecido para LED Verde: %.*s", event->data_len, event->data);
            }
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "Erro na comunicação MQTT: tipo %d", event->error_handle->error_type);
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "Erro TCP Transport. errno: %d (%s)",
                     event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;

    default:
        break;
    }
}

esp_err_t mqtt_service_init(void)
{
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(s_client_id, sizeof(s_client_id), "ESP32-Botoes-%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    ESP_LOGI(TAG, "Inicializando MQTT (Broker: %s, ClientID: %s)", MQTT_BROKER_URI, s_client_id);

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.client_id = s_client_id,
        .network.reconnect_timeout_ms = 5000,
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "Falha ao alocar instância do cliente MQTT");
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    return ESP_OK;
}

esp_err_t mqtt_service_start(void)
{
    if (s_mqtt_client == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "Iniciando cliente MQTT...");
    esp_err_t err = esp_mqtt_client_start(s_mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao iniciar cliente MQTT: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t mqtt_service_stop(void)
{
    if (s_mqtt_client == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    s_is_connected = false;
    ESP_LOGI(TAG, "Parando cliente MQTT...");
    return esp_mqtt_client_stop(s_mqtt_client);
}

esp_err_t mqtt_service_publish_led_state(led_id_t led)
{
    if (s_mqtt_client == NULL || !s_is_connected) {
        ESP_LOGW(TAG, "Não é possível publicar: cliente MQTT desconectado");
        return ESP_ERR_INVALID_STATE;
    }

    const char *topic = (led == LED_RED) ? MQTT_TOPIC_LED_RED : MQTT_TOPIC_LED_GREEN;
    const char *payload = gpio_controller_get_led_state_str(led);

    int msg_id = esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 0, 0);
    if (msg_id >= 0) {
        ESP_LOGI(TAG, "Publicado com sucesso no tópico '%s': '%s' (msg_id=%d)", topic, payload, msg_id);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Falha ao publicar no tópico '%s'", topic);
        return ESP_FAIL;
    }
}

esp_err_t mqtt_service_publish_led_toggle(led_id_t led)
{
    if (s_mqtt_client == NULL || !s_is_connected) {
        ESP_LOGW(TAG, "Não é possível publicar: cliente MQTT desconectado");
        return ESP_ERR_INVALID_STATE;
    }

    const char *topic = (led == LED_RED) ? MQTT_TOPIC_LED_RED : MQTT_TOPIC_LED_GREEN;
    led_state_t current = gpio_controller_get_led(led);
    // Inverte o estado atual para solicitar a alternância via MQTT
    const char *payload = (current == LED_STATE_ON) ? "OFF" : "ON";

    int msg_id = esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 0, 0);
    if (msg_id >= 0) {
        ESP_LOGI(TAG, "Publicado pedido de alternância no tópico '%s': '%s' (msg_id=%d)", topic, payload, msg_id);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Falha ao publicar pedido de alternância no tópico '%s'", topic);
        return ESP_FAIL;
    }
}

bool mqtt_service_is_connected(void)
{
    return s_is_connected;
}
