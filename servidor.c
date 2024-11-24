/******************************************************************************
 ** ISCTE-IUL: Trabalho prático 2 de Sistemas Operativos
 **
 ** Aluno: Nº: 110976      Nome: Diogo Pedro Cordeiro Pereira
 ** Nome do Módulo: servidor.c v1.2
 ** Descrição/Explicação do Módulo:
 ** Descrito em linhas de comentário
 **
 ******************************************************************************/
#include "common.h"
// #define SO_HIDE_DEBUG   // Uncomment this line to hide all @DEBUG statements
#include "so_utils.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

/* Variáveis globais */
Login clientRequest; // Variável que tem o pedido enviado do Cliente para o Servidor
int index_client;    // Índice do cliente que fez o pedido ao servidor/servidor dedicado na BD

/* Protótipos de funções */
int existsDB_S1(char *);                // S1:   Função a ser implementada pelos alunos
int createFifo_S2(char *);              // S2:   Função a ser implementada pelos alunos
int triggerSignals_S3();                // S3:   Função a ser implementada pelos alunos
Login readRequest_S4(char *);           // S4:   Função a ser implementada pelos alunos
int createServidorDedicado_S5();        // S5:   Função a ser implementada pelos alunos
void deleteFifoAndExit_S6();            // S6:   Função a ser implementada pelos alunos
void trataSinalSIGINT_S7(int);          // S7:   Função a ser implementada pelos alunos
void trataSinalSIGCHLD_S8(int);         // S8:   Função a ser implementada pelos alunos
int triggerSignals_SD9();               // SD9:  Função a ser implementada pelos alunos
int validaPedido_SD10(Login);           // SD10:  Função a ser implementada pelos alunos
int buildNomeFifo(char *, int, char *, int, char *); // Função a ser implementada pelos alunos
int procuraUtilizadorBD_SD11(Login, char *, Login *); // SD11: Função a ser implementada pelos alunos
int reservaUtilizadorBD_SD12(Login *, char *, int, Login);  // SD12: Função a ser implementada pelos alunos
int createFifo_SD13();                  // SD13: Função a ser implementada pelos alunos
int sendAckLogin_SD14(Login, char *);   // SD14: Função a ser implementada pelos alunos
int readFimSessao_SD15(char *);         // SD15: Função a ser implementada pelos alunos
int terminaSrvDedicado_SD16(Login *, char *, int);  // SD16: Função a ser implementada pelos alunos
void deleteFifoAndExit_SD17();          // SD17: Função a ser implementada pelos alunos
void trataSinalSIGUSR1_SD18(int);       // SD18: Função a ser implementada pelos alunos

/**
 * Main: Processamento do processo Servidor e dos processos Servidor Dedicado
 *       Não é suposto que os alunos alterem nada na função main()
 * @return int Exit value
 */
int main() {
    // S1
    so_exit_on_error(existsDB_S1(FILE_DATABASE), "S1");
    // S2
    so_exit_on_error(createFifo_S2(FILE_REQUESTS), "S2");
    // S3
    so_exit_on_error(triggerSignals_S3(FILE_REQUESTS), "S3");

    while (TRUE) {  // O processamento do Servidor é cíclico e iterativo
        // S4
        clientRequest = readRequest_S4(FILE_REQUESTS);    // Convenciona-se que se houver problemas, esta função coloca clientRequest.nif = -1
        if (clientRequest.nif < 0)
            continue;   // Caso a leitura tenha tido problemas, não avança, e lê o próximo item
        // S5
        int pidFilho = createServidorDedicado_S5();
        if (pidFilho < 0)
            continue;   // Caso o fork() tenha tido problemas, não avança, e lê o próximo item
        else if (pidFilho > 0) // Processo Servidor (Pai)
            continue;   // Caso tudo tenha corrido bem, o PROCESSO PAI volta para S4

        // Este código é apenas corrido pelo Processo Servidor Dedicado (Filho)
        // SD9
        so_exit_on_error(triggerSignals_SD9(), "SD9");
        // SD10
        so_exit_on_error(validaPedido_SD10(clientRequest), "SD10");
        // SD11
        Login elementoClienteBD;
        index_client = procuraUtilizadorBD_SD11(clientRequest, FILE_DATABASE, &elementoClienteBD);
        // SD12
        so_exit_on_error(reservaUtilizadorBD_SD12(&clientRequest, FILE_DATABASE, index_client,
                                                                    elementoClienteBD), "SD12");
        // SD13
        char nameFifoServidorDedicado[SIZE_FILENAME]; // Nome do FIFO do Servidor Dedicado
        so_exit_on_error(createFifo_SD13(nameFifoServidorDedicado), "SD13");
        // SD14
        char nomeFifoCliente[SIZE_FILENAME];
        so_exit_on_error(sendAckLogin_SD14(clientRequest, nomeFifoCliente), "SD14");
        // SD15
        so_exit_on_error(readFimSessao_SD15(nameFifoServidorDedicado), "SD15");
        // SD16
        so_exit_on_error(terminaSrvDedicado_SD16(&clientRequest, FILE_DATABASE, index_client),
                                                                                        "SD16");
        // SD17
        deleteFifoAndExit_SD17();
        so_exit_on_error(-1, "ERRO: O servidor dedicado nunca devia chegar a este passo");
    }
}

