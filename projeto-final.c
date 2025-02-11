#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
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

// Pino do botão do joystick
const int SW = 22;

// Pinos usados para ADC (joystick) e PWM
const int VRX = 26;          // Eixo X do joystick (ADC)
const int VRY = 27;          // Eixo Y do joystick (ADC)
#define ADC_CHANNEL_0 0      // Canal ADC para o eixo X
#define ADC_CHANNEL_1 1      // Canal ADC para o eixo Y

#define RESPONSE_BUFFER_SIZE 2048

static char response_buffer[RESPONSE_BUFFER_SIZE];
static int response_length = 0;
static ip_addr_t server_ip;

ssd1306_t disp; // Display OLED

// Ponteiro para o texto a ser exibido com rolagem (corpo da resposta HTTP)
static char *display_message = NULL;

// Variáveis globais para acumular a posição do scroll
int scroll_x = 0, scroll_y = 0;

// Parâmetros para definir os limiares e a velocidade do scroll
#define JOY_THRESHOLD_UP    2500   // se ADC Y maior que este valor, rola para cima
#define JOY_THRESHOLD_DOWN  1600   // se ADC Y menor que este valor, rola para baixo
#define SCROLL_SPEED        2      // pixels por iteração

/**
 * Limpa o display OLED.
 */
void clear_display() {
    ssd1306_clear(&disp);
    ssd1306_show(&disp);
}

/**
 * Função para escrever texto no display com rolagem sem modificar a string original.
 *
 * @param msg       Mensagem a ser exibida.
 * @param offset_x  Deslocamento horizontal (em pixels).
 * @param offset_y  Deslocamento vertical (em pixels).
 * @param scale     Escala do texto.
 */
void print_texto_scroll(const char *msg, int offset_x, int offset_y, uint scale) {
    // Cria uma cópia local da mensagem para não modificar o buffer original
    char temp[RESPONSE_BUFFER_SIZE];
    strncpy(temp, msg, RESPONSE_BUFFER_SIZE);
    temp[RESPONSE_BUFFER_SIZE - 1] = '\0';

    clear_display();

    // Divide a mensagem em linhas (usando strtok em uma cópia local)
    char *line = strtok(temp, "\n");
    int y = offset_y;
    while (line != NULL) {
        ssd1306_draw_string(&disp, offset_x, y, scale, line);
        y += 8 * scale; // Avança para a próxima linha (ajuste conforme a altura da fonte)
        line = strtok(NULL, "\n");
    }
    ssd1306_show(&disp);
}

/**
 * Callback para processar a resposta HTTP.
 */
static err_t http_client_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    printf("Callback HTTP\n");
    if (p == NULL) {
        // Quando p for NULL, o servidor fechou a conexão.
        response_buffer[response_length] = '\0'; // Termina a string
        printf("Resposta completa do servidor:\n%s\n", response_buffer);

        // Procura o início do corpo da resposta (após o header)
        char *body = strstr(response_buffer, "\r\n\r\n");
        if (body) {
            body += 4; // Pula as quebras de linha
            printf("Corpo da resposta:\n%s\n", body);
            // Armazena o corpo da resposta para que o loop principal o exiba com rolagem
            display_message = body;
        } else {
            printf("Corpo da resposta não encontrado\n");
        }
        tcp_close(tpcb);
        return ERR_OK;
    }

    // Copia os dados de todos os pbufs para o buffer global
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

/**
 * Callback chamado quando a conexão TCP é estabelecida.
 */
static err_t tcp_connected_callback(void *arg, struct tcp_pcb *tpcb, err_t err) {
    if (err != ERR_OK) {
        printf("Erro ao conectar ao servidor: %d\n", err);
        return err;
    }
    if (tcp_write(tpcb, HTTP_REQUEST, strlen(HTTP_REQUEST), TCP_WRITE_FLAG_COPY) != ERR_OK) {
        printf("Erro ao enviar a requisição HTTP\n");
        return ERR_VAL;
    }
    tcp_output(tpcb); // Garante envio imediato
    printf("Requisição HTTP enviada\n");
    return ERR_OK;
}

