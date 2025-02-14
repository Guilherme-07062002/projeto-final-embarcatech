#include "globals.h"

// Variáveis para gerenciamento de estado da aplicação
char *display_message = NULL; // Ponteiro para o texto a ser exibido com rolagem (corpo da resposta HTTP)
int scroll_y = 0; // Variável para acumular a posição do scroll
int pergunta_selecionada = 0; // Variável para indicar qual pergunta foi selecionada
bool mensagem_recebida = false; // Variavel que indica que a mensagem foi recebida
bool botao_foi_pressionado = false; // Variavel que irá indicar quando o botão foi pressionado para enviar a requisição HTTP
bool botao_b_foi_pressionado = false; // Variavel que irá indicar se botão B foi pressionado para quando enviar requisição HTTP enviar pergunta escolhida
bool esta_processando = false; // Variável para indicar se a mensagem está sendo processada
bool inicializacao_completa = false; // Variável para indicar se a inicialização foi completada
bool program_running = false; // Indica se um programa acionado pelo botão b está em execução
#define my_abs(x) (((x) < 0) ? (-(x)) : (x)) // Função para retornar o valor absoluto de um número
unsigned int pos_y = 12; // Variável de controle do menu (posição Y do seletor)