#include "display.h"

ssd1306_t disp; // Display OLED

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
 * Desenha o menu no display OLED
 */
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
 * Função para remover espaços em branco à esquerda e à direita de uma string.
 */
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

// Interrompe a execução do programa atual e retorna ao menu
void stop_program() {
    program_running = false;
    display_message = NULL;
    mensagem_recebida = false;

    // Desenha o menu novamente
    desenha_menu();
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
                        send_http_request();
                        print_texto_scroll("Processando...", 0, 0, 1);
                        break;
                    case 24:
                        printf("Segunda opção selecionada\n");    
                        pergunta_selecionada = 24;
                        send_http_request();
                        print_texto_scroll("Processando...", 0, 0, 1);
                        break;
                    case 36:
                        printf("Terceira opção selecionada\n");
                        pergunta_selecionada = 36;
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