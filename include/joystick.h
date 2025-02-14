#ifndef JOYSTICK_H
#define JOYSTICK_H

#include "pico/stdlib.h"
#include "hardware/adc.h"

// Pinos usados para ADC (joystick) e PWM
extern const int VRY;          // Eixo Y do joystick (ADC)
extern const int SW; // Pino do botão do joystick
#define ADC_CHANNEL_0 0      // Canal ADC para o eixo X
#define ADC_CHANNEL_1 1      // Canal ADC para o eixo Y

// Parâmetros para definir os limiares e a velocidade do scroll
#define JOY_THRESHOLD_UP    2500   // se ADC Y maior que este valor, rola para cima
#define JOY_THRESHOLD_DOWN  1600   // se ADC Y menor que este valor, rola para baixo
#define SCROLL_SPEED        2      // pixels por iteração

void joystick_read_axis(uint16_t *vry_value);
void init_joystick();
void joystick_read_axis_menu_oled(uint16_t *vrx_value, uint16_t *vry_value);

#endif // JOYSTICK_H