/**
 *  O módulo Servidor é responsável pelo processamento das autenticações dos utilizadores.
 *  Está dividido em duas partes, o Servidor (pai) e zero ou mais Servidores Dedicados (filhos).
 *  Este módulo realiza as seguintes tarefas:
 */

/**
 * @brief S1    Valida se o ficheiro nameDB existe na diretoria local, dá so_error
 *              (i.e., so_error("S1", "")) e termina o Servidor se o ficheiro não existir.
 *              Caso contrário, dá so_success (i.e., so_success("S1", ""));
 *
 * @param nameDB O nome da base de dados (i.e., FILE_DATABASE)
 * @return int Sucesso (0: success, -1: error)
 */
int existsDB_S1(char *nameDB) {
    int result = -1;    // Por omissão retorna erro
    so_debug("< [@param nameDB:%s]", nameDB);

    FILE *file;
    if((file = fopen(nameDB,"r"))!=NULL){
            so_success("S1", "");
            fclose(file);
            result=0;
    }else{
            so_error("S1", "");
    }

    so_debug("> [@return:%d]", result);
    return result;
}

/**
 * @brief S2    Cria o ficheiro com organização FIFO (named pipe) do Servidor, de nome
 *              nameFifo, na diretoria local. Se houver erro na operação, dá so_error e
 *              termina o Servidor. Caso contrário, dá  so_success;
 *
 * @param nameFifo O nome do FIFO do servidor (i.e., FILE_REQUESTS)
 * @return int Sucesso (0: success, -1: error)
 */
int createFifo_S2(char *nameFifo) {
    int result = -1;    // Por omissão retorna erro
    so_debug("< [@param nameFifo:%s]", nameFifo);

    if( mkfifo(nameFifo, 666) != 0 ){
        so_error("S2", "");
    }else{
        so_success("S2", "");
        result=0;
    }

    so_debug("> [@return:%d]", result);
    return result;
}

/**
 * @brief S3    Arma e trata os sinais SIGINT (ver S7) e SIGCHLD (ver S8). Em caso de qualquer
 *              erro a armar os sinais, dá so_error e segue para o passo S6.
 *              Caso contrário, dá so_success;
 *
 * @return int Sucesso (0: success, -1: error)
 */
int triggerSignals_S3() {
    int result = -1;    // Por omissão retorna erro
    so_debug("<");

    if(signal(SIGINT, trataSinalSIGINT_S7) == SIG_ERR) {
        so_error("S3", "");
        deleteFifoAndExit_S6();
    }
    if(signal(SIGCHLD, trataSinalSIGCHLD_S8) == SIG_ERR) {
        so_error("S3", "");
        deleteFifoAndExit_S6();
    }
    result = 0;
    so_success("S3", "");

    so_debug("> [@return:%d]", result);
    return result;
}

