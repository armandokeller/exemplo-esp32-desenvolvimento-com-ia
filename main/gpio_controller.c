#include "gpio_controller.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"

static const char *TAG = "gpio_controller";

static led_state_t s_led_red_state = LED_STATE_OFF;
static led_state_t s_led_green_state = LED_STATE_OFF;
static QueueHandle_t s_button_queue = NULL;

static volatile int64_t s_last_press_time_s1 = 0;
static volatile int64_t s_last_press_time_s2 = 0;

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    int64_t now_ms = esp_timer_get_time() / 1000;
    button_event_t evt;
    bool should_send = false;

    if (gpio_num == GPIO_BTN_S1) {
        if ((now_ms - s_last_press_time_s1) > GPIO_DEBOUNCE_TIME_MS) {
            s_last_press_time_s1 = now_ms;
            evt.id = BTN_S1;
            evt.timestamp_ms = (uint32_t)now_ms;
            should_send = true;
        }
    } else if (gpio_num == GPIO_BTN_S2) {
        if ((now_ms - s_last_press_time_s2) > GPIO_DEBOUNCE_TIME_MS) {
            s_last_press_time_s2 = now_ms;
            evt.id = BTN_S2;
            evt.timestamp_ms = (uint32_t)now_ms;
            should_send = true;
        }
    }

    if (should_send && s_button_queue != NULL) {
        BaseType_t high_task_awoken = pdFALSE;
        xQueueSendFromISR(s_button_queue, &evt, &high_task_awoken);
        portYIELD_FROM_ISR(high_task_awoken);
    }
}

esp_err_t gpio_controller_init(QueueHandle_t button_queue)
{
    s_button_queue = button_queue;

    // 1. Configuração dos LEDs (Saídas em nível baixo)
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << GPIO_LED_RED) | (1ULL << GPIO_LED_GREEN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&led_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao configurar GPIOs dos LEDs: %s", esp_err_to_name(err));
        return err;
    }

    gpio_set_level(GPIO_LED_RED, 0);
    gpio_set_level(GPIO_LED_GREEN, 0);
    s_led_red_state = LED_STATE_OFF;
    s_led_green_state = LED_STATE_OFF;

    // 2. Configuração dos Botões S1 e S2 (Entradas com Pull-Down interno, interrupção na borda de subida)
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << GPIO_BTN_S1) | (1ULL << GPIO_BTN_S2),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    err = gpio_config(&btn_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao configurar GPIOs dos botões: %s", esp_err_to_name(err));
        return err;
    }

    // 3. Instalação do serviço de interrupção GPIO
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Falha ao instalar serviço ISR de GPIO: %s", esp_err_to_name(err));
        return err;
    }

    err = gpio_isr_handler_add(GPIO_BTN_S1, gpio_isr_handler, (void *)GPIO_BTN_S1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao adicionar ISR para S1: %s", esp_err_to_name(err));
        return err;
    }

    err = gpio_isr_handler_add(GPIO_BTN_S2, gpio_isr_handler, (void *)GPIO_BTN_S2);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao adicionar ISR para S2: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "GPIOs configurados (LED Vermelho: GPIO %d, LED Verde: GPIO %d, S1: GPIO %d, S2: GPIO %d)",
             GPIO_LED_RED, GPIO_LED_GREEN, GPIO_BTN_S1, GPIO_BTN_S2);

    return ESP_OK;
}

esp_err_t gpio_controller_set_led(led_id_t led, led_state_t state)
{
    gpio_num_t pin = (led == LED_RED) ? GPIO_LED_RED : GPIO_LED_GREEN;
    uint32_t level = (state == LED_STATE_ON) ? 1 : 0;

    esp_err_t err = gpio_set_level(pin, level);
    if (err == ESP_OK) {
        if (led == LED_RED) {
            s_led_red_state = state;
            ESP_LOGI(TAG, "LED Vermelho (GPIO %d) -> %s", pin, (state == LED_STATE_ON) ? "ON" : "OFF");
        } else {
            s_led_green_state = state;
            ESP_LOGI(TAG, "LED Verde (GPIO %d) -> %s", pin, (state == LED_STATE_ON) ? "ON" : "OFF");
        }
    } else {
        ESP_LOGE(TAG, "Erro ao alterar nível do pino %d: %s", pin, esp_err_to_name(err));
    }
    return err;
}

led_state_t gpio_controller_get_led(led_id_t led)
{
    return (led == LED_RED) ? s_led_red_state : s_led_green_state;
}

const char *gpio_controller_get_led_state_str(led_id_t led)
{
    led_state_t st = gpio_controller_get_led(led);
    return (st == LED_STATE_ON) ? "ON" : "OFF";
}

void gpio_controller_self_test(void)
{
    ESP_LOGI(TAG, "Iniciando autoteste dos LEDs...");
    gpio_set_level(GPIO_LED_RED, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level(GPIO_LED_RED, 0);

    gpio_set_level(GPIO_LED_GREEN, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level(GPIO_LED_GREEN, 0);

    gpio_set_level(GPIO_LED_RED, 1);
    gpio_set_level(GPIO_LED_GREEN, 1);
    vTaskDelay(pdMS_TO_TICKS(300));
    gpio_set_level(GPIO_LED_RED, 0);
    gpio_set_level(GPIO_LED_GREEN, 0);
    ESP_LOGI(TAG, "Autoteste dos LEDs concluído");
}

bool gpio_controller_are_both_buttons_pressed(void)
{
    int high_count = 0;
    for (int i = 0; i < 5; i++) {
        if (gpio_get_level(GPIO_BTN_S1) == 1 && gpio_get_level(GPIO_BTN_S2) == 1) {
            high_count++;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return (high_count >= 4);
}
