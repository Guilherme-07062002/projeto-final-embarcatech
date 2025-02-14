#ifndef __NEOPIXEL_INC
#define __NEOPIXEL_INC

#include <stdint.h> // Para uint8_t, uint16_t, etc.
#include <stdlib.h> // Para calloc
#include <pico/stdlib.h> // Necessário para pio_add_program, etc.
#include <hardware/pio.h> // Necessário para PIO

// Definição de pixel GRB
typedef struct {
  uint8_t G, R, B; // Três valores de 8-bits compõem um pixel.
} npLED_t;

// Funções
void npInit(uint pin, uint amount);
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b);
void npClear();
void npWrite();

#endif