/**
 * @brief S4    Abre o FIFO do Servidor para leitura, lê um pedido (acesso direto) que deverá
 *              ser um elemento do tipo Login, e fecha o mesmo FIFO. Se houver erro, dá so_error
 *              e reinicia o processo neste mesmo passo S4, lendo um novo pedido.
 *              Caso contrário, dá so_success <nif> <senha> <pid_cliente>;
 *              Convenciona-se que se houver problemas, esta função retorna request.nif = -1;
 *
 * @param nameFifo O nome do FIFO do servidor (i.e., FILE_REQUESTS)
 * @return Login Elemento com os dados preenchidos. Se nif=-1, significa que o elemento é inválido
 */
Login readRequest_S4(char *nameFifo) {
    Login request;
    request.nif = -1;   // Por omissão retorna erro
    so_debug("< [@param nameFifo:%s]", nameFifo);

    FILE *fifo = fopen (nameFifo, "r"); 
    if(fifo == NULL){
        so_error("S4","");
    }else{
        if( fread(&request, sizeof(Login), 1, fifo) < 1 ){ 
            so_error("S4",""); 
            request.nif = -1;
        }else{
            fclose(fifo);
            so_success("S4","%d %s %d", request.nif, request.senha,request.pid_cliente); 
        }
    }

    so_debug("> [@return nif:%d, senha:%s, pid_cliente:%d]", request.nif, request.senha,
                                                                            request.pid_cliente);
    return request;
}

/**
 * @brief S5    Cria um processo filho (fork) Servidor Dedicado. Se houver erro, dá so_error.
 *              Caso contrário, o processo Servidor Dedicado (filho) continua no passo SD9,
 *              enquanto o processo Servidor (pai) dá
 *              so_success "Servidor Dedicado: PID <pid_servidor_dedicado>",
 *              e recomeça o processo no passo S4;
 *
 * @return int PID do processo filho, se for o processo Servidor (pai),
 *         0 se for o processo Servidor Dedicado (filho), ou -1 em caso de erro.
 */
int createServidorDedicado_S5() {
    int pid_filho = -1;    // Por omissão retorna erro
    so_debug("<");

    pid_filho = fork();
    if(pid_filho < 0){
        so_error("S5","");
    }else{
        if(pid_filho > 0){
            so_success("S5", "Servidor Dedicado: PID %d", pid_filho);
        }
    }

    so_debug("> [@return:%d]", pid_filho);
    return pid_filho;
}

/**
 * @brief S6    Remove o FIFO do Servidor, de nome FILE_REQUESTS, da diretoria local.
 *              Em caso de erro, dá so_error, caso contrário, dá so_success.
 *              Em ambos os casos, termina o processo Servidor.
 */
void deleteFifoAndExit_S6() {
    so_debug("<");

    if (remove(FILE_REQUESTS) == 0) {
        so_success("S6","");
    } else {
        so_error("S6","");
    }

    so_debug(">");
    exit(0);
}

/**
 * @brief S7    O sinal armado SIGINT serve para o dono da loja encerrar o Servidor, usando o
 *              atalho <CTRL+C>. Se receber esse sinal (do utilizador via Shell), o Servidor dá
 *              so_success "Shutdown Servidor", e faz as ações:
 *        S7.1  Abre o ficheiro bd_utilizadores.dat para leitura. Em caso de erro na abertura do
 *              ficheiro, dá so_error e segue para o passo S6. Caso contrário, dá so_success;
 *        S7.2  Lê (acesso direto) um elemento do tipo Login deste ficheiro. Em caso de erro na
 *              leitura do ficheiro, dá so_error e segue para o passo S6;
 *        S7.3  Se o elemento Login lido tiver pid_servidor_dedicado > 0, então envia ao PID
 *              desse Servidor Dedicado o sinal SIGUSR1;
 *        S7.4  Se tiver chegado ao fim do ficheiro bd_utilizadores.dat, fecha o ficheiro e dá
 *              so_success. Caso contrário, volta ao passo S7.2;
 *        S7.5  Vai para o passo S6.
 *
 * @param sinalRecebido nº do Sinal Recebido (preenchido pelo SO)
 */
