#include "base64.h"

// Configurações codificação base64
const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; // Caracteres base64
const int mod_table[] = {0, 2, 1}; // Tabela para calcular os caracteres de padding

/**
 * Função para codificar um buffer em base64.
 */
void base64_encode(const uint8_t *data, size_t input_length, char *encoded_data, size_t encoded_size) {
    // Tamanho do buffer de saída
    size_t output_length = 4 * ((input_length + 2) / 3);
    if (encoded_size < output_length + 1) {
        // O buffer fornecido não é grande o suficiente
        return;
    }
    size_t i, j;

    // Codifica os dados em grupos de 3 octetos
    for (i = 0, j = 0; i < input_length;) {
        uint32_t octet_a = i < input_length ? data[i++] : 0;
        uint32_t octet_b = i < input_length ? data[i++] : 0;
        uint32_t octet_c = i < input_length ? data[i++] : 0;

        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

        encoded_data[j++] = base64_chars[(triple >> 18) & 0x3F];
        encoded_data[j++] = base64_chars[(triple >> 12) & 0x3F];
        encoded_data[j++] = base64_chars[(triple >> 6) & 0x3F];
        encoded_data[j++] = base64_chars[triple & 0x3F];
    }
    // Adiciona os '=' conforme necessário
    for (i = 0; i < mod_table[input_length % 3]; i++) {
        encoded_data[output_length - 1 - i] = '=';
    }
    encoded_data[output_length] = '\0';
}