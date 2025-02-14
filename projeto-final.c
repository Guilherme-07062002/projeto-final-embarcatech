#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stddef.h>
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

// Variavel que indica que a mensagem foi recebida
bool mensagem_recebida = false;


#define MAX_PERGUNTAS 3 // Número máximo de perguntas a serem recebidas
#define MAX_PERGUNTA_LENGTH 100 // Tamanho máximo de cada pergunta

// Array para armazenar as perguntas
char perguntas[MAX_PERGUNTAS][MAX_PERGUNTA_LENGTH];

#define BUTTON_A 5    // GPIO conectado ao Botão A
#define BUTTON_B 6    // GPIO conectado ao Botão B

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

// Variavel que irá indicar quando o botão foi pressionado para enviar a requisição HTTP
bool botao_foi_pressionado = false;

// Variavel que irá indicar se botão B foi pressionado para quando enviar requisição HTTP enviar pergunta escolhida
bool botao_b_foi_pressionado = false;

// Variável para indicar qual pergunta foi selecionada
int pergunta_selecionada = 0;

// Variável para indicar se a mensagem está sendo processada
bool esta_processando = false;

// Variável para indicar se a inicialização foi completada
bool inicializacao_completa = false;

#define MAX_AUDIO_SAMPLES 16000  // Exemplo: espaço para 16.000 amostras (ajuste conforme necessário)
uint16_t audio_buffer[MAX_AUDIO_SAMPLES];
volatile int audio_index = 0;

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