void trataSinalSIGINT_S7(int sinalRecebido) {
    so_debug("< [@param sinalRecebido:%d]", sinalRecebido);

    so_success("S7","Shutdown Servidor");

    // S7.1 //
    FILE* file;
    file = fopen(FILE_DATABASE,"r");
    if(file == NULL){
        so_error("S7.1", "");
    }else{
        so_success("S7.1", "");

        // S7.2 //
        Login access;
        int index=0;
        while(TRUE){
            if(fread(&access, sizeof(Login), 1, file) == 1){
                index ++;
                fseek(file,index*sizeof(Login),SEEK_SET);
                if(access.pid_servidor_dedicado > 0){ 
                    kill(access.pid_servidor_dedicado, SIGUSR1); 
                }
            }else{
                break;
            }   
        }

        // S7.4 //
        so_success("S7.4","");
        fclose(file);
    }
    // S7.5 //
    deleteFifoAndExit_S6();

    so_debug(">");
    exit(0);
}

/**
 * @brief S8    O sinal armado SIGCHLD serve para que o Servidor seja alertado quando um dos
 *              seus filhos Servidor Dedicado terminar. Se o Servidor receber esse sinal,
 *              identifica o PID do Servidor Dedicado que terminou (usando wait), e dá
 *              so_success "Terminou Servidor Dedicado <pid_servidor_dedicado>", retornando ao
 *              passo S4 sem reportar erro;
 *
 * @param sinalRecebido nº do Sinal Recebido (preenchido pelo SO)
 */
void trataSinalSIGCHLD_S8(int sinalRecebido) {
    so_debug("< [@param sinalRecebido:%d]", sinalRecebido);

    int pidSD =  wait (NULL);
    so_success("S8","Terminou Servidor Dedicado %d", pidSD);

    so_debug(">");
}

/**
 * @brief SD9   O novo processo Servidor Dedicado (filho) arma os sinais SIGUSR1 (ver SD18) e
 *              SIGINT (programa-o para ignorar este sinal). Em caso de erro a armar os sinais,
 *              dá so_error e termina o Servidor Dedicado. Caso contrário, dá so_success;
 *
 * @return int Sucesso (0: success, -1: error)
 */
int triggerSignals_SD9() {
    int result = -1;    // Por omissão retorna erro
    so_debug("<");

    if(signal(SIGUSR1, trataSinalSIGUSR1_SD18) == SIG_ERR) {
        so_error("SD9", "");
        return result;
    }
    if(signal(SIGINT, SIG_IGN) == SIG_ERR) {
        so_error("SD9", "");
        return result;
    }
    result = 0;
    so_success("SD9", "");

    so_debug("> [@return:%d]", result);
    return result;
}

/**
 * @brief SD10  O Servidor Dedicado deve validar, em primeiro lugar, no pedido Login recebido do
 *              Cliente (herdado do processo Servidor pai), se o campo pid_cliente > 0. Se for,
 *              dá so_success, caso contrário dá so_error e termina o Servidor Dedicado;
 *
 * @param request Pedido de Login que foi enviado pelo Cliente.
 * @return int Sucesso (0: success, -1: error)
 */
int validaPedido_SD10(Login request) {
    int result = -1;    // Por omissão retorna erro
    so_debug("< [@param request.nif:%d, request.senha:%s, request.pid_cliente:%d]", request.nif,
                                                            request.senha, request.pid_cliente);

    if(request.pid_cliente > 0){
        so_success("SD10","");
        result = 0;
    }else{
        so_error("SD10","");
    }

    so_debug("> [@return:%d]", result);
    return result;
}

/**
 * @brief SD11      Abre o ficheiro nameDB para leitura. Em caso de erro na
 *                  abertura do ficheiro, dá so_error e termina o Servidor Dedicado.
 *                  Caso contrário, dá so_success, e faz as seguintes operações:
 *        SD11.1    Inicia uma variável index_client com o índice (inteiro) do elemento Login
 *                  corrente lido do ficheiro. Para simplificar, pode considerar que este
 *                  ficheiro nunca terá nem mais nem menos elementos;
 *        SD11.2    Se já chegou ao final do ficheiro nameDB sem encontrar o
 *                  cliente, coloca index_client=-1, dá so_error, fecha o ficheiro,
 *                  e segue para o passo SD12;
 *        SD11.3    Caso contrário, lê (acesso direto) um elemento Login do ficheiro e
 *                  incrementa a variável index_client. Em caso de erro na leitura do ficheiro,
 *                  dá so_error e termina o Servidor Dedicado;
 *        SD11.4    Valida se o NIF passado no pedido do Cliente corresponde ao NIF do elemento
 *                  Login do ficheiro. Se não corresponder, então reinicia ao passo SD11.2;
 *        SD11.5    Se, pelo contrário, os NIFs correspondem, valida se a Senha passada no
 *                  pedido do Cliente bate certo com a Senha desse mesmo elemento Login do
 *                  ficheiro. Caso isso seja verdade, então dá  so_success <index_client>.
 *                  Caso contrário, dá so_error e coloca index_client=-1;
 *        SD11.6    Termina a pesquisa, e fecha o ficheiro nameDB.
 *
 * @param request O pedido do cliente
 * @param nameDB O nome da base de dados
 * @param itemDB O endereço de estrutura Login a ser preenchida nesta função com o elemento da BD
 * @return int Em caso de sucesso, retorna o índice de itemDB no ficheiro nameDB.
 *             Caso contrário retorna -1
 */
