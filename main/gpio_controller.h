#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define GPIO_LED_RED    GPIO_NUM_14
#define GPIO_LED_GREEN  GPIO_NUM_13
#define GPIO_BTN_S1     GPIO_NUM_23
#define GPIO_BTN_S2     GPIO_NUM_22

#define GPIO_DEBOUNCE_TIME_MS 200

typedef enum {
    LED_RED = 0,
    LED_GREEN = 1
} led_id_t;

typedef enum {
    LED_STATE_OFF = 0,
    LED_STATE_ON = 1
} led_state_t;

typedef enum {
    BTN_S1 = 1,
    BTN_S2 = 2
} button_id_t;

typedef struct {
    button_id_t id;
    uint32_t timestamp_ms;
} button_event_t;

/**
 * @brief Inicializa os pinos de LEDs e botões com debounce.
 *
 * @param button_queue Fila do FreeRTOS para envio de eventos de botão pressionado.
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t gpio_controller_init(QueueHandle_t button_queue);

/**
 * @brief Altera o estado de um LED especificado.
 *
 * @param led LED_RED ou LED_GREEN.
 * @param state LED_STATE_ON ou LED_STATE_OFF.
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t gpio_controller_set_led(led_id_t led, led_state_t state);

/**
 * @brief Obtém o estado atual de um LED especificado.
 *
 * @param led LED_RED ou LED_GREEN.
 * @return LED_STATE_ON ou LED_STATE_OFF.
 */
led_state_t gpio_controller_get_led(led_id_t led);

/**
 * @brief Retorna o estado do LED em formato de texto para MQTT ("ON" ou "OFF").
 *
 * @param led LED_RED ou LED_GREEN.
 * @return "ON" ou "OFF".
 */
const char *gpio_controller_get_led_state_str(led_id_t led);

/**
 * @brief Executa um ciclo rápido de autoteste visual piscando os LEDs.
 */
void gpio_controller_self_test(void);

/**
 * @brief Verifica se ambos os botões S1 e S2 estão pressionados fisicamente.
 * Utilizado na inicialização (boot) para detecção de reset de fábrica de rede.
 *
 * @return true se ambos os botões estiverem em nível alto (pressionados).
 */
bool gpio_controller_are_both_buttons_pressed(void);
