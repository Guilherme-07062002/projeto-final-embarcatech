#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"
#include <string.h>
#include <stdio.h>

#define LED_PIN 12
#define WIFI_SSID "CAVALO_DE_TROIA"
#define WIFI_PASS "81001693"
#define HTTP_REQUEST "GET /todos/1 HTTP/1.1\r\nHost: jsonplaceholder.typicode.com\r\n\r\n"

// Função de callback para processar a resposta HTTP
static err_t http_client_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    printf("Callback HTTP\n");
    if (p == NULL) {
        // Servidor fechou a conexão
        tcp_close(tpcb);
        return ERR_OK;
    }

    // Processa a resposta HTTP
    char *response = (char *)p->payload;
    printf("Resposta do servidor:\n%s\n", response);

    // Libera o buffer recebido
    pbuf_free(p);

    return ERR_OK;
}

// Função de callback para quando a conexão TCP é estabelecida
static err_t tcp_connected_callback(void *arg, struct tcp_pcb *tpcb, err_t err) {
    if (err != ERR_OK) {
        printf("Erro ao conectar ao servidor\n");
        return err;
    }

    if (tcp_write(tpcb, HTTP_REQUEST, strlen(HTTP_REQUEST), TCP_WRITE_FLAG_COPY) != ERR_OK) {
        printf("Erro ao enviar a requisição HTTP\n");
        return ERR_VAL;
    }

    printf("Requisição HTTP enviada\n");
    return ERR_OK;
}

// Função de callback para quando o endereço IP é resolvido
static void dns_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    if (ipaddr == NULL) {
        printf("Erro ao resolver o endereço do servidor\n");
        return;
    }

    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        printf("Erro ao criar PCB\n");
        return;
    }

    tcp_recv(pcb, http_client_callback);  // Associa o callback HTTP

    if (tcp_connect(pcb, ipaddr, 80, tcp_connected_callback) != ERR_OK) {
        printf("Erro ao conectar ao servidor\n");
        return;
    }
}

// Função para enviar a requisição HTTP
static ip_addr_t server_ip;

static void send_http_request(void) {
    err_t err = dns_gethostbyname("jsonplaceholder.typicode.com", &server_ip, dns_callback, NULL);
    if (err == ERR_OK) {
        // O endereço IP foi resolvido imediatamente
        dns_callback("jsonplaceholder.typicode.com", &server_ip, NULL);
    } else if (err == ERR_INPROGRESS) {
        // A resolução do DNS está em andamento
        printf("Resolução do DNS em andamento...\n");
    } else {
        printf("Erro ao iniciar a resolução do DNS\n");
    }
}

int main() {
    stdio_init_all();  // Inicializa a saída padrão
    sleep_ms(10000);
    printf("Iniciando servidor HTTP\n");

    // Inicializa o Wi-Fi
    if (cyw43_arch_init()) {
        printf("Erro ao inicializar o Wi-Fi\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi...\n");

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        return 1;
    } else {
        printf("Connected.\n");
        // Read the ip address in a human readable way
        uint8_t *ip_address = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);
        printf("Endereço IP %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
    }

    printf("Wi-Fi conectado!\n");

    // Envia a requisição HTTP
    send_http_request();

    // Loop principal
    while (true) {
        cyw43_arch_poll();  // Necessário para manter o Wi-Fi ativo
        sleep_ms(100);
    }

    cyw43_arch_deinit();  // Desliga o Wi-Fi (não será chamado, pois o loop é infinito)
    return 0;
}