#include "joystick.h"

// Pinos usados para ADC (joystick) e PWM
const int VRY = 27;          // Eixo Y do joystick (ADC)
const int SW = 22; // Pino do botão do joystick

/**
 * Lê os valores dos eixos do joystick no eixo Y.
 */
void joystick_read_axis(uint16_t *vry_value) {
    if (vry_value != NULL) {
        // Se o eixo vertical estiver conectado ao ADC_CHANNEL_0, selecione este canal
        adc_select_input(ADC_CHANNEL_0);
        sleep_us(2);
        *vry_value = adc_read();
    }
}

/**
 * Inicializa o joystick (botão e ADC).
 */
void init_joystick(){
    gpio_init(SW);
    gpio_set_dir(SW, GPIO_IN);
    gpio_pull_up(SW);
}

/**
 * Lê os valores dos eixos do joystick (X e Y)
 * 
 * @param vrx_value Ponteiro para armazenar o valor do eixo X
 * @param vry_value Ponteiro para armazenar o valor do eixo Y
 */
void joystick_read_axis_menu_oled(uint16_t *vrx_value, uint16_t *vry_value) {
    // Seleciona o canal ADC para o eixo X e lê o valor
    adc_select_input(ADC_CHANNEL_0);
    sleep_us(2);
    *vrx_value = adc_read();

    // Seleciona o canal ADC para o eixo Y e lê o valor
    adc_select_input(ADC_CHANNEL_1);
    sleep_us(2);
    *vry_value = adc_read();
}