int procuraUtilizadorBD_SD11(Login request, char *nameDB, Login *itemDB) {
    int index_client = -1;    // SD11.1: Por omissão retorna erro
    so_debug("< [@param request.nif:%d, request.senha:%s, nameDB:%s, itemDB:%p]", request.nif,
                                                                    request.senha, nameDB, itemDB);

   // SD11 //
    FILE* file;
    file = fopen(nameDB,"r+");
    if(file == NULL){
        so_error("SD11", "");
    }else{
        so_success("SD11", "");

        // SD 11.1 //
        index_client=0;
    
        // SD 11.2 //
        while(TRUE){
            if(fread(itemDB,sizeof(Login), 1, file) < 1){
                index_client = -1;
                so_error("SD11.2","");
                break;
            }
            fseek(file,index_client*sizeof(Login),SEEK_SET);
            fread(itemDB,sizeof(Login), 1, file);
            if(itemDB->nif == request.nif){
                if(strcmp(itemDB->senha, request.senha) == 0){
                    so_success("SD11.5","%d",index_client);
                }else{
                    so_error("SD11.5","");
                    index_client=-1;
                }
                break;
            }
            index_client ++;
        }
        fclose(file);
    }

    so_debug("> [@return:%d, nome:%s, saldo:%d]", index_client, itemDB->nome, itemDB->saldo);
    return index_client;
}

/**
 * @brief SD12      Modifica a estrutura Login recebida no pedido do Cliente:
 *                  se index_client < 0, então preenche o campo pid_servidor_dedicado=-1,
 *                  e segue para o passo SD13. Caso contrário (index_client >= 0):
 *        SD12.1    Preenche os campos nome e saldo da estrutura Login recebida no pedido do
 *                  Cliente com os valores do item de bd_utilizadores.dat para index_client.
 *                  Preenche o campo pid_servidor_dedicado com o PID do processo Servidor
 *                  Dedicado, ficando assim a estrutura Login completamente preenchida;
 *        SD12.2    Abre o ficheiro bd_utilizadores.dat para escrita. Em caso de erro na
 *                  abertura do ficheiro, dá so_error e termina o Servidor Dedicado.
 *                  Caso contrário, dá so_success;
 *        SD12.3    Posiciona o apontador do ficheiro (fseek) para o elemento Login
 *                  correspondente a index_client, mais precisamente, para imediatamente antes
 *                  dos campos a atualizar (pid_cliente e pid_servidor_dedicado). Em caso de
 *                  erro, dá so_error e termina. Caso contrário, dá so_success;
 *        SD12.4    Escreve no ficheiro (acesso direto), na posição atual, os campos pid_cliente
 *                  e pid_servidor_dedicado atualizando assim a estrutura Login correspondente a
 *                  este Cliente na base de dados, e fecha o ficheiro. Em caso de erro, dá
 *                  so_error e termina. Caso contrário, dá so_success.
 *
 * @param request O endereço do pedido do cliente (endereço é necessário pois será alterado)
 * @param nameDB O nome da base de dados
 * @param index_client O índica na base de dados do elemento correspondente ao cliente
 * @param itemDB O elemento da BD correspondente ao cliente
 * @return int Sucesso (0: success, -1: error)
 */
