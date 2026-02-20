#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "us_sensor.hpp"

// Tag para os logs do ESP-IDF
static const char *TAG = "ULTRASONIC_EXAMPLE";

// Definição dos pinos GPIO utilizados
#define TRIGER_GPIO GPIO_NUM_21
#define ECHO_GPIO GPIO_NUM_7

// Número inicial de pings por medição
// Nota: O limite máximo definido no componente é MAX_PINGS = 15
#define PINGS_PER_MEASURE 7

extern "C" void app_main(void)
{
    // --- Hardware Configurations for RCWL-1655 sensor
    // Este sensor é à prova d'água e possui algumas características específicas
    UsConfig us_cfg = {
        // Intervalo de 70ms entre pings para evitar interferência de ecos residuais
        .ping_interval_ms = 70,

        // Pulso de trigger de 20us (mais longo que os 10us padrão) para garantir
        // que o transdutor do sensor à prova d'água seja excitado corretamente
        .ping_duration_us = 20,

        // Timeout de 25000us. Considerando a velocidade do som (~0.0343 cm/us),
        // o som percorre ~857cm ida e volta, o que permite medir até ~428cm.
        .timeout_us       = 25000,

        // Filtro DOMINANT_CLUSTER: agrupa as medições próximas e descarta outliers,
        // ideal para ambientes com potenciais ruídos ou reflexões indesejadas
        .filter           = Filter::DOMINANT_CLUSTER,

        // Distância mínima de 25cm, específica para a zona morta do RCWL-1655
        .min_distance_cm  = 25.0f,

        // Distância máxima desejada para esta aplicação
        .max_distance_cm  = 200.0f,

        // Tempo de warmup (aquecimento). Se o sensor estiver sempre ligado,
        // podemos deixar em 0 para ganhar tempo no primeiro boot
        .warmup_time_ms   = 0
    };

    // Instancia o componente UsSensor passando os pinos e a configuração
    UsSensor sensor(TRIGER_GPIO, ECHO_GPIO, us_cfg);

    // Inicializa o sensor (configura GPIOs, etc)
    ESP_LOGI(TAG, "Inicializando sensor ultrasonico...");
    esp_err_t err = sensor.init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar o sensor: %s", esp_err_to_name(err));
        return;
    }

    uint8_t current_pings = PINGS_PER_MEASURE;

    while (1) {
        // Realiza uma medição baseada na média/filtro de múltiplos pings
        Reading reading = sensor.read_distance(current_pings);

        // Verifica se a leitura foi bem sucedida (OK ou WEAK_SIGNAL)
        if (is_success(reading.result)) {
            ESP_LOGI(TAG, "Distancia: %.2f cm | Qualidade: %s (Pings: %u)",
                     reading.cm,
                     (reading.result == UsResult::OK) ? "EXCELENTE" : "FRACA",
                     current_pings);

            // Ajuste dinâmico: se a qualidade for fraca (muitos pings perdidos),
            // aumentamos o número de pings para a próxima rodada, respeitando o limite de 15.
            if (reading.result == UsResult::WEAK_SIGNAL) {
                if (current_pings < 15) current_pings++;
                ESP_LOGW(TAG, "Sinal fraco detectado. Aumentando pings para %u", current_pings);
            } else {
                // Se a qualidade estiver excelente, voltamos ao valor padrão para economizar tempo/energia
                current_pings = PINGS_PER_MEASURE;
            }
        }
        else {
            // Tratamento detalhado de erros
            switch (reading.result) {
                case UsResult::TIMEOUT:
                    ESP_LOGW(TAG, "Erro: Sensor nao respondeu (Timeout). Verifique conexao.");
                    break;
                case UsResult::OUT_OF_RANGE:
                    ESP_LOGW(TAG, "Erro: Objeto fora do alcance (%.1fcm a %.1fcm).",
                             us_cfg.min_distance_cm, us_cfg.max_distance_cm);
                    break;
                case UsResult::HIGH_VARIANCE:
                    ESP_LOGW(TAG, "Erro: Muita variancia nas leituras. Objeto pode estar se movendo.");
                    break;
                case UsResult::INSUFFICIENT_SAMPLES:
                    ESP_LOGW(TAG, "Erro: Amostras insuficientes para um calculo confiavel.");
                    break;
                case UsResult::ECHO_STUCK:
                    ESP_LOGE(TAG, "ERRO CRITICO: Pino ECHO travado em HIGH! Sugerido power-cycle.");
                    break;
                case UsResult::HW_FAULT:
                    ESP_LOGE(TAG, "ERRO CRITICO: Falha de hardware no driver GPIO.");
                    break;
                default:
                    ESP_LOGE(TAG, "Erro desconhecido na leitura.");
                    break;
            }
        }

        // Aguarda 2000ms antes da próxima medição
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