void trim(char *str) {
    char *start = str;
    // Remove espaços à esquerda
    while (isspace((unsigned char)*start)) start++;
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
    // Remove espaços à direita
    char *end = str + strlen(str) - 1;
    while (end >= str && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
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
            if (inicializacao_completa == true){
                 // Copia a resposta para uma string temporária para não modificar a original
                char temp[RESPONSE_BUFFER_SIZE];
                strncpy(temp, body, RESPONSE_BUFFER_SIZE);
                temp[RESPONSE_BUFFER_SIZE - 1] = '\0';

                // Usa strtok para separar as perguntas com base na vírgula
                char *token = strtok(temp, ",");
                int i = 0;
                while (token != NULL && i < MAX_PERGUNTAS) {
                    trim(token); // Remove espaços em branco
                    strncpy(perguntas[i], token, MAX_PERGUNTA_LENGTH);
                    perguntas[i][MAX_PERGUNTA_LENGTH - 1] = '\0'; // Garante que a string está terminada
                    token = strtok(NULL, ",");
                    i++;
                }

                // Exibe as perguntas para verificação
                for (int j = 0; j < i; j++) {
                    printf("Pergunta %d: %s\n", j + 1, perguntas[j]);
                }

                inicializacao_completa = false;
                draw_smile();
                print_texto_scroll("Pressione e segure A para falar ou pressione B para escolher uma pergunta", 0, 0, 1);
            } else {
                body += 4; // Pula as quebras de linha
                printf("Corpo da resposta:\n%s\n", body);
                // Armazena o corpo da resposta para que o loop principal o exiba com rolagem
                display_message = body;
    
                // Marca que a mensagem foi recebida
                mensagem_recebida = true;
                esta_processando = false;
    
                // Desenha notificação na matriz de LEDs
                draw_notification();

                // Desenha sorriso na matriz de LEDs novamente
                draw_smile();
    
                printf("Mensagem recebida\n");
            }
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
        print_texto_scroll("Erro ao conectar ao servidor", 0, 0, 1);
        return err;
    }

    // Verifica se foi realizada a inicialização completa
    if (inicializacao_completa == true){
        // Declara o buffer para a requisição HTTP
        char http_request[1000]; // 1000 bytes adicionais para o restante da requisição HTTP

        // Monta o corpo JSON com a pergunta escolhida
        char json_body[1000];
        
        snprintf(json_body, sizeof(json_body), "{\"message\": \"Forneça-me 3 tópicos de perguntas sobre conhecimentos gerais, separadas por vírgula sem espaço entre elas, com no máximo 3 palavras cada. Elas serão exibidas em um display OLED. Retorne APENAS o texto com os tópicos, sem aspas e sem qualquer texto adicional. Exemplos: Capital da Franca,Autor de Dom Quixote,Número de planetas.\"}");
        
        int json_length = strlen(json_body);

        printf("Corpo JSON: %s\n", json_body);

        snprintf(http_request, sizeof(http_request),
                "POST /ai?senha=secret-bitdog HTTP/1.1\r\n"
                "Host: bitdog-api.guilherme762002.workers.dev\r\n"
                "User-Agent: PicoClient/1.0\r\n"
                "Accept: */*\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: %d\r\n"
                "Connection: close\r\n"
                "Cache-Control: no-cache\r\n\r\n"
                "%s",
                json_length, json_body);
        
        printf("Requisição HTTP:\n%s\n", http_request);

        if (tcp_write(tpcb, http_request, strlen(http_request), TCP_WRITE_FLAG_COPY) != ERR_OK) {
            draw_notification();
            npClear();
            npWrite();
            print_texto_scroll("Erro ao enviar a requisicao HTTP devido a limitacao da placa BitDogLab", 0, 0, 1);
            printf("Erro ao enviar a requisição HTTP\n");
            
            return ERR_VAL;
        }
        tcp_output(tpcb); // Envia imediatamente
        printf("Requisição HTTP enviada\n");
    } else if (botao_b_foi_pressionado == false){
    // Se botão B não foi pressionado, envia a requisição HTTP contendo o áudio
        // Guarde o valor atual de audio_index para calcular o total de bytes
        size_t captured_audio_bytes = audio_index * sizeof(uint16_t);
        size_t required_size = 4 * ((captured_audio_bytes + 2) / 3) + 1;

        // Aloca dinamicamente o buffer para o áudio codificado
        char *encoded_audio = malloc(required_size);
        if (!encoded_audio) {
            print_texto_scroll("Erro: memoria insuficiente para a codificação base64", 0, 0, 1);
            printf("Erro: memória insuficiente para a codificação base64\n");
            return ERR_MEM;
        }

        base64_encode((const uint8_t*)audio_buffer, captured_audio_bytes, encoded_audio, required_size);
        printf("Áudio codificado em base64: %s\n", encoded_audio);

        // Calcula o tamanho necessário para json_body
        size_t json_body_size = required_size + 100; // 100 bytes adicionais para o restante do JSON
        char *json_body = malloc(json_body_size);
        if (!json_body) {
            print_texto_scroll("Erro: memoria insuficiente para o corpo JSON", 0, 0, 1);
            printf("Erro: memória insuficiente para o corpo JSON\n");
            free(encoded_audio);
            return ERR_MEM;
        }

        // Monta o corpo JSON com o áudio codificado
        snprintf(json_body, json_body_size, "{\"audioBase64\": \"%s\"}", encoded_audio);
        int json_length = strlen(json_body);

        printf("Corpo JSON: %s\n", json_body);
        printf("Tamanho do JSON: %d\n", json_length);

        // Declara o buffer para a requisição HTTP
        char http_request[json_body_size + 200]; // 200 bytes adicionais para o restante da requisição HTTP

        // // json body de teste
        // char json_body_teste[] = "{\"audioBase64\": \"U29ycnksIEkgY2Fubm90IGhlbHAgdG8gYmUgYSB0ZXN0IGJvZHk=\"}";

        // // length do json body de teste
        // int json_length_teste = strlen(json_body_teste);

        snprintf(http_request, sizeof(http_request),
                "POST /voice-to-text?senha=secret-bitdog HTTP/1.1\r\n"
                "Host: bitdog-api.guilherme762002.workers.dev\r\n"
                "User-Agent: PicoClient/1.0\r\n"
                "Accept: */*\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: %d\r\n"
                "Accept-Encoding: gzip, deflate, br\r\n"
                "Connection: close\r\n"
                "Cache-Control: no-cache\r\n\r\n"
                "%s",
                json_length, json_body);

        printf("Requisição HTTP:\n%s\n", http_request);

        if (tcp_write(tpcb, http_request, strlen(http_request), TCP_WRITE_FLAG_COPY) != ERR_OK) {
            draw_notification();
            npClear();
            npWrite();
            print_texto_scroll("Erro ao enviar a requisicao HTTP devido a limitacao da placa BitDogLab", 0, 0, 1);
            printf("Erro ao enviar a requisição HTTP\n");
            free(encoded_audio);
            free(json_body);
            return ERR_VAL;
        }
        tcp_output(tpcb); // Envia imediatamente
        printf("Requisição HTTP enviada\n");
            
        // Esvazia o buffer de áudio
        audio_index = 0;
                
        free(encoded_audio);
        free(json_body);
    } else if (botao_b_foi_pressionado == true){
        // Declara o buffer para a requisição HTTP
        char http_request[1000]; // 1000 bytes adicionais para o restante da requisição HTTP

        // Monta o corpo JSON com a pergunta escolhida
        char json_body[1000];

        // Switch case para montar o json_body de acordo com a pergunta escolhida
        switch (pergunta_selecionada)
        {
            case 12:
                printf("Montando json_body para a pergunta 12\n");
                snprintf(json_body, sizeof(json_body), "{\"message\": \"%s\"}", perguntas[0]);
                break;
            case 24:
                printf("Montando json_body para a pergunta 24\n");
                snprintf(json_body, sizeof(json_body), "{\"message\": \"%s\"}", perguntas[1]);
                break;
            case 36:
                printf("Montando json_body para a pergunta 36\n");
                snprintf(json_body, sizeof(json_body), "{\"message\": \"%s\"}", perguntas[2]);
                break;
            default:
                break;  
        }
        
        int json_length = strlen(json_body);

        printf("Corpo JSON: %s\n", json_body);

        snprintf(http_request, sizeof(http_request),
                "POST /ai?senha=secret-bitdog HTTP/1.1\r\n"
                "Host: bitdog-api.guilherme762002.workers.dev\r\n"
                "User-Agent: PicoClient/1.0\r\n"
                "Accept: */*\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: %d\r\n"
                "Connection: close\r\n"
                "Cache-Control: no-cache\r\n\r\n"
                "%s",
                json_length, json_body);
        
        printf("Requisição HTTP:\n%s\n", http_request);

        if (tcp_write(tpcb, http_request, strlen(http_request), TCP_WRITE_FLAG_COPY) != ERR_OK) {
            draw_notification();
            npClear();
            npWrite();
            print_texto_scroll("Erro ao enviar a requisicao HTTP devido a limitacao da placa BitDogLab", 0, 0, 1);
            printf("Erro ao enviar a requisição HTTP\n");
            
            return ERR_VAL;
        }
        tcp_output(tpcb); // Envia imediatamente
        printf("Requisição HTTP enviada\n");
                
        pergunta_selecionada = 0;
    }
    
    return ERR_OK;
}


