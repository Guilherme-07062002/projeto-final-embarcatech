#include "internet.h"

int response_length = 0;
ip_addr_t server_ip;

char perguntas[MAX_PERGUNTAS][MAX_PERGUNTA_LENGTH]; // Array para armazenar as perguntas
char response_buffer[RESPONSE_BUFFER_SIZE];
int response_length;

/**
 * Callback para processar a resposta HTTP.
 */
err_t http_client_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
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
                beep(BUZZER_PIN, 100); // Bipe de 100ms
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

    // Libera o pbuf
    pbuf_free(p);
    return ERR_OK;
}

/**
 * Callback chamado quando a conexão TCP é estabelecida.
 */
err_t tcp_connected_callback(void *arg, struct tcp_pcb *tpcb, err_t err) {
    if (err != ERR_OK) {
        printf("Erro ao conectar ao servidor: %d\n", err);
        print_texto_scroll("Erro ao conectar ao servidor", 0, 0, 1);
        return err;
    }

    // Verifica se foi realizada a inicialização completa
    if (inicializacao_completa == true){
        // Declara o buffer para a requisição HTTP e o corpo JSON
        char http_request[1000];
        char json_body[1000];
        
        // Monta o corpo JSON
        snprintf(json_body, sizeof(json_body), "{\"message\": \"[PROMPT INICIAL] Forneça-me 3 tópicos de perguntas sobre conhecimentos gerais, separadas por vírgula sem espaço entre elas, com no máximo 3 palavras cada. Elas serão exibidas em um display OLED. Retorne APENAS o texto com os tópicos, sem aspas e sem qualquer texto adicional. Exemplos: Capital da Franca,Autor de Dom Quixote,Número de planetas.\"}");
        int json_length = strlen(json_body);

        printf("Corpo JSON: %s\n", json_body);
        // Monta a requisição HTTP
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

        // Codifica o áudio em base64
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

        // Esse json_body e length de teste comprovam que é possível realizar a requisição passando o base64 codificado
        // Desde que o tamanho do body seja inferior
        // // json body de teste
        // char json_body_teste[] = "{\"audioBase64\": \"U29ycnksIEkgY2Fubm90IGhlbHAgdG8gYmUgYSB0ZXN0IGJvZHk=\"}";
        // // length do json body de teste
        // int json_length_teste = strlen(json_body_teste);

        // Monta a requisição HTTP
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
            
        // Libera a memória alocada
        audio_index = 0;
        free(encoded_audio);
        free(json_body);
    } else if (botao_b_foi_pressionado == true){
        // Declara o buffer para a requisição HTTP e o corpo JSON
        char http_request[1000];
        char json_body[1000];

        // Switch case para montar o json_body de acordo com a pergunta escolhida
        switch (pergunta_selecionada)
        {
            case 12:
                printf("Montando json_body para a primeira pergunta\n");
                snprintf(json_body, sizeof(json_body), "{\"message\": \"%s\"}", perguntas[0]);
                break;
            case 24:
                printf("Montando json_body para a segunda pergunta\n");
                snprintf(json_body, sizeof(json_body), "{\"message\": \"%s\"}", perguntas[1]);
                break;
            case 36:
                printf("Montando json_body para a terceira pergunta\n");
                snprintf(json_body, sizeof(json_body), "{\"message\": \"%s\"}", perguntas[2]);
                break;
            default:
                break;  
        }
        
        // Calcula o tamanho do json_body
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
                
        // Libera a memória alocada
        pergunta_selecionada = 0;
    }
    
    return ERR_OK;
}


/**
 * Callback do DNS (usado quando o DNS não está em cache).
 */
void dns_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
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
void send_http_request(void) {
    // Limpa o buffer da resposta anterior
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