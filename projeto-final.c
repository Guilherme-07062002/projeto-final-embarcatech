#include "neopixel.h"
#include "matriz.h"
#include "buzzer.h"
#include "display.h"
#include "internet.h"
#include "globals.h"
#include "microfone.h"
#include "button.h"
#include "pico/cyw43_arch.h"
#include "joystick.h"

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
    adc_gpio_init(MIC_PIN);
    adc_init();

    // Configuração do ADC.
    adc_select_input(MIC_CHANNEL);
    adc_fifo_setup(
        true, // Habilitar FIFO
        true, // Habilitar request de dados do DMA
        1, // Threshold para ativar request DMA é 1 leitura do ADC
        false, // Não usar bit de erro
        false // Não fazer downscale das amostras para 8-bits, manter 12-bits.
    );
    adc_set_clkdiv(ADC_CLOCK_DIV);

    // Tomando posse de canal do DMA.
    dma_channel = dma_claim_unused_channel(true);

    // Configurações do DMA.
    dma_cfg = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_16); // Tamanho da transferência é 16-bits (usamos uint16_t para armazenar valores do ADC)
    channel_config_set_read_increment(&dma_cfg, false); // Desabilita incremento do ponteiro de leitura (lemos de um único registrador)
    channel_config_set_write_increment(&dma_cfg, true); // Habilita incremento do ponteiro de escrita (escrevemos em um array/buffer)
    channel_config_set_dreq(&dma_cfg, DREQ_ADC); // Usamos a requisição de dados do ADC
    
    sample_mic(); // Realiza uma amostragem do microfone para testar o funcionamento.	
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