int reservaUtilizadorBD_SD12(Login *request, char *nameDB, int index_client, Login itemDB) {
    int result = -1;    // Por omissão retorna erro
    so_debug("< [@param request:%p, nameDB:%s, index_client:%d, itemDB.pid_servidor_dedicado:%d]",
                                    request, nameDB, index_client, itemDB.pid_servidor_dedicado);

    // SD12 //
    if(index_client < 0){
        request->pid_servidor_dedicado=-1;
        return 0;
    }

    // SD12.1 // 
    strcpy(request->nome,itemDB.nome);
    request->saldo=itemDB.saldo;
    request->pid_servidor_dedicado = getpid();

    // SD12.2 // 
    FILE* file;
    file = fopen(nameDB,"r+");
    if(file == NULL){
        so_error("SD12.2", "");
        return -1;
    }else{
        so_success("SD12.2", "");
    

        // SD12.3 // 
        if(fseek(file,index_client*sizeof(Login),SEEK_SET) != 0){
           so_error("SD12.3", "");
            return -1;
        }
        so_success("SD12.3", "");

        Login access;

        if(fread(&access,sizeof(Login), 1, file) < 1 ){
           so_error("SD12.4", "");
           return -1;
        }
        access.pid_servidor_dedicado= request->pid_servidor_dedicado;
        access.pid_cliente= request->pid_cliente;
        fseek(file,index_client*sizeof(Login),SEEK_SET);
        if(fwrite(&access, sizeof(Login), 1, file) < 1){
            so_error("SD12.4", "");
        }else{
            so_success("SD12.4", "");
            result= 0;
        }

        // SD12.4 // 
        fclose(file);
    }

    so_debug("> [@return:%d, nome:%s, saldo:%d, pid_servidor_dedicado:%d]", result, request->nome,
                                                request->saldo, request->pid_servidor_dedicado);
    return result;
}

/**
 * @brief Constrói o nome de um FIFO baseado no prefixo (FILE_PREFIX_SRVDED ou FILE_PREFIX_CLIENT),
 *        PID (passado) e sufixo (FILE_SUFFIX_FIFO).
 *
 * @param buffer Buffer onde vai colocar o resultado
 * @param buffer_size Tamanho do buffer anterior
 * @param prefix Prefixo do nome do FIFO (deverá ser FILE_PREFIX_SRVDED ou FILE_PREFIX_CLIENT)
 * @param pid PID do processo respetivo
 * @param suffix Sufixo do nome do FIFO (deverá ser FILE_SUFFIX_FIFO)
 * @return int Sucesso (0: success, -1: error)
 */
int buildNomeFifo(char *buffer, int buffer_size, char *prefix, int pid, char *suffix) {
    int result = -1;    // Por omissão retorna erro
    so_debug("< [@param buffer:%s, buffer_size:%d, prefix:%s, pid:%d, suffix:%s]", buffer,
                                                                buffer_size, prefix, pid, suffix);

     result = snprintf(buffer, buffer_size, "%s%d%s", prefix, pid, suffix); 

    so_debug("> [@return:%d, buffer:%s]", result, buffer);
    return result;
}

/**
 * @brief SD13  Usa buildNomeFifo() para definir nameFifo como "sd-<pid_servidor_dedicado>.fifo".
 *              Cria o ficheiro com organização FIFO (named pipe) do Servidor Dedicado, de nome
 *              sd-<pid_servidor_dedicado>.fifo na diretoria local. Se houver erro na operação,
 *              dá so_error, e termina o Servidor Dedicado. Caso contrário, dá so_success;
 *
 * @param nameFifo String preenchida com o nome do FIFO (i.e., sd-<pid_servidor_dedicado>.fifo)
 * @return int Sucesso (0: success, -1: error)
 */
int createFifo_SD13(char *nameFifo) {
    int result = -1;    // Por omissão retorna erro
    so_debug("< [@param nameFifo:%s]", nameFifo);
    buildNomeFifo(nameFifo, SIZE_FILENAME, FILE_PREFIX_SRVDED, getpid(), FILE_SUFFIX_FIFO);

    if( mkfifo(nameFifo, 666) != 0 ){
        so_error("SD13", "");
    }else{
        so_success("SD13", "");
        result=0;
    }

    so_debug("> [@return:%d]", result);
    return result;
}

