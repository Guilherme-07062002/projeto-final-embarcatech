#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>
#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

void pwm_init_buzzer(uint pin);
void beep(uint pin, uint duration_ms);

// Configuração do buzzer
#define BUZZER_PIN 21 // Configuração do pino do buzzer
#define BUZZER_FREQUENCY 100 // Frequência do buzzer em Hz

#endif // BUZZER_H