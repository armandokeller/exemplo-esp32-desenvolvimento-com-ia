#include <stdio.h>
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "storage_nvs.h"
#include "gpio_controller.h"
#include "wifi_manager.h"
#include "mqtt_service.h"

static const char *TAG = "main_app";

static QueueHandle_t s_button_queue = NULL;

static void on_wifi_manager_event(wifi_mgr_event_t event)
{
    switch (event) {
    case WIFI_MGR_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG, "Conexão Wi-Fi ativa. Iniciando serviço MQTT...");
        mqtt_service_start();
        break;

    case WIFI_MGR_EVENT_STA_DISCONNECTED:
        ESP_LOGW(TAG, "Wi-Fi desconectado. Pausando serviço MQTT...");
        mqtt_service_stop();
        break;

    case WIFI_MGR_EVENT_PORTAL_STARTED: {
        char ap_ssid[32] = {0};
        wifi_manager_get_ap_ssid(ap_ssid, sizeof(ap_ssid));
        ESP_LOGI(TAG, "=================================================");
        ESP_LOGI(TAG, ">>> CAPTIVE PORTAL ATIVADO <<<");
        ESP_LOGI(TAG, "1. Conecte seu celular/PC à rede Wi-Fi: '%s'", ap_ssid);
        ESP_LOGI(TAG, "2. Abra o navegador e acesse: http://192.168.4.1");
        ESP_LOGI(TAG, "3. Selecione sua rede Wi-Fi e digite a senha.");
        ESP_LOGI(TAG, "=================================================");
        break;
    }

    default:
        break;
    }
}

static void button_monitor_task(void *pvParameters)
{
    button_event_t evt;
    ESP_LOGI(TAG, "Tarefa de monitoramento de botões iniciada");

    while (1) {
        if (xQueueReceive(s_button_queue, &evt, portMAX_DELAY) == pdTRUE) {
            gpio_num_t pin = (evt.id == BTN_S1) ? GPIO_BTN_S1 : GPIO_BTN_S2;
            led_id_t led = (evt.id == BTN_S1) ? LED_RED : LED_GREEN;
            const char *btn_name = (evt.id == BTN_S1) ? "S1" : "S2";

            // 1. Validação do nível alto com pequeno filtro para descartar ruído eletrostático
            vTaskDelay(pdMS_TO_TICKS(30));
            if (gpio_get_level(pin) == 1) {
                ESP_LOGI(TAG, "Botão %s pressionado (GPIO %d) -> Publicando alternância via MQTT", btn_name, pin);
                // RF4/RF5: Publica o novo estado via MQTT sem alterar o pino localmente.
                mqtt_service_publish_led_toggle(led);

                // 2. Aguarda o usuário soltar fisicamente o botão
                while (gpio_get_level(pin) == 1) {
                    vTaskDelay(pdMS_TO_TICKS(20));
                }

                // 3. Aguarda o repique mecânico da liberação estabilizar
                vTaskDelay(pdMS_TO_TICKS(80));

                // 4. Descarta repiques residuais do mesmo botão que possam ter entrado na fila
                button_event_t dummy;
                while (xQueueReceive(s_button_queue, &dummy, 0) == pdTRUE) {
                    if (dummy.id != evt.id) {
                        // Se o outro botão foi pressionado, preserva o evento
                        xQueueSend(s_button_queue, &dummy, 0);
                        break;
                    }
                }
            }
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Iniciando sistema Botoes-MQTT (ESP-IDF v6.1)...");

    // 1. Inicializar partição NVS
    ESP_ERROR_CHECK(storage_nvs_init());

    // 2. Inicializar TCP/IP e Event Loop global
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 3. Criar fila FreeRTOS para eventos de botão
    s_button_queue = xQueueCreate(10, sizeof(button_event_t));
    if (s_button_queue == NULL) {
        ESP_LOGE(TAG, "Falha ao alocar fila de eventos de botões");
        return;
    }

    // 4. Inicializar controlador de GPIOs (LEDs e Botões)
    ESP_ERROR_CHECK(gpio_controller_init(s_button_queue));

    // 5. Verificar se ambos os botões (S1 e S2) estão pressionados no boot (Reset de Fábrica de Rede)
    if (gpio_controller_are_both_buttons_pressed()) {
        ESP_LOGW(TAG, "=================================================");
        ESP_LOGW(TAG, ">>> RESET DE FÁBRICA DE REDE DETECTADO NO BOOT <<<");
        ESP_LOGW(TAG, "Botões S1 (GPIO %d) e S2 (GPIO %d) pressionados!", GPIO_BTN_S1, GPIO_BTN_S2);
        ESP_LOGW(TAG, "Apagando credenciais Wi-Fi salvas na NVS...");
        storage_nvs_clear_wifi_credentials();
        ESP_LOGW(TAG, "Credenciais apagadas. O Captive Portal será iniciado.");
        ESP_LOGW(TAG, "=================================================");

        // Esvaziar eventos de botão residuais enfileirados durante a inicialização
        xQueueReset(s_button_queue);

        // Feedback visual específico de reset: piscar ambos os LEDs rapidamente 5 vezes
        for (int i = 0; i < 5; i++) {
            gpio_controller_set_led(LED_RED, LED_STATE_ON);
            gpio_controller_set_led(LED_GREEN, LED_STATE_ON);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_controller_set_led(LED_RED, LED_STATE_OFF);
            gpio_controller_set_led(LED_GREEN, LED_STATE_OFF);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    } else {
        // Autoteste de hardware dos LEDs em boot normal
        gpio_controller_self_test();
    }

    // 6. Inicializar serviço MQTT (agora com event loop já ativo)
    ESP_ERROR_CHECK(mqtt_service_init());

    // 7. Iniciar tarefa assíncrona de monitoramento dos botões
    xTaskCreate(button_monitor_task, "btn_task", 3072, NULL, 10, NULL);

    // 8. Inicializar e iniciar gerenciador Wi-Fi (com Captive Portal integrado)
    ESP_ERROR_CHECK(wifi_manager_init(on_wifi_manager_event));
    ESP_ERROR_CHECK(wifi_manager_start());

    ESP_LOGI(TAG, "Sistema inicializado com sucesso. Aguardando eventos de rede e periféricos.");
}