/**
 * @brief SD14  Usa buildNomeFifo() para definir o nome nameFifo como "cli-<pid_cliente>.fifo".
 *              Abre o FIFO do Cliente, de nome cli-<pid_cliente>.fifo na diretoria
 *              local, para escrita, escreve (acesso direto) no FIFO do Cliente a estrutura
 *              Login recebida no pedido do Cliente, e fecha o mesmo FIFO.  Em caso de erro, dá
 *              so_error, e segue para o passo SD17. Caso contrário, dá so_success.
 *
 * @param ackLogin Estrutura Login a escrever no FIFO do Cliente
 * @param nameFifo String a ser preenchida com o nome do FIFO (i.e., cli-<pid_cliente>.fifo)
 * @return int Sucesso (0: success, -1: error)
 */
int sendAckLogin_SD14(Login ackLogin, char *nameFifo) {
    int result = -1;    // Por omissão retorna erro
    so_debug("< [@param ackLogin.nome:%s, ackLogin.saldo:%d, ackLogin.pid_servidor_dedicado:%d, nameFifo:%s]",
                        ackLogin.nome, ackLogin.saldo, ackLogin.pid_servidor_dedicado, nameFifo);
    // Invoca buildNomeFifo de forma similar ao realizado em createFifo_SD13() para definir nameFifo
    buildNomeFifo(nameFifo, SIZE_FILENAME, FILE_PREFIX_CLIENT, ackLogin.pid_cliente, FILE_SUFFIX_FIFO);

    FILE *fifo = fopen(nameFifo, "w"); 
    if(fifo == NULL){
        so_error("SD14","");
        deleteFifoAndExit_SD17();
    }else{
        if (fwrite(&ackLogin, sizeof(ackLogin), 1, fifo) != 1) {
            so_error("SD14","");
            fclose(fifo);
            deleteFifoAndExit_SD17();
        }else{
            fclose(fifo);
            so_success("SD14","");
            result = 0;
        }
    }

    so_debug("> [@return:%d, nameFifo:%s]", result, nameFifo);
    return result;
}

/**
 * @brief SD15  Abre o FIFO do Servidor Dedicado, lê uma string enviada pelo Cliente, e fecha o
 *              mesmo FIFO. Em caso de erro, dá so_error, e segue para o passo SD17.
 *              Caso contrário, dá so_success <string enviada>.
 *
 * @param nameFifo o nome do FIFO do Servidor Dedicado (sd-<pid_servidor_dedicado>.fifo)
 * @return int Sucesso (0: success, -1: error)
 */
int readFimSessao_SD15(char *nameFifo) {
    int result = -1;    // Por omissão retorna erro
    so_debug("< [@param nameFifo:%s]", nameFifo);

    char string[100];
    FILE *file;
    file = fopen(nameFifo,"r");
    if(file != NULL){
            fread(string, sizeof(file), 1, file);
            fclose(file);
            so_success("SD15", "%s", string);
            result=0;
    }else{
            so_error("SD15", "");
            deleteFifoAndExit_SD17();
    }

    so_debug("> [@return:%d]", result);
    return result;
}

/**
 * @brief SD16      Modifica a estrutura Login recebida no pedido do Cliente,
 *                  por forma a terminar a sessão:
 *        SD16.1    Preenche os campos pid_cliente=-1 e pid_servidor_dedicado=-1;
 *        SD16.2    Abre o ficheiro bd_utilizadores.dat na diretoria local para escrita. Em caso
 *                  de erro na abertura do ficheiro, dá so_error, e segue para o passo SD17.
 *                  Caso contrário, dá so_success;
 *        SD16.3    Posiciona o apontador do ficheiro (fseek) para o elemento Login
 *                  correspondente a index_client, mais precisamente, para imediatamente antes
 *                  dos campos a atualizar (pid_cliente e pid_servidor_dedicado). Em caso de
 *                  erro, dá so_error, e segue para o passo SD17. Caso contrário, dá so_success;
 *        SD16.4    Escreve no ficheiro (acesso direto), na posição atual, os campos pid_cliente
 *                  e pid_servidor_dedicado atualizando assim a estrutura Login correspondente a
 *                  este Cliente na base de dados, e fecha o ficheiro. Em caso de erro, dá
 *                  so_error, caso contrário, dá so_success.
 *                  Em ambos casos, segue para o passo SD17;
 *
 * @param clientRequest O endereço do pedido do cliente (endereço é necessário pois será alterado)
 * @param nameDB O nome da base de dados
 * @param index_client O índica na base de dados do elemento correspondente ao cliente
 * @return int Sucesso (0: success, -1: error)
 */
