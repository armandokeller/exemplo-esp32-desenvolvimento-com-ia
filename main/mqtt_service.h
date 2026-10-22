#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "gpio_controller.h"

#define MQTT_BROKER_URI       "mqtt://broker.hivemq.com:1883"
#define MQTT_TOPIC_LED_RED    "/aula/led/vermelho"
#define MQTT_TOPIC_LED_GREEN  "/aula/led/verde"

/**
 * @brief Inicializa a instância do cliente MQTT.
 *
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t mqtt_service_init(void);

/**
 * @brief Inicia a conexão com o broker MQTT.
 *
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t mqtt_service_start(void);

/**
 * @brief Desconecta e para o cliente MQTT.
 *
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t mqtt_service_stop(void);

/**
 * @brief Publica o estado atual do LED correspondente no broker MQTT.
 *
 * @param led LED_RED ou LED_GREEN.
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t mqtt_service_publish_led_state(led_id_t led);

/**
 * @brief Publica a alternância (toggle) do estado do LED no broker MQTT.
 * Se o LED estiver OFF, publica "ON"; se estiver ON, publica "OFF".
 * O estado físico do LED não é alterado localmente (respeitando RF5),
 * sendo atualizado apenas quando a mensagem for ecoada pelo broker MQTT (RF3).
 *
 * @param led LED_RED ou LED_GREEN.
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t mqtt_service_publish_led_toggle(led_id_t led);

/**
 * @brief Retorna se o cliente está atualmente conectado e autenticado no broker MQTT.
 *
 * @return true se conectado, false caso contrário.
 */
bool mqtt_service_is_connected(void);
