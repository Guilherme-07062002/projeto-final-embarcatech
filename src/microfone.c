#include "microfone.h"
#include <math.h>      // Caso não esteja incluso em microfone.h
#include <string.h>    // Para uso do memcpy

volatile int audio_index = 0;               // Índice atual no buffer de áudio
uint16_t audio_buffer[MAX_AUDIO_SAMPLES];     // Buffer para armazenar as amostras do microfone
uint16_t adc_buffer[SAMPLES];                 // Buffer de amostras do ADC.

uint dma_channel;                             // Canal DMA
dma_channel_config dma_cfg;                   // Configuração do canal DMA

/**
 * Realiza as leituras do ADC e armazena os valores no buffer.
 */
void sample_mic() {
    // Força o ADC a ler do canal do microfone
    adc_select_input(MIC_CHANNEL);

    adc_fifo_drain();    // Limpa o FIFO do ADC.
    adc_run(false);      // Desliga o ADC para configurar o DMA.

    dma_channel_configure(dma_channel, &dma_cfg,
        adc_buffer,         // Destino: buffer de amostras do ADC
        &(adc_hw->fifo),    // Origem: FIFO do ADC
        SAMPLES,            // Número de amostras a transferir
        true                // Inicia o DMA imediatamente
    );

    // Liga o ADC e aguarda a conclusão da transferência DMA
    adc_run(true);
    dma_channel_wait_for_finish_blocking(dma_channel);
  
    // Finalizada a transferência, desliga o ADC novamente.
    adc_run(false);
}

/**
 * Captura e armazena um bloco de amostras de áudio no buffer global.
 */
void capture_audio_block() {
    // Captura um bloco de SAMPLES amostras (já implementado em sample_mic())
    sample_mic();
    // Verifica se há espaço suficiente no buffer global;
    // permite preencher exatamente até MAX_AUDIO_SAMPLES.
    if (audio_index + SAMPLES <= MAX_AUDIO_SAMPLES) {
        memcpy(&audio_buffer[audio_index], adc_buffer, SAMPLES * sizeof(uint16_t));
        audio_index += SAMPLES;
    } else {
        printf("Buffer de áudio cheio!\n");
    }
}