int terminaSrvDedicado_SD16(Login *clientRequest, char *nameDB, int index_client) {
    int result = -1;    // Por omissão retorna erro
    so_debug("< [@param clientRequest:%p, nameDB:%s, index_client:%d]", clientRequest, nameDB,
                                                                                    index_client);
    //// SD16.1 ////
    clientRequest->pid_cliente = -1;
    clientRequest->pid_servidor_dedicado = -1;

    //// SD16.2 ////
    FILE *file;
    file = fopen(nameDB,"r+");
    if(file == NULL){
        so_error("SD16.2","");
        deleteFifoAndExit_SD17();
    }else{
        so_success("SD16.2","");

        //// SD16.3 ////
        if(fseek(file,index_client*sizeof(Login),SEEK_SET) != 0){
            so_error("SD16.3","");
            fclose(file);
            deleteFifoAndExit_SD17();
        }
        so_success("SD16.3","");

        //// SD16.4 ////
        Login access;
        if(fread(&access,sizeof(Login), 1, file) < 1 ){
            so_error("SD16.4", "");
            deleteFifoAndExit_SD17(); 
        }else{
            access.pid_cliente=clientRequest->pid_cliente;
            access.pid_servidor_dedicado=clientRequest->pid_servidor_dedicado;
            fseek(file,index_client*sizeof(Login),SEEK_SET);
            if(fwrite(&access, sizeof(Login), 1, file) < 1){
                so_error("SD16.4", "");
                deleteFifoAndExit_SD17();
            }else{
                so_success("SD16.4", "");
                result=0;
            }
        }
        fclose(file);
    }

    so_debug("> [@return:%d, pid_cliente:%d, pid_servidor_dedicado:%d]", result,
                                clientRequest->pid_cliente, clientRequest->pid_servidor_dedicado);
    return result;
}

/**
 * @brief SD17  Usa buildNomeFifo() para definir um nameFifo "sd-<pid_servidor_dedicado>.fifo".
 *              Remove o FIFO sd-<pid_servidor_dedicado>.fifo da diretoria local. Em caso de
 *              erro, dá so_error, caso contrário, dá so_success. Em ambos os casos, termina o
 *              processo Servidor Dedicado.
 */
void deleteFifoAndExit_SD17() {
    so_debug("<");

    char nameFifoServidorDedicado[SIZE_FILENAME];
    // Invoca buildNomeFifo de forma similar ao realizado em createFifo_SD13() para definir nameFifoServidorDedicado
    buildNomeFifo(nameFifoServidorDedicado, SIZE_FILENAME, FILE_PREFIX_SRVDED, getpid(), FILE_SUFFIX_FIFO);

    if (remove(nameFifoServidorDedicado) == 0) {
        so_success("SD17","");
    } else {
        so_error("SD17","");
    }

    so_debug(">");
    exit(0);
}

/**
 * @brief SD18  O sinal armado SIGUSR1 serve para que o Servidor Dedicado seja alertado quando o
 *              Servidor principal quer terminar. Se o Servidor Dedicado receber esse sinal,
 *              envia um sinal SIGUSR2 ao Cliente (para avisá-lo do Shutdown), dá so_success, e
 *              vai para o passo SD16 para terminar de forma correta o Servidor Dedicado.
 *
 * @param sinalRecebido nº do Sinal Recebido (preenchido pelo SO)
 */
void trataSinalSIGUSR1_SD18(int sinalRecebido) {
    so_debug("< [@param sinalRecebido:%d]", sinalRecebido);

    if(clientRequest.pid_cliente > 0){
        kill(clientRequest.pid_cliente,SIGUSR2);
        so_success("SD18","");
        terminaSrvDedicado_SD16( &clientRequest, FILE_DATABASE,index_client);
    }

    so_debug(">");
}