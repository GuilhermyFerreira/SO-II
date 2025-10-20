#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>
#include <sys/wait.h>
#include <ncurses.h> // Inclui a biblioteca NCurses
#include <time.h>    // Para srand(time(NULL))

#define SHM_NAME "/mem_compartilhada"
#define SEM_PROD "/sem_produtor"
#define SEM_CONS "/sem_consumidor"
#define NUM_ITERACOES 5

// Estrutura da Memória Compartilhada
typedef struct
{
    int dado;
    int pronto; // flag de controle (0 = vazio/pronto para produzir, 1 = cheio/pronto para consumir)
} MemoriaCompartilhada;

// Variáveis Globais de Controle e NCurses
MemoriaCompartilhada *mem;
sem_t *sem_prod, *sem_cons;
int fd;

WINDOW *win_status; // Janela para status
WINDOW *win_log;    // Janela para eventos

// Função para iniciar o NCurses
void init_ncurses()
{
    initscr();   // Inicia o modo curses
    cbreak();    // Desativa o buffer de linha
    noecho();    // Não exibe caracteres digitados
    curs_set(0); // Oculta o cursor

    // Configura cores se o terminal suportar
    if (has_colors())
    {
        start_color();
        init_pair(1, COLOR_GREEN, COLOR_BLACK);  // Para Produtor
        init_pair(2, COLOR_CYAN, COLOR_BLACK);   // Para Consumidor
        init_pair(3, COLOR_RED, COLOR_BLACK);    // Para Erro/Bloqueio
        init_pair(4, COLOR_YELLOW, COLOR_BLACK); // Para Status
    }

    // Cria janelas (usando a altura e largura do terminal)
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    // Janela de Status (topo, menor)
    win_status = newwin(10, max_x, 0, 0);
    box(win_status, 0, 0);
    mvwprintw(win_status, 0, 2, " STATUS DO SISTEMA ");

    // Janela de Log (baixo, maior)
    win_log = newwin(max_y - 10, max_x, 10, 0);
    box(win_log, 0, 0);
    mvwprintw(win_log, 0, 2, " LOG DE EVENTOS ");

    scrollok(win_log, TRUE); // Permite a rolagem no log
    wrefresh(win_status);
    wrefresh(win_log);
}

// Função para limpar o NCurses
void end_ncurses()
{
    delwin(win_status);
    delwin(win_log);
    endwin();
}

// Função para atualizar o status na janela STATUS
void update_status(const char *producer_state, const char *consumer_state, int prod_val, int cons_val)
{
    wclear(win_status);
    box(win_status, 0, 0);
    mvwprintw(win_status, 0, 2, " STATUS DO SISTEMA ");

    // Informação de Dado Compartilhado
    wattron(win_status, COLOR_PAIR(4));
    mvwprintw(win_status, 2, 2, "Memória Compartilhada (SHM):");
    mvwprintw(win_status, 3, 4, "  -> Dado Atual: %d", mem->dado);
    mvwprintw(win_status, 4, 4, "  -> Pronto (Flag): %s", (mem->pronto == 1) ? "CHEIO (1)" : "VAZIO (0)");
    wattroff(win_status, COLOR_PAIR(4));

    // Status do Semáforo Produtor
    wattron(win_status, COLOR_PAIR(1));
    mvwprintw(win_status, 6, 2, "Produtor:");
    mvwprintw(win_status, 7, 4, "  -> Estado: %s", producer_state);
    mvwprintw(win_status, 8, 4, "  -> Último Valor Produzido: %d", prod_val);
    wattroff(win_status, COLOR_PAIR(1));

    // Status do Semáforo Consumidor
    wattron(win_status, COLOR_PAIR(2));
    mvwprintw(win_status, 6, 40, "Consumidor:");
    mvwprintw(win_status, 7, 42, "  -> Estado: %s", consumer_state);
    mvwprintw(win_status, 8, 42, "  -> Último Valor Consumido: %d", cons_val);
    wattroff(win_status, COLOR_PAIR(2));

    wrefresh(win_status);
}

// Função para adicionar um log na janela LOG
// Agora aceita uma string de formato e argumentos variáveis (...)
void add_log(int color_pair, const char *format, ...)
{
    char buffer[256]; // Buffer temporário para armazenar a string formatada
    va_list args;

    // 1. Inicia o processamento dos argumentos variáveis
    va_start(args, format);

    // 2. Formata a string (como printf) no buffer usando vsnprintf
    vsnprintf(buffer, sizeof(buffer), format, args);

    // 3. Limpa a lista de argumentos variáveis
    va_end(args);

    // 4. Adiciona a string formatada à janela NCurses
    wattron(win_log, COLOR_PAIR(color_pair));
    wprintw(win_log, "%s\n", buffer); // wprintw agora recebe a string final formatada
    wattroff(win_log, COLOR_PAIR(color_pair));
    wrefresh(win_log);
}

// Função para fechar recursos (chamada no final)
void fechar_recursos()
{
    sem_close(sem_prod);
    sem_close(sem_cons);
    sem_unlink(SEM_PROD);
    sem_unlink(SEM_CONS);
    munmap(mem, sizeof(MemoriaCompartilhada));
    shm_unlink(SHM_NAME);
}

