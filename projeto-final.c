#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include <string.h>
#include <stdio.h>

#define WIFI_SSID "CAVALO_DE_TROIA"
#define WIFI_PASS "81001693"
#define HTTP_REQUEST "GET /todos/1 HTTP/1.1\r\nHost: jsonplaceholder.typicode.com\r\nConnection: close\r\n\r\n"

// Pinos e módulo I2C
#define I2C_PORT i2c1
#define PINO_SCL 14
#define PINO_SDA 15

#define RESPONSE_BUFFER_SIZE 2048

static char response_buffer[RESPONSE_BUFFER_SIZE];
static int response_length = 0;
static ip_addr_t server_ip;

ssd1306_t disp; // Display OLED

// Função de callback para processar a resposta HTTP
static err_t http_client_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    printf("Callback HTTP\n");
    if (p == NULL) {
        // Quando p for NULL, o servidor fechou a conexão.
        response_buffer[response_length] = '\0'; // Termina a string
        printf("Resposta completa do servidor:\n%s\n", response_buffer);

        // Procura o início do corpo da resposta
        char *body = strstr(response_buffer, "\r\n\r\n");
        if (body) {
            body += 4; // Pula as duas quebras de linha
            printf("Corpo da resposta:\n%s\n", body);

            // Exibe o corpo da resposta no display
            print_texto(body, 0, 0, 1);
        } else {
            printf("Corpo da resposta não encontrado\n");
        }

        tcp_close(tpcb);
        return ERR_OK;
    }

    // Itera sobre a cadeia de pbufs para copiar todos os dados
    struct pbuf *q;
    for (q = p; q != NULL; q = q->next) {
        if (response_length + q->len < RESPONSE_BUFFER_SIZE) {
            memcpy(response_buffer + response_length, q->payload, q->len);
            response_length += q->len;
        } else {
            printf("Buffer de resposta cheio\n");
            break;
        }
    }

    pbuf_free(p);
    return ERR_OK;
}

// Callback chamado quando a conexão TCP é estabelecida
static err_t tcp_connected_callback(void *arg, struct tcp_pcb *tpcb, err_t err) {
    if (err != ERR_OK) {
        printf("Erro ao conectar ao servidor: %d\n", err);
        return err;
    }

    if (tcp_write(tpcb, HTTP_REQUEST, strlen(HTTP_REQUEST), TCP_WRITE_FLAG_COPY) != ERR_OK) {
        printf("Erro ao enviar a requisição HTTP\n");
        return ERR_VAL;
    }

    // Garante que os dados sejam enviados imediatamente
    tcp_output(tpcb);
    printf("Requisição HTTP enviada\n");
    return ERR_OK;
}

// Callback do DNS (usado somente quando o DNS não está em cache)
static void dns_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    if (ipaddr == NULL) {
        printf("Erro ao resolver o endereço do servidor\n");
        return;
    }
    printf("DNS resolvido: %s\n", ipaddr_ntoa(ipaddr));

    // Cria o PCB e conecta ao servidor
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        printf("Erro ao criar PCB\n");
        return;
    }
    tcp_recv(pcb, http_client_callback);
    if (tcp_connect(pcb, ipaddr, 80, tcp_connected_callback) != ERR_OK) {
        printf("Erro ao conectar ao servidor\n");
        return;
    }
}

// Função para enviar a requisição HTTP
static void send_http_request(void) {
    // Reseta o buffer de resposta
    response_length = 0;
    memset(response_buffer, 0, RESPONSE_BUFFER_SIZE);

    err_t err = dns_gethostbyname("jsonplaceholder.typicode.com", &server_ip, dns_callback, NULL);
    if (err == ERR_OK) {
        // Se o DNS já está em cache, o endereço já foi resolvido.
        printf("DNS resolvido imediatamente: %s\n", ipaddr_ntoa(&server_ip));
        struct tcp_pcb *pcb = tcp_new();
        if (!pcb) {
            printf("Erro ao criar PCB\n");
            return;
        }
        tcp_recv(pcb, http_client_callback);
        if (tcp_connect(pcb, &server_ip, 80, tcp_connected_callback) != ERR_OK) {
            printf("Erro ao conectar ao servidor\n");
            return;
        }
    } else if (err == ERR_INPROGRESS) {
        printf("Resolução do DNS em andamento...\n");
    } else {
        printf("Erro ao iniciar a resolução do DNS\n");
    }
}

/**
 * Limpa o display OLED
 */
void clear_display() {
    ssd1306_clear(&disp);
    ssd1306_show(&disp);
}

/**
 * Inicializa o display OLED
 */
void init_display() {
    // Inicializa o I2C e o display OLED
    i2c_init(I2C_PORT, 400 * 1000); // 400 KHz
    gpio_set_function(PINO_SCL, GPIO_FUNC_I2C);
    gpio_set_function(PINO_SDA, GPIO_FUNC_I2C);
    gpio_pull_up(PINO_SCL);
    gpio_pull_up(PINO_SDA);
    disp.external_vcc = false;
    ssd1306_init(&disp, 128, 64, 0x3C, I2C_PORT);

    // Limpa o display
    clear_display();
}

/**
 * Função para escrever texto no display
 * 
 * @param msg   Mensagem a ser exibida
 * @param pos_x Posição X no display
 * @param pos_y Posição Y no display
 * @param scale Escala do texto
 */
void print_texto(char *msg, uint pos_x, uint pos_y, uint scale) {
    // Limpa o display
    clear_display();

    ssd1306_draw_string(&disp, pos_x, pos_y, scale, msg);
    ssd1306_show(&disp);
}

int main() {
    stdio_init_all();  // Inicializa a saída padrão

    // Inicializa o display OLED
    init_display();

    // Exibe uma mensagem no display
    print_texto("Projeto Final", 0, 0, 1);

    sleep_ms(10000);
    printf("Iniciando requisição HTTP\n");

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
        printf("Conectado.\n");
        uint8_t *ip_address = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);
        printf("Endereço IP %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
    }

    printf("Wi-Fi conectado!\n");

    // Envia a requisição HTTP
    send_http_request();

    // Loop principal para manter o Wi-Fi ativo
    while (true) {
        cyw43_arch_poll();
        sleep_ms(100);
    }

    cyw43_arch_deinit();
    return 0;
}