/**
 * Callback do DNS (usado quando o DNS não está em cache).
 */
static void dns_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    if (ipaddr == NULL) {
        print_texto_scroll("Erro ao resolver o endereço do servidor", 0, 0, 1);
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
        print_texto_scroll("Erro ao conectar ao servidor", 0, 0, 1);
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
    err_t err = dns_gethostbyname("bitdog-api.guilherme762002.workers.dev", &server_ip, dns_callback, NULL);
    if (err == ERR_OK) {
        // Se o DNS já está em cache, conecta imediatamente.
        printf("DNS resolvido imediatamente: %s\n", ipaddr_ntoa(&server_ip));
        struct tcp_pcb *pcb = tcp_new();
        if (!pcb) {
            print_texto_scroll("Erro ao criar PCB", 0, 0, 1);
            printf("Erro ao criar PCB\n");
            return;
        }
        tcp_recv(pcb, http_client_callback);
        if (tcp_connect(pcb, &server_ip, 80, tcp_connected_callback) != ERR_OK) {
            print_texto_scroll("Erro ao conectar ao servidor", 0, 0, 1);
            printf("Erro ao conectar ao servidor\n");
            return;
        }
    } else if (err == ERR_INPROGRESS) {
        printf("Resolução do DNS em andamento...\n");
    } else {
        print_texto_scroll("Erro ao iniciar a resolucao do DNS", 0, 0, 1);
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

// Pinos e módulo I2C
#define I2C_PORT i2c1
#define PINO_SCL 14
#define PINO_SDA 15

// Configuração do pino do buzzer
#define BUZZER_PIN 21

bool program_running = false; // Indica se um programa acionado pelo botão b está em execução

// Variáveis de controle do menu
uint pos_y = 12; // Variável de controle do menu

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
    // Force o ADC a ler do canal do microfone
    adc_select_input(MIC_CHANNEL);

    adc_fifo_drain(); // Limpa o FIFO do ADC.
    adc_run(false);   // Desliga o ADC para configurar o DMA.

    dma_channel_configure(dma_channel, &dma_cfg,
        adc_buffer,         // Escreve no buffer.
        &(adc_hw->fifo),    // Lê do ADC.
        SAMPLES,            // Faz SAMPLES amostras.
        true                // Liga o DMA.
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
        printf("Tensão: %f\n", v);
        
        uint count = 0;
        
        while ((v -= ADC_STEP/20) > 0.f)
          ++count;
        
        return count;
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

// Função para capturar e armazenar um bloco de amostras
void capture_audio_block() {
    size_t total_bytes = audio_index * sizeof(uint16_t);
    // Captura um bloco de SAMPLES amostras (já implementado em sample_mic())
    sample_mic();
    // Verifica se há espaço suficiente no buffer global
    if (audio_index + SAMPLES < MAX_AUDIO_SAMPLES) {
        // Copia as amostras capturadas para o buffer global
        memcpy(&audio_buffer[audio_index], adc_buffer, SAMPLES * sizeof(uint16_t));
        audio_index += SAMPLES;
    } else {
        // Buffer cheio - pode definir uma flag para interromper a gravação ou descartar o excesso
        printf("Buffer de áudio cheio!\n");
    }
}

static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Tabela para calcular os caracteres de padding
static const int mod_table[] = {0, 2, 1};

void base64_encode(const uint8_t *data, size_t input_length, char *encoded_data, size_t encoded_size) {
    size_t output_length = 4 * ((input_length + 2) / 3);
    if (encoded_size < output_length + 1) {
        // O buffer fornecido não é grande o suficiente
        return;
    }
    size_t i, j;
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

/**
 * Função para escrever texto no display
 * 
 * @param msg   Mensagem a ser exibida
 * @param pos_x Posição X no display
 * @param pos_y Posição Y no display
 * @param scale Escala do texto
 */
void print_texto(char *msg, uint pos_x, uint pos_y, uint scale) {
    ssd1306_draw_string(&disp, pos_x, pos_y, scale, msg);
    ssd1306_show(&disp);
}

/**
 * Desenha um retângulo (usado como seletor no menu)
 * 
 * @param x1 Coordenada X do canto superior esquerdo
 * @param y1 Coordenada Y do canto superior esquerdo
 * @param x2 Coordenada X do canto inferior direito
 * @param y2 Coordenada Y do canto inferior direito
 */
void print_retangulo(int x1, int y1, int x2, int y2) {
    ssd1306_draw_empty_square(&disp, x1, y1, x2, y2);
    ssd1306_show(&disp);
}

// Desenha o menu na tela OLED
void desenha_menu() {
    ssd1306_clear(&disp);
    print_texto("Selecione a pergunta", 6, 2, 1);

    // O retângulo tem altura de 12 pixels a partir da posição pos_y
    print_retangulo(2, pos_y, 120, 18);

    // Verifica se as perguntas foram recebidas
    if (strlen(perguntas[0]) > 0 && strlen(perguntas[1]) > 0 && strlen(perguntas[2]) > 0) {
        // Exibe as perguntas recebidas
        print_texto(perguntas[0], 6, 18, 1.5);
        print_texto(perguntas[1], 6, 30, 1.5);
        print_texto(perguntas[2], 6, 42, 1.5);
    }
}


/**
 * Lê os valores dos eixos do joystick (X e Y)
 * 
 * @param vrx_value Ponteiro para armazenar o valor do eixo X
 * @param vry_value Ponteiro para armazenar o valor do eixo Y
 */
void joystick_read_axis_menu_oled(uint16_t *vrx_value, uint16_t *vry_value) {
    // Seleciona o canal ADC para o eixo X e lê o valor
    adc_select_input(ADC_CHANNEL_0);
    sleep_us(2);
    *vrx_value = adc_read();

    // Seleciona o canal ADC para o eixo Y e lê o valor
    adc_select_input(ADC_CHANNEL_1);
    sleep_us(2);
    *vry_value = adc_read();
}

// Interrompe a execução do programa atual e retorna ao menu
void stop_program() {
    program_running = false;
    display_message = NULL;
    mensagem_recebida = false;

    // Desenha o menu novamente
    desenha_menu();
}

// Configuração da frequência do buzzer (em Hz)
#define BUZZER_FREQUENCY 100

// Definição de uma função para inicializar o PWM no pino do buzzer
void pwm_init_buzzer(uint pin) {
    // Configurar o pino como saída de PWM
    gpio_set_function(pin, GPIO_FUNC_PWM);

    // Obter o slice do PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(pin);

    // Configurar o PWM com frequência desejada
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (BUZZER_FREQUENCY * 4096)); // Divisor de clock
    pwm_init(slice_num, &config, true);

    // Iniciar o PWM no nível baixo
    pwm_set_gpio_level(pin, 0);
}

 
/** 
 * Função para reproduzir um beep no buzzer
 */
void beep(uint pin, uint duration_ms) {
    // Obter o slice do PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(pin);

    // Configurar o duty cycle para 50% (ativo)
    pwm_set_gpio_level(pin, 2048);

    // Temporização
    sleep_ms(duration_ms);

    // Desativar o sinal PWM (duty cycle 0)
    pwm_set_gpio_level(pin, 0);

    // Pausa entre os beeps
    sleep_ms(100); // Pausa de 100ms
}

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
 * Função para exibir menu no display OLED
 * 
 * @return int Código de retorno
 */
int menu_oled() {
    desenha_menu(); // Exibe o menu inicial

    uint countdown = 0; // Para controle de histerese (movimento para baixo)
    uint countup = 2;   // Para controle de histerese (movimento para cima)
    uint last_pos_y = pos_y;

    // Variáveis para leitura do joystick e cálculo dos offsets
    uint16_t vry_value = 0;
    int offset_y = 0;
    int prev_scroll_y = scroll_y; // Armazena o valor anterior de scroll_y

    while (1) {
        // Usa ADC (canal 0) para detectar o movimento do joystick no eixo Y
        adc_select_input(0);
        uint adc_y_raw = adc_read();
        const uint bar_width = 40;
        const uint adc_max = (1 << 12) - 1;
        uint bar_y_pos = adc_y_raw * bar_width / adc_max;

        // Ajusta a posição do seletor conforme a leitura do ADC
        if (bar_y_pos < 15 && countdown < 2) {
            pos_y += 12;
            countdown++;
            if (countup > 0) countup--;
        } else if (bar_y_pos > 25 && countup < 2) {
            pos_y -= 12;
            countup++;
            if (countdown > 0) countdown--;
        }

        // Atualiza o menu se a posição do seletor mudar e nenhum programa estiver em execução
        if (pos_y != last_pos_y && !program_running) {
            last_pos_y = pos_y;
            desenha_menu();
        }

        sleep_ms(100);

        // Se o botão A for pressionado, sai da função menu_oled
        if (gpio_get(BUTTON_A) == 0) {
            botao_b_foi_pressionado = false;

            return 0;

            // Aguarda liberação do botão para evitar múltiplas detecções
            while (gpio_get(BUTTON_A) == 0) {
                sleep_ms(10);
            }
        }

        // Se mensagem foi recebida, exibe
        if (mensagem_recebida == true) {
            print_texto_scroll(display_message, 0, 0, 1);

            // Marca que a mensagem foi exibida
            mensagem_recebida = false;
        }

        
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
        }

        // Verifica se o botão do joystick foi pressionado
        if (gpio_get(SW) == 0) {
            if (program_running) {
                // Se alguma mensagem estiver sendo exibida, interrompe a exibição e volta ao menu
                stop_program();
            } else {
                program_running = true;
                switch (pos_y) {
                    case 12:
                        printf("Primeira opção selecionada\n");
                        pergunta_selecionada = 12;

                        // Envia requisição HTTP
                        send_http_request();

                        print_texto_scroll("Processando...", 0, 0, 1);
                        break;
                    case 24:
                        printf("Segunda opção selecionada\n");    
                        pergunta_selecionada = 24;

                        // Envia requisição HTTP 
                        send_http_request();

                        print_texto_scroll("Processando...", 0, 0, 1);
                        break;
                    case 36:
                        printf("Terceira opção selecionada\n");
                        pergunta_selecionada = 36;

                        // Envia requisição HTTP
                        send_http_request();

                        print_texto_scroll("Processando...", 0, 0, 1);
                        break;
                }
            }
            
            // Aguarda liberação do botão para evitar múltiplas detecções
            while (gpio_get(SW) == 0) {
                sleep_ms(10);
            }
        }
    }
    return 0;
}


/**
 * Main.
 */
int main() {
    stdio_init_all();  // Inicializa a saída padrão

    // Inicializa ADC para leitura do joystick
    adc_init();

    // Inicializar buzzer
    pwm_init_buzzer(BUZZER_PIN);

    // Inicializa o display OLED e o joystick
    init_display();
    init_joystick();

    // Configuração do GPIO do Botão A como entrada com pull-up interno
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);

    // Configuração do GPIO do Botão B como entrada com pull-up interno
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);

    // Inicialização do buffer: zera o índice antes da gravação
    audio_index = 0;

    print_texto_scroll("Inicializando Assistente BitDog AI", 0, 0, 1);

    // Inicializa o Wi-Fi
    if (cyw43_arch_init()) {
        printf("Erro ao inicializar o Wi-Fi\n");
        return 1;
    }
    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        print_texto_scroll("Falha ao conectar ao Wi-Fi", 0, 0, 1);
        printf("Falha ao conectar ao Wi-Fi\n");
        return 1;
    } else {
        printf("Conectado.\n");
        uint8_t *ip_address = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);
        printf("Endereço IP %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
    }
    printf("Wi-Fi conectado!\n");

    // Preparação da matriz de LEDs.
    npInit(LED_PIN, LED_COUNT);

    // Preparação do ADC.
    printf("Preparando ADC...\n");
    adc_gpio_init(MIC_PIN);
    adc_init();

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

    printf("Preparando DMA...\n");
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
    printf("Configurações completas!\n");

    inicializacao_completa = true;

    // Envia requisição HTTP para obter as perguntas
    send_http_request();

    // Variáveis para leitura do joystick e cálculo dos offsets
    uint16_t vry_value = 0;
    int offset_y = 0;
    int prev_scroll_y = scroll_y; // Armazena o valor anterior de scroll_y

    // Loop principal: coleta áudio e atualiza a rolagem se o corpo da resposta estiver disponível
    while (true) {
        cyw43_arch_poll();  // Necessário para manter o Wi-Fi ativo
        sleep_ms(50);

        // Se mensagem foi recebida, exibe
        if (mensagem_recebida == true) {
            print_texto_scroll(display_message, 0, 0, 1);

            // Marca que a mensagem foi exibida
            mensagem_recebida = false;
        }

        // Verifica se o botão está pressionado
        while (gpio_get(BUTTON_A) == 0) {
            print_texto_scroll("Captando a sua pergunta...", 0, 0, 1);

            // Captura áudio enquanto o botão estiver pressionado
            capture_audio_block();
                    
            // Realiza uma amostragem do microfone.
            sample_mic();

            // Pega a potência média da amostragem do microfone.
            float avg = mic_power();
            avg = 2.f * abs(ADC_ADJUST(avg)); // Ajusta para intervalo de 0 a 3.3V. (apenas magnitude, sem sinal)

            uint intensity = get_intensity(avg); // Calcula intensidade a ser mostrada na matriz de LEDs.

            // Limpa a matriz de LEDs.
            npClear();

            // A depender da intensidade do som, acende LEDs específicos com cores diferentes
            switch (intensity) {
                case 0: break; // Se o som for muito baixo, não acende nada.
                case 1:
                npSetLED(12, 0, 0, 80); // Acende apenas o centro.
                break;
                case 2:
                npSetLED(12, 0, 0, 120); // Acente o centro.
        
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

            botao_foi_pressionado = true;
        }
        
        // Se botão B for pressionado, exibe o menu no display OLED
        if (gpio_get(BUTTON_B) == 0) {
            botao_b_foi_pressionado = true;
            menu_oled();
        }
        
        if (gpio_get(BUTTON_A) == 1) {            
            if (botao_foi_pressionado == true) {
                // Quando o botão for solto, a gravação está finalizada.
                // Aqui, audio_index contém o número total de amostras gravadas.
                printf("Gravação finalizada: %d amostras capturadas.\n", audio_index);

                print_texto_scroll("Processando...", 0, 0, 1);
                display_message = NULL; // Limpa a mensagem exibida

                // limpa matriz de LEDs
                npClear();
                npWrite();

                // Envia a requisição HTTP
                send_http_request();

                // Marca que o botão foi pressionado
                botao_foi_pressionado = false;
                esta_processando = true;
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
            }
        }
        
    }

    cyw43_arch_deinit();
    return 0;
}