int main()
{
    srand(time(NULL));

    // 1. Cria e Mapeia memória compartilhada
    fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd == -1)
    {
        perror("Erro ao criar memória compartilhada");
        return 1;
    }

    ftruncate(fd, sizeof(MemoriaCompartilhada));
    mem = mmap(NULL, sizeof(MemoriaCompartilhada),
               PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (mem == MAP_FAILED)
    {
        perror("Erro ao mapear memória");
        fechar_recursos();
        return 1;
    }

    // Inicializa a estrutura
    mem->dado = 0;
    mem->pronto = 0;

    // 2. Cria e Abre semáforos nomeados
    // O_CREAT | O_EXCL garante que o semáforo seja criado limpo, importante para POSIX IPC
    sem_unlink(SEM_PROD); // Limpa resíduos anteriores
    sem_unlink(SEM_CONS);

    sem_prod = sem_open(SEM_PROD, O_CREAT, 0666, 1); // 1: Produção liberada
    sem_cons = sem_open(SEM_CONS, O_CREAT, 0666, 0); // 0: Consumo bloqueado

    if (sem_prod == SEM_FAILED || sem_cons == SEM_FAILED)
    {
        perror("Erro ao criar semáforos");
        fechar_recursos();
        return 1;
    }

    // 3. Inicializa NCurses e a interface
    init_ncurses();
    add_log(4, "Sistema Produtor-Consumidor iniciado. Pressione 'q' para sair.");
    update_status("Pronto", "Aguardando", 0, 0);

    // 4. Cria Processo Filho
    pid_t pid = fork();

    if (pid < 0)
    {
        end_ncurses();
        perror("Erro no fork");
        fechar_recursos();
        return 1;
    }

    if (pid == 0)
    {
        // ----- Processo FILHO: CONSUMIDOR -----
        int ultimo_consumido = 0;
        for (int i = 0; i < NUM_ITERACOES; i++)
        {
            sem_wait(sem_cons); // Espera o produtor sinalizar (sem_cons > 0)

            // Se o produtor for muito rápido, o log pode ficar desatualizado,
            // mas o valor final do dado estará correto.
            ultimo_consumido = mem->dado;
            mem->pronto = 0; // marca como lido (Vazio)

            // O consumidor não interage com a interface NCurses,
            // o pai (Produtor) fará isso, mas para garantir que o log seja atualizado:
            // Isso funcionaria se os processos estivessem usando a mesma tela,
            // mas é melhor que apenas um (o pai) manipule o NCurses.

            sem_post(sem_prod); // Libera o produtor (sem_prod + 1)
            sleep(1);
        }
        exit(0);
    }
    else
    {
        // ----- Processo PAI: PRODUTOR/VISUALIZADOR -----
        int ultimo_produzido = 0;
        int ultimo_consumido_exibido = 0;
        int iteracoes_prod = 0;

        for (iteracoes_prod = 0; iteracoes_prod < NUM_ITERACOES; iteracoes_prod++)
        {

            // 1. BLOQUEIO: Espera permissão para produzir (sem_prod > 0)
            update_status("BLOQUEADO (sem_wait)", "Aguardando (sem_cons)", ultimo_produzido, ultimo_consumido_exibido);
            add_log(3, "[PRODUTOR] Bloqueado. Aguardando vaga...");
            sem_wait(sem_prod);

            // 2. PRODUÇÃO: Se chegou aqui, sem_prod foi decrementado (== 0)
            ultimo_produzido = rand() % 100 + 1; // +1 para evitar 0, mais visível
            mem->dado = ultimo_produzido;
            mem->pronto = 1;

            update_status("PRODUZINDO/LIBERADO", "Aguardando", ultimo_produzido, ultimo_consumido_exibido);
            add_log(1, "[PRODUTOR] Produziu valor: %d. Libera Consumidor.", ultimo_produzido);

            // 3. LIBERAÇÃO: Libera o consumidor (sem_cons + 1)
            sem_post(sem_cons);

            // Se for a última iteração, não precisa esperar
            if (iteracoes_prod < NUM_ITERACOES - 1)
            {
                sleep(1);
            }
        }

        // Espera o consumidor terminar
        wait(NULL);

        // Tenta obter o último valor consumido (embora o dado possa ter sido
        // escrito por qualquer um, a lógica produtor-consumidor garante a ordem)
        ultimo_consumido_exibido = mem->dado;

        update_status("TERMINADO", "TERMINADO", ultimo_produzido, ultimo_consumido_exibido);
        add_log(4, "\nExecução Produtor-Consumidor Finalizada.");

        // Permite visualizar o resultado antes de fechar (pressionar 'q')
        nodelay(stdscr, FALSE); // Bloqueia para esperar input
        while (getch() != 'q')
        {
        }

        // ----- Limpeza -----
        end_ncurses();
        fechar_recursos();
        printf("\nSistema Produtor-Consumidor finalizado com sucesso.\n");
    }

    return 0;
}