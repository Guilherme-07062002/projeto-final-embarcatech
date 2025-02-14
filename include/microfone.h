#ifndef MICROFONE_H
#define MICROFONE_H

#include <stdint.h>
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// Configuração do microfone
extern volatile int audio_index; // Índice atual no buffer de áudio
#define MAX_AUDIO_SAMPLES 16000  // Exemplo: espaço para 16.000 amostras (ajuste conforme necessário)
extern uint16_t audio_buffer[MAX_AUDIO_SAMPLES]; // Buffer para armazenar as amostras do microfone

#define MIC_CHANNEL 2 // Canal do microfone no ADC.
#define MIC_PIN (26 + MIC_CHANNEL) // Pino do microfone.

// Parâmetros e macros do ADC.
#define ADC_CLOCK_DIV 96.f
#define SAMPLES 200 // Número de amostras que serão feitas do ADC.
#define ADC_ADJUST(x) (x * 3.3f / (1 << 12u) - 1.65f)
#define ADC_MAX 3.3f // Valor máximo do ADC.
#define ADC_STEP (3.3f/5.f) // Intervalos de volume do microfone.
extern uint16_t adc_buffer[SAMPLES]; // Buffer de amostras do ADC.

// Canal e configurações do DMA
extern uint dma_channel; // Canal DMA
extern dma_channel_config dma_cfg; // Configuração do canal DMA

// Protótipos de funções
void sample_mic();
float mic_power();
uint8_t get_intensity(float v);
void capture_audio_block();

#endif // MICROFONE_H