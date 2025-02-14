#include "matriz.h"

/**
 * Função para desenhar sorriso na matriz de leds
 */
void draw_smile() {
    npClear();

    // Olhos
    npSetLED(16, 0, 100, 0);
    npSetLED(18, 0, 100, 0);

    // Boca
    npSetLED(14, 0, 100, 0);
    npSetLED(10, 0, 100, 0);
    npSetLED(6, 0, 100, 0);
    npSetLED(7, 0, 100, 0);
    npSetLED(8, 0, 100, 0);

    npWrite();
}

/**
 * Desenha notificação quando mensagem é recebida.
 */
void draw_notification() {
    npClear();
            
    npSetLED(2, 0, 100, 0);  
    npSetLED(12, 0, 100, 0);  
    npSetLED(17, 0, 100, 0);  
    npSetLED(22, 0, 100, 0);  

    // Emitir notificação
    beep(BUZZER_PIN, 100); // Bipe de 500ms

    npWrite();
    sleep_ms(500);
}