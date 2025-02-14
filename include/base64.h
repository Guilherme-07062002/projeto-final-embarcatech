#ifndef BASE64_H
#define BASE64_H

#include <stdint.h>
#include <stddef.h>

// Configurações codificação base64
extern const char base64_chars[]; // Caracteres base64
extern const int mod_table[]; // Tabela para calcular os caracteres de padding

void base64_encode(const uint8_t *data, size_t input_length, char *encoded_data, size_t encoded_size);

#endif // BASE64_H