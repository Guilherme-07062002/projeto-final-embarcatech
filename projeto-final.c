#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "ssd1306.h"
#include "neopixel.c"

#define WIFI_SSID "CAVALO_DE_TROIA"
#define WIFI_PASS "81001693"
#define HTTP_REQUEST "GET /todos/1 HTTP/1.1\r\nHost: jsonplaceholder.typicode.com\r\nConnection: close\r\n\r\n"

#define BUTTON_A 5    // GPIO conectado ao Botão A

// Pinos e módulo I2C
#define I2C_PORT i2c1
#define PINO_SCL 14
#define PINO_SDA 15

// Pino do botão do joystick
const int SW = 22;

// Pinos usados para ADC (joystick) e PWM
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
int scroll_y = 0;

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
 * @param offset_y  Deslocamento vertical (em pixels).
 * @param scale     Escala do texto.
 */
void print_texto_scroll(const char *msg, int offset_x, int offset_y, uint scale) {
    // Cria uma cópia local da mensagem para não modificar o buffer original
    char temp[RESPONSE_BUFFER_SIZE];
    strncpy(temp, msg, RESPONSE_BUFFER_SIZE);
    temp[RESPONSE_BUFFER_SIZE - 1] = '\0';

    clear_display();

    // Variáveis para controle de posição e largura do display
    int display_width = 128; // Largura do display em pixels
    int char_width = 6 * scale; // Largura de um caractere em pixels (ajuste conforme necessário)
    int max_chars_per_line = display_width / char_width; // Número máximo de caracteres por linha

    // Divide a mensagem em palavras
    char *word = strtok(temp, " ");
    int x = 0;
    int y = offset_y;
    while (word != NULL) {
        int word_length = strlen(word);
        if (x + word_length * char_width > display_width) {
            // Se a palavra não cabe na linha atual, move para a próxima linha
            x = 0;
            y += 8 * scale;
        }
        // Desenha a palavra no display
        ssd1306_draw_string(&disp, x, y, scale, word);
        x += (word_length + 1) * char_width; // Avança a posição x (inclui espaço)
        word = strtok(NULL, " ");
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
 * Lê os valores dos eixos do joystick no eixo Y.
 */
void joystick_read_axis(uint16_t *vry_value) {
    if (vry_value != NULL) {
        // Se o eixo vertical estiver conectado ao ADC_CHANNEL_0, selecione este canal
        adc_select_input(ADC_CHANNEL_0);
        sleep_us(2);
        *vry_value = adc_read();
    }
}

/**
 * Inicializa o joystick (botão e ADC).
 */
void init_joystick(){
    gpio_init(SW);
    gpio_set_dir(SW, GPIO_IN);
    gpio_pull_up(SW);
}


// Pino e canal do microfone no ADC.
#define MIC_CHANNEL 2
#define MIC_PIN (26 + MIC_CHANNEL)

// Parâmetros e macros do ADC.
#define ADC_CLOCK_DIV 96.f
#define SAMPLES 200 // Número de amostras que serão feitas do ADC.
#define ADC_ADJUST(x) (x * 3.3f / (1 << 12u) - 1.65f) // Ajuste do valor do ADC para Volts.
#define ADC_MAX 3.3f
#define ADC_STEP (3.3f/5.f) // Intervalos de volume do microfone.

// Pino e número de LEDs da matriz de LEDs.
#define LED_PIN 7
#define LED_COUNT 25

#define abs(x) ((x < 0) ? (-x) : (x))

// Canal e configurações do DMA
uint dma_channel;
dma_channel_config dma_cfg;

// Buffer de amostras do ADC.
uint16_t adc_buffer[SAMPLES];

void sample_mic();
float mic_power();
uint8_t get_intensity(float v);

/**
 * Realiza as leituras do ADC e armazena os valores no buffer.
 */
void sample_mic() {
  adc_fifo_drain(); // Limpa o FIFO do ADC.
  adc_run(false); // Desliga o ADC (se estiver ligado) para configurar o DMA.

  dma_channel_configure(dma_channel, &dma_cfg,
    adc_buffer, // Escreve no buffer.
    &(adc_hw->fifo), // Lê do ADC.
    SAMPLES, // Faz SAMPLES amostras.
    true // Liga o DMA.
  );

  // Liga o ADC e espera acabar a leitura.
  adc_run(true);
  dma_channel_wait_for_finish_blocking(dma_channel);
  
  // Acabou a leitura, desliga o ADC de novo.
  adc_run(false);
}

/**
 * Calcula a potência média das leituras do ADC. (Valor RMS)
 */
float mic_power() {
  float avg = 0.f;

  for (uint i = 0; i < SAMPLES; ++i)
    avg += adc_buffer[i] * adc_buffer[i];
  
  avg /= SAMPLES;
  return sqrt(avg);
}

/**
 * Calcula a intensidade do volume registrado no microfone, de 0 a 4, usando a tensão.
 */
uint8_t get_intensity(float v) {
  uint count = 0;

  while ((v -= ADC_STEP/20) > 0.f)
    ++count;
  
  return count;
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

    // Configuração do GPIO do Botão A como entrada com pull-up interno
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);

    // Exibe uma mensagem inicial
    print_texto_scroll("Projeto Final", 0, 0, 1);

    sleep_ms(10000);
    printf("Iniciando requisição HTTP\n");

    // Inicializa o Wi-Fi (comentado para foco na coleta de áudio e exibição de texto)
    // if (cyw43_arch_init()) {
    //     printf("Erro ao inicializar o Wi-Fi\n");
    //     return 1;
    // }
    // cyw43_arch_enable_sta_mode();
    // printf("Conectando ao Wi-Fi...\n");
    // if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
    //     printf("Falha ao conectar ao Wi-Fi\n");
    //     return 1;
    // } else {
    //     printf("Conectado.\n");
    //     uint8_t *ip_address = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);
    //     printf("Endereço IP %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
    // }
    // printf("Wi-Fi conectado!\n");

    // Envia a requisição HTTP (comentado para foco na coleta de áudio e exibição de texto)
    // send_http_request();

    // Preparação da matriz de LEDs.
    printf("Preparando NeoPixel...");
    npInit(LED_PIN, LED_COUNT);

    // Preparação do ADC.
    printf("Preparando ADC...\n");
    adc_gpio_init(MIC_PIN);
    adc_select_input(MIC_CHANNEL);
    adc_fifo_setup(
        true, // Habilitar FIFO
        true, // Habilitar request de dados do DMA
        1, // Threshold para ativar request DMA é 1 leitura do ADC
        false, // Não usar bit de erro
        false // Não fazer downscale das amostras para 8-bits, manter 12-bits.
    );
    adc_set_clkdiv(ADC_CLOCK_DIV);
    printf("ADC Configurado!\n\n");

    printf("Preparando DMA...");
    // Tomando posse de canal do DMA.
    dma_channel = dma_claim_unused_channel(true);
    // Configurações do DMA.
    dma_cfg = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_16); // Tamanho da transferência é 16-bits (usamos uint16_t para armazenar valores do ADC)
    channel_config_set_read_increment(&dma_cfg, false); // Desabilita incremento do ponteiro de leitura (lemos de um único registrador)
    channel_config_set_write_increment(&dma_cfg, true); // Habilita incremento do ponteiro de escrita (escrevemos em um array/buffer)
    channel_config_set_dreq(&dma_cfg, DREQ_ADC); // Usamos a requisição de dados do ADC

    // Amostragem de teste.
    printf("Amostragem de teste...\n");
    sample_mic();
    printf("Configuracoes completas!\n");

    printf("\n----\nIniciando loop...\n----\n");

    // Variáveis para leitura do joystick e cálculo dos offsets
    uint16_t vry_value = 0;
    int offset_y = 0;
    int prev_scroll_y = scroll_y; // Armazena o valor anterior de scroll_y

    display_message = "O ceu e azul devido a forma como a luz do sol interage com a atmosfera da Terra. A luz do sol parece branca, mas na verdade e composta por varias cores, cada uma com um comprimento de onda diferente. Quando a luz solar entra na atmosfera, ela colide com moleculas de ar e outras particulas. A luz azul, que tem um comprimento de onda mais curto, e espalhada em todas as direcoes por essas moleculas e particulas. Esse espalhamento e chamado de espalhamento de Rayleigh. Como a luz azul e espalhada em todas as direcoes, ela chega aos nossos olhos de todos os lados, fazendo com que o ceu pareca azul. Durante o nascer e o por do sol, a luz solar tem que passar por uma porcao maior da atmosfera, o que faz com que mais luz azul seja espalhada para fora do nosso campo de visao, deixando as cores vermelha e laranja mais predominantes.";

    // Se houver uma mensagem para exibir, exibe-a
    if (display_message != NULL) {
        print_texto_scroll(display_message, 0, 0, 1);
    }

    // Loop principal: coleta áudio e atualiza a rolagem se o corpo da resposta estiver disponível
    while (true) {
        sleep_ms(50);

        // Verifica se o botão está pressionado
        while (gpio_get(BUTTON_A) == 0) {
            // Coleta de áudio
            sample_mic();
            float avg = mic_power();
            avg = 2.f * abs(ADC_ADJUST(avg)); // Ajusta para intervalo de 0 a 3.3V. (apenas magnitude, sem sinal)
            uint intensity = get_intensity(avg); // Calcula intensidade a ser mostrada na matriz de LEDs

            // Limpa a matriz de LEDs
            npClear();

            // A depender da intensidade do som, acende LEDs específicos
            switch (intensity) {
                case 0: break; // Se o som for muito baixo, não acende nada.
                case 1:
                    npSetLED(12, 0, 0, 80); // Acende apenas o centro.
                    break;
                case 2:
                    npSetLED(12, 0, 0, 120); // Acende o centro.
                    // Primeiro anel.
                    npSetLED(7, 0, 0, 80);
                    npSetLED(11, 0, 0, 80);
                    npSetLED(13, 0, 0, 80);
                    npSetLED(17, 0, 0, 80);
                    break;
                case 3:
                    // Centro.
                    npSetLED(12, 60, 60, 0);
                    // Primeiro anel.
                    npSetLED(7, 0, 0, 120);
                    npSetLED(11, 0, 0, 120);
                    npSetLED(13, 0, 0, 120);
                    npSetLED(17, 0, 0, 120);
                    // Segundo anel.
                    npSetLED(2, 0, 0, 80);
                    npSetLED(6, 0, 0, 80);
                    npSetLED(8, 0, 0, 80);
                    npSetLED(10, 0, 0, 80);
                    npSetLED(14, 0, 0, 80);
                    npSetLED(16, 0, 0, 80);
                    npSetLED(18, 0, 0, 80);
                    npSetLED(22, 0, 0, 80);
                    break;
                case 4:
                    // Centro.
                    npSetLED(12, 80, 0, 0);
                    // Primeiro anel.
                    npSetLED(7, 60, 60, 0);
                    npSetLED(11, 60, 60, 0);
                    npSetLED(13, 60, 60, 0);
                    npSetLED(17, 60, 60, 0);
                    // Segundo anel.
                    npSetLED(2, 0, 0, 120);
                    npSetLED(6, 0, 0, 120);
                    npSetLED(8, 0, 0, 120);
                    npSetLED(10, 0, 0, 120);
                    npSetLED(14, 0, 0, 120);
                    npSetLED(16, 0, 0, 120);
                    npSetLED(18, 0, 0, 120);
                    npSetLED(22, 0, 0, 120);
                    // Terceiro anel.
                    npSetLED(1, 0, 0, 80);
                    npSetLED(3, 0, 0, 80);
                    npSetLED(5, 0, 0, 80);
                    npSetLED(9, 0, 0, 80);
                    npSetLED(15, 0, 0, 80);
                    npSetLED(19, 0, 0, 80);
                    npSetLED(21, 0, 0, 80);
                    npSetLED(23, 0, 0, 80);
                    break;
            }
            // Atualiza a matriz.
            npWrite();

            // Envia a intensidade e a média das leituras do ADC por serial.
            printf("%2d %8.4f\r", intensity, avg);

            // Pequena pausa para evitar leitura contínua muito rápida
            sleep_ms(50);
        }

        // Atualiza a rolagem do texto
        if (display_message != NULL) {
            joystick_read_axis(&vry_value); // Lê apenas o eixo Y

            // Atualiza a rolagem vertical
            if (vry_value > JOY_THRESHOLD_UP) { // joystick inclinado para cima
                scroll_y += SCROLL_SPEED;
            } else if (vry_value < JOY_THRESHOLD_DOWN) { // joystick inclinado para baixo
                scroll_y -= SCROLL_SPEED;
            }

            // Desenha o display apenas se scroll_y mudou
            if (scroll_y != prev_scroll_y) {
                print_texto_scroll(display_message, 0, scroll_y, 1);
                prev_scroll_y = scroll_y; // Atualiza o valor anterior de scroll_y
            }

            printf("Joystick Y: %d   scroll_y: %d\n", vry_value, scroll_y);
        }
    }

    cyw43_arch_deinit();
    return 0;
}