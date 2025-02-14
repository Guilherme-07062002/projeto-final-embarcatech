#ifndef GLOBALS_H
#define GLOBALS_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// Variáveis para gerenciamento de estado da aplicação
extern char *display_message; // Ponteiro para o texto a ser exibido com rolagem (corpo da resposta HTTP)
extern int scroll_y; // Variável para acumular a posição do scroll
extern int pergunta_selecionada; // Variável para indicar qual pergunta foi selecionada
extern bool mensagem_recebida; // Variavel que indica que a mensagem foi recebida
extern bool botao_foi_pressionado; // Variavel que irá indicar quando o botão foi pressionado para enviar a requisição HTTP
extern bool botao_b_foi_pressionado; // Variavel que irá indicar se botão B foi pressionado para quando enviar requisição HTTP enviar pergunta escolhida
extern bool esta_processando; // Variável para indicar se a mensagem está sendo processada
extern bool inicializacao_completa; // Variável para indicar se a inicialização foi completada
extern bool program_running; // Indica se um programa acionado pelo botão b está em execução
extern unsigned int pos_y; // Variável de controle do menu (posição Y do seletor)

#endif // GLOBALS_H