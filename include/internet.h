#ifndef INTERNET_H
#define INTERNET_H

#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include <string.h>
#include "globals.h"
#include "microfone.h"
#include "buzzer.h"
#include "display.h"
#include "matriz.h"
#include "base64.h"

// Credênciais da rede Wi-Fi
#define WIFI_SSID "" // SSID da rede Wi-Fi
#define WIFI_PASS "" // Senha da rede Wi-Fi

// Variáveis globais para armazenar as perguntas
#define MAX_PERGUNTAS 3 // Número máximo de perguntas a serem recebidas
#define MAX_PERGUNTA_LENGTH 100 // Tamanho máximo de cada pergunta
extern char perguntas[MAX_PERGUNTAS][MAX_PERGUNTA_LENGTH]; // Array para armazenar as perguntas

// Variáveis relacionadas ao cliente HTTP
#define RESPONSE_BUFFER_SIZE 2048 // Tamanho do buffer para armazenar a resposta HTTP
extern char response_buffer[RESPONSE_BUFFER_SIZE];
extern int response_length;
extern ip_addr_t server_ip;

// Funções
void dns_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg);
void send_http_request(void);
err_t tcp_connected_callback(void *arg, struct tcp_pcb *tpcb, err_t err);
err_t http_client_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);

#endif // INTERNET_H