/**
 * Callback do DNS (usado quando o DNS não está em cache).
 */
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

/**
 * Função para enviar a requisição HTTP.
 */
static void send_http_request(void) {
    response_length = 0;
    memset(response_buffer, 0, RESPONSE_BUFFER_SIZE);
    err_t err = dns_gethostbyname("jsonplaceholder.typicode.com", &server_ip, dns_callback, NULL);
    if (err == ERR_OK) {
        // Se o DNS já está em cache, conecta imediatamente.
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
 * Inicializa o display OLED.
 */
void init_display() {
    i2c_init(I2C_PORT, 400 * 1000); // 400 KHz
    gpio_set_function(PINO_SCL, GPIO_FUNC_I2C);
    gpio_set_function(PINO_SDA, GPIO_FUNC_I2C);
    gpio_pull_up(PINO_SCL);
    gpio_pull_up(PINO_SDA);
    disp.external_vcc = false;
    ssd1306_init(&disp, 128, 64, 0x3C, I2C_PORT);
    clear_display();
}

/**
 * Lê os valores dos eixos do joystick (X e Y).
 */
void joystick_read_axis(uint16_t *vrx_value, uint16_t *vry_value) {
    adc_select_input(ADC_CHANNEL_0);
    sleep_us(2);
    *vrx_value = adc_read();
    adc_select_input(ADC_CHANNEL_1);
    sleep_us(2);
    *vry_value = adc_read();
}

/**
 * Inicializa o joystick (botão e ADC).
 */
void init_joystick(){
    gpio_init(SW);
    gpio_set_dir(SW, GPIO_IN);
    gpio_pull_up(SW);
}

/**
 * Main.
 */
int main() {
    stdio_init_all();  // Inicializa a saída padrão

    // Inicializa ADC para leitura do joystick
    adc_init();

    // Inicializa o display OLED e o joystick
    init_display();
    init_joystick();

    // Exibe uma mensagem inicial
    print_texto_scroll("Projeto Final", 0, 0, 1);

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

    // Variáveis para leitura do joystick e cálculo dos offsets
    uint16_t vrx_value = 0, vry_value = 0;
    int offset_x = 0, offset_y = 0;

    // Loop principal: mantém o Wi-Fi ativo e atualiza a rolagem se o corpo da resposta estiver disponível
    while (true) {
        cyw43_arch_poll();
        sleep_ms(100);

        if (display_message != NULL) {
            uint16_t vrx_value = 0, vry_value = 0;
            joystick_read_axis(&vrx_value, &vry_value);
            
            // Atualiza a rolagem vertical
            if (vry_value > JOY_THRESHOLD_UP) { // joystick inclinado para cima
                scroll_y -= SCROLL_SPEED;
            } else if (vry_value < JOY_THRESHOLD_DOWN) { // joystick inclinado para baixo
                scroll_y += SCROLL_SPEED;
            }
            // Atualiza a rolagem horizontal (se necessário)
            if (vrx_value > JOY_THRESHOLD_UP) {
                scroll_x -= SCROLL_SPEED;
            } else if (vrx_value < JOY_THRESHOLD_DOWN) {
                scroll_x += SCROLL_SPEED;
            }
            
            // (Opcional) Você pode aplicar limites ao scroll_y e scroll_x,
            // se souber a altura total do texto e a largura do display.
            // Por exemplo, se o texto tiver N linhas, total_text_height = N * (8*scale)
            // e o scroll_y não deve ser menor que -(total_text_height - display_height)
            // ou maior que 0 (ou vice-versa, conforme a orientação desejada).

            print_texto_scroll(display_message, scroll_x, scroll_y, 1);
            printf("Joystick X: %d, Y: %d   scroll_x: %d, scroll_y: %d\n", vrx_value, vry_value, scroll_x, scroll_y);
        }
    }

    cyw43_arch_deinit();
    return 0;
}
