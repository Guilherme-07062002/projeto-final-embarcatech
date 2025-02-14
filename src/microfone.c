#include "microfone.h"

volatile int audio_index = 0; // Índice atual no buffer de áudio
uint16_t audio_buffer[MAX_AUDIO_SAMPLES]; // Buffer para armazenar as amostras do microfone
uint16_t adc_buffer[SAMPLES]; // Buffer de amostras do ADC.

uint dma_channel; // Canal DMA
dma_channel_config dma_cfg; // Configuração do canal DMA

/**
 * Realiza as leituras do ADC e armazena os valores no buffer.
 */
void sample_mic() {
    // Force o ADC a ler do canal do microfone
    adc_select_input(MIC_CHANNEL);

    adc_fifo_drain(); // Limpa o FIFO do ADC.
    adc_run(false);   // Desliga o ADC para configurar o DMA.

    dma_channel_configure(dma_channel, &dma_cfg,
        adc_buffer,         // Escreve no buffer.
        &(adc_hw->fifo),    // Lê do ADC.
        SAMPLES,            // Faz SAMPLES amostras.
        true                // Liga o DMA.
    );

    // Liga o ADC e espera acabar a leitura.
    adc_run(true);
    dma_channel_wait_for_finish_blocking(dma_channel);
  
    // Acabou a leitura, desliga o ADC de novo.
    adc_run(false);
}

/**
 * Calcula a potência média das leituras do ADC. (Valor RMS)
 */
float mic_power() {
    float avg = 0.f;
    
    for (uint i = 0; i < SAMPLES; ++i)
      avg += adc_buffer[i] * adc_buffer[i];
    
    avg /= SAMPLES;
    return sqrt(avg);
  }
  
  /**
   * Calcula a intensidade do volume registrado no microfone, de 0 a 4, usando a tensão.
   */
  uint8_t get_intensity(float v) {
        printf("Tensão: %f\n", v);
        
        uint count = 0;
        while ((v -= ADC_STEP/20) > 0.f)
          ++count;
        
        return count;
    }

/**
 * Função para capturar e armazenar um bloco de amostras de áudio no buffer global.
 */
void capture_audio_block() {
    size_t total_bytes = audio_index * sizeof(uint16_t);
    // Captura um bloco de SAMPLES amostras (já implementado em sample_mic())
    sample_mic();
    // Verifica se há espaço suficiente no buffer global
    if (audio_index + SAMPLES < MAX_AUDIO_SAMPLES) {
        // Copia as amostras capturadas para o buffer global
        memcpy(&audio_buffer[audio_index], adc_buffer, SAMPLES * sizeof(uint16_t));
        audio_index += SAMPLES;
    } else {
        printf("Buffer de áudio cheio!\n");
    }
}