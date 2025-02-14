#ifndef MATRIZ_H
#define MATRIZ_H

#include "neopixel.h"
#include "buzzer.h"

// Pino e número de LEDs da matriz de LEDs.
#define LED_PIN 7 // Pino de dados da matriz de LEDs
#define LED_COUNT 25 // Número de LEDs na matriz

void draw_smile();
void draw_notification();

#endif // MATRIZ_H