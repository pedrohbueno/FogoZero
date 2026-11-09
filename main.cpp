/*
 * ============================================================================
 *  FogoZero - Sistema de Controle de Ocorrências do Corpo de Bombeiros
 *  Arquivo único: main.cpp
 *
 *  Compilação:  g++ -std=c++11 main.cpp -o fogozero
 *  Execução  :  ./fogozero        (Windows: fogozero.exe)
 *
 *  Estruturas de dados utilizadas:
 *   - Lista duplamente encadeada ... todas as ocorrências (ListaOcorrencias)
 *   - Quatro filas encadeadas ...... prioridades Crítica, Alta, Média e Baixa
 *   - Lista simplesmente encadeada . resultados das buscas (ListaResultado)
 *   - Pilha encadeada .............. histórico de operações
 *   - std::list .................... viaturas e bombeiros
 *
 *  Organização deste arquivo:
 *   PARTE 1 - Constantes, enums e structs
 *   PARTE 2 - Protótipos das funções
 *   PARTE 3 - Utilitários, listas, filas, pilha e classificação
 *   PARTE 4 - Viaturas, bombeiros e despacho
 *   PARTE 5 - Consultas, relatórios e histórico
 *   PARTE 6 - Persistência em arquivos CSV
 *   PARTE 7 - Menu principal (main)
 * ============================================================================
 */
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <list>
#include <vector>
#include <algorithm>
#include <ctime>
#include <cctype>
#include <cstdio>
#include <cstdlib>

using namespace std;

// ============================================================================
//  PARTE 1 - CONSTANTES, ENUMS E STRUCTS
// ============================================================================

// ----------------------------------------------------------------------------
// Constantes e enumerações
// ----------------------------------------------------------------------------
const int TOTAL_TIPOS = 10;
const int TOTAL_PRIORIDADES = 4;
const int PRIORIDADE_INDEFINIDA = 0;   // indica que a classificação deve ser manual

enum TipoOcorrencia {
    BATIDA_TRANSITO = 1,
    INCENDIO,
    PARADA_CARDIACA,
    AFOGAMENTO,
    SUFOCAMENTO,
    VAZAMENTO_GAS,
    ENCHENTE,
    ARVORE_CAIDA,
    ANIMAL_PRESO,
    ABELHAS
};

enum Prioridade {
    PRIORIDADE_CRITICA = 1,
    PRIORIDADE_ALTA = 2,
    PRIORIDADE_MEDIA = 3,
    PRIORIDADE_BAIXA = 4
};

enum StatusOcorrencia {
    AGUARDANDO = 1,
    EM_ATENDIMENTO,
    ENCERRADA
};

enum LocalIncendio {
    LOCAL_NAO_SE_APLICA = 0,
    LOCAL_PREDIO,
    LOCAL_VEGETACAO,
    LOCAL_OUTRO
};

enum SituacaoAfogamento {
    AFOGAMENTO_NAO_SE_APLICA = 0,
    VITIMA_SUBMERSA,
    VITIMA_RETIRADA_DA_AGUA
};

enum TipoOperacao {
    OP_CADASTRO_OCORRENCIA = 1,
    OP_ALTERACAO_PRIORIDADE,
    OP_DESPACHO_VIATURA,
    OP_RETORNO_VIATURA,
    OP_ENCERRAMENTO_OCORRENCIA,
    OP_CADASTRO_BOMBEIRO,
    OP_CADASTRO_VIATURA,
    OP_DESFAZER_CADASTRO,
    OP_VINCULO_BOMBEIRO,
    OP_STATUS_VIATURA
};

// Exceção lançada quando a entrada padrão termina (Ctrl+D / Ctrl+Z / arquivo).
// Permite salvar os dados antes de encerrar, em vez de entrar em laço infinito.
struct FimDeEntrada {};

// ----------------------------------------------------------------------------
// Structs de dados
// ----------------------------------------------------------------------------

// Respostas da triagem (perguntas feitas no cadastro)
struct Triagem {
    int tipo;
    bool haVitimas;
    bool pessoasPresas;
    int localIncendio;
    int situacaoAfogamento;
};

struct Ocorrencia {
    int numero;                 // número sequencial (exibido como OC-0001)
    int tipo;                   // TipoOcorrencia
    std::string bairro;
    std::string endereco;
    std::string descricao;
    bool haVitimas;
    std::string detalhes;       // respostas da triagem em texto
    int prioridade;             // 1 a 4
    bool classificacaoManual;   // true se a prioridade foi definida/alterada manualmente
    std::string dataHora;       // data e hora do registro
    int status;                 // StatusOcorrencia
    std::string viatura;        // prefixo da viatura responsável ("" se não houver)
    long sequenciaFila;         // ordem de entrada na fila (preserva FIFO ao recarregar)
};

// ----- Lista DUPLAMENTE encadeada de ocorrências -----
struct NoOcorrencia {
    Ocorrencia dados;
    NoOcorrencia* anterior;
    NoOcorrencia* proximo;
};

struct ListaOcorrencias {
    NoOcorrencia* inicio;
    NoOcorrencia* fim;
    int tamanho;
    int proximoNumero;          // próximo código automático
};

// ----- Fila (FIFO) de ponteiros para ocorrências -----
struct NoFila {
    Ocorrencia* ocorrencia;
    NoFila* proximo;
};

struct Fila {
    NoFila* inicio;
    NoFila* fim;
    int tamanho;
};

// As quatro filas: filas[0] = Crítica, filas[1] = Alta, filas[2] = Média, filas[3] = Baixa
struct FilasPrioridade {
    Fila filas[TOTAL_PRIORIDADES];
    long proximaSequencia;
};

// ----- Lista SIMPLESMENTE encadeada (resultados de buscas) -----
struct NoResultado {
    Ocorrencia* ocorrencia;
    NoResultado* proximo;
};

struct ListaResultado {
    NoResultado* inicio;
    NoResultado* fim;
    int tamanho;
};

// ----- Pilha do histórico de operações -----
struct Operacao {
    int tipo;                   // TipoOperacao
    std::string descricao;
    std::string dataHora;
    int numeroOcorrencia;       // 0 quando não se refere a uma ocorrência
};

struct NoPilha {
    Operacao dados;
    NoPilha* abaixo;
};

struct Pilha {
    NoPilha* topo;
    int tamanho;
};

const int EQUIPE_MINIMA = 2;   // bombeiros exigidos para despachar uma viatura
const int EQUIPE_MAXIMA = 6;   // capacidade máxima de uma viatura

enum StatusViatura {
    VIATURA_DISPONIVEL = 1,
    VIATURA_EM_OCORRENCIA,
    VIATURA_EM_MANUTENCAO
};

struct Viatura {
    std::string prefixo;        // identificação única (ex.: ABT-01)
    std::string tipo;           // Auto Bomba Tanque, Unidade de Resgate...
    std::string placa;
    int status;                 // StatusViatura
    int numeroOcorrencia;       // ocorrência em atendimento (0 = nenhuma)
};

struct Bombeiro {
    std::string matricula;      // identificação única
    std::string nome;
    std::string posto;          // graduação / posto
    std::string prefixoViatura; // viatura vinculada ("" = sem vínculo)
};

typedef std::list<Viatura> ListaViaturas;
typedef std::list<Bombeiro> ListaBombeiros;

// ---------------- Resultado do despacho ----------------
enum ResultadoDespacho {
    DESPACHO_REALIZADO = 1,
    DESPACHO_SEM_OCORRENCIAS,
    DESPACHO_SEM_VIATURA_DISPONIVEL,
    DESPACHO_EQUIPE_INCOMPLETA,
    DESPACHO_VIATURA_INEXISTENTE
};

struct InfoDespacho {
    int resultado;
    Ocorrencia* ocorrencia;     // ocorrência envolvida (despachada ou que continua na fila)
    Viatura* viatura;           // viatura despachada (ou a escolhida que falhou)
};

// ============================================================================
//  PARTE 2 - PROTÓTIPOS DAS FUNÇÕES
// ============================================================================

// ----------------------------------------------------------------------------
// Utilitários gerais (entrada validada, formatação, data/hora)
// ----------------------------------------------------------------------------
std::string aparar(const std::string& texto);
std::string paraMinusculas(std::string texto);
std::string paraMaiusculas(std::string texto);
std::string sanitizar(const std::string& texto);
std::size_t comprimentoVisivel(const std::string& texto);
std::string ajustar(const std::string& texto, std::size_t largura);

std::string lerLinha(const std::string& mensagem);
std::string lerTexto(const std::string& mensagem, std::size_t minimo, std::size_t maximo);
int lerInteiro(const std::string& mensagem, int minimo, int maximo);
bool lerSimNao(const std::string& mensagem);
void pausar();
void imprimirLinha(char caractere = '-', int tamanho = 70);
void imprimirTitulo(const std::string& titulo);

std::string dataHoraAtual();
std::string formatarCodigo(int numero);
int extrairNumeroCodigo(const std::string& codigo);

std::string nomeTipo(int tipo);
std::string nomePrioridade(int prioridade);
std::string nomeStatus(int status);
std::string nomeOperacao(int tipo);

// ----------------------------------------------------------------------------
// Lista duplamente encadeada
// ----------------------------------------------------------------------------
void inicializarLista(ListaOcorrencias& lista);
Ocorrencia* inserirOcorrenciaNoFim(ListaOcorrencias& lista, const Ocorrencia& ocorrencia);
Ocorrencia* buscarOcorrencia(ListaOcorrencias& lista, int numero);
bool removerOcorrencia(ListaOcorrencias& lista, int numero);
void liberarLista(ListaOcorrencias& lista);

// ----------------------------------------------------------------------------
// Filas
// ----------------------------------------------------------------------------
void inicializarFila(Fila& fila);
bool filaVazia(const Fila& fila);
void enfileirar(Fila& fila, Ocorrencia* ocorrencia);
Ocorrencia* desenfileirar(Fila& fila);
Ocorrencia* frenteDaFila(const Fila& fila);
bool removerDaFila(Fila& fila, const Ocorrencia* ocorrencia);
int posicaoNaFila(const Fila& fila, const Ocorrencia* ocorrencia);
void liberarFila(Fila& fila);

void inicializarFilas(FilasPrioridade& filas);
void inserirNaFilaDePrioridade(FilasPrioridade& filas, Ocorrencia* ocorrencia);
void restaurarNaFila(FilasPrioridade& filas, Ocorrencia* ocorrencia);
int filaMaiorPrioridadeComOcorrencias(const FilasPrioridade& filas);
int totalAguardando(const FilasPrioridade& filas);
void liberarFilas(FilasPrioridade& filas);

// ----------------------------------------------------------------------------
// Lista simplesmente encadeada de resultados
// ----------------------------------------------------------------------------
void inicializarResultado(ListaResultado& resultado);
void adicionarResultado(ListaResultado& resultado, Ocorrencia* ocorrencia);
void liberarResultado(ListaResultado& resultado);

// ----------------------------------------------------------------------------
// Pilha de histórico
// ----------------------------------------------------------------------------
void inicializarPilha(Pilha& pilha);
bool pilhaVazia(const Pilha& pilha);
void empilhar(Pilha& pilha, const Operacao& operacao);
bool desempilhar(Pilha& pilha, Operacao& operacao);
void liberarPilha(Pilha& pilha);
void registrarOperacao(Pilha& pilha, int tipo, const std::string& descricao, int numeroOcorrencia = 0);

// ----------------------------------------------------------------------------
// Regras de negócio de ocorrências
// ----------------------------------------------------------------------------
int classificarPrioridade(const Triagem& triagem, std::string& justificativa);
std::string descreverTriagem(const Triagem& triagem);
Ocorrencia* cadastrarOcorrencia(ListaOcorrencias& lista, FilasPrioridade& filas, Pilha& historico,
                                Ocorrencia dados);
bool alterarPrioridade(FilasPrioridade& filas, Pilha& historico, Ocorrencia* ocorrencia,
                       int novaPrioridade, const std::string& motivo, std::string& erro);
bool desfazerUltimoCadastro(ListaOcorrencias& lista, FilasPrioridade& filas, Pilha& historico,
                            std::string& mensagem);

// ----------------------------------------------------------------------------
// Interface (console)
// ----------------------------------------------------------------------------
void exibirCabecalhoOcorrencias();
void exibirOcorrenciaResumida(const Ocorrencia& ocorrencia);
void exibirOcorrenciaDetalhada(const Ocorrencia& ocorrencia);
Ocorrencia* solicitarOcorrencia(ListaOcorrencias& lista);
void menuRegistrarOcorrencia(ListaOcorrencias& lista, FilasPrioridade& filas, Pilha& historico);
void menuAlterarPrioridade(ListaOcorrencias& lista, FilasPrioridade& filas, Pilha& historico);

// ---------------- Consultas ----------------
std::string nomeStatusViatura(int status);
Viatura* buscarViatura(ListaViaturas& viaturas, const std::string& prefixo);
Bombeiro* buscarBombeiro(ListaBombeiros& bombeiros, const std::string& matricula);
int contarEquipe(const ListaBombeiros& bombeiros, const std::string& prefixo);
bool viaturaApta(const Viatura& viatura, const ListaBombeiros& bombeiros);

// ---------------- Operações (regras de negócio) ----------------
bool cadastrarViatura(ListaViaturas& viaturas, Pilha& historico, Viatura dados, std::string& erro);
bool cadastrarBombeiro(ListaBombeiros& bombeiros, Pilha& historico, Bombeiro dados, std::string& erro);
bool vincularBombeiro(ListaBombeiros& bombeiros, ListaViaturas& viaturas, Pilha& historico,
                      const std::string& matricula, const std::string& prefixo, std::string& erro);
bool desvincularBombeiro(ListaBombeiros& bombeiros, ListaViaturas& viaturas, Pilha& historico,
                         const std::string& matricula, std::string& erro);
bool alterarStatusViatura(ListaViaturas& viaturas, Pilha& historico, const std::string& prefixo,
                          int novoStatus, std::string& erro);

InfoDespacho despacharOcorrencia(FilasPrioridade& filas, ListaViaturas& viaturas,
                                 ListaBombeiros& bombeiros, Pilha& historico,
                                 const std::string& prefixoEscolhido = "");
bool finalizarAtendimento(ListaOcorrencias& lista, ListaViaturas& viaturas, Pilha& historico,
                          int numeroOcorrencia, std::string& erro);
bool encerrarSemAtendimento(ListaOcorrencias& lista, FilasPrioridade& filas, Pilha& historico,
                            int numeroOcorrencia, const std::string& motivo, std::string& erro);
bool registrarRetornoViatura(ListaOcorrencias& lista, ListaViaturas& viaturas, Pilha& historico,
                             const std::string& prefixo, std::string& erro);

// ---------------- Interface ----------------
void listarViaturas(const ListaViaturas& viaturas, const ListaBombeiros& bombeiros, bool apenasDisponiveis);
void exibirEquipe(const ListaBombeiros& bombeiros, const std::string& prefixo);
void menuDespachar(ListaOcorrencias& lista, FilasPrioridade& filas, ListaViaturas& viaturas,
                   ListaBombeiros& bombeiros, Pilha& historico);
void menuEncerrarOcorrencia(ListaOcorrencias& lista, FilasPrioridade& filas, ListaViaturas& viaturas,
                            Pilha& historico);
void menuViaturas(ListaOcorrencias& lista, FilasPrioridade& filas, ListaViaturas& viaturas,
                  ListaBombeiros& bombeiros, Pilha& historico);
void menuBombeiros(ListaViaturas& viaturas, ListaBombeiros& bombeiros, Pilha& historico);

// Buscas (retornam uma lista simplesmente encadeada; liberar com liberarResultado)
ListaResultado filtrarPorTipo(ListaOcorrencias& lista, int tipo);
ListaResultado filtrarPorBairro(ListaOcorrencias& lista, const std::string& bairro);
ListaResultado filtrarPorStatus(ListaOcorrencias& lista, int status);

// Consultas
void listarTodasOcorrencias(ListaOcorrencias& lista);
void listarPorStatus(ListaOcorrencias& lista, int status);
void consultarPorCodigo(ListaOcorrencias& lista, const FilasPrioridade& filas);
void buscarPorTipo(ListaOcorrencias& lista);
void buscarPorBairro(ListaOcorrencias& lista);

// Filas
void visualizarFilas(const FilasPrioridade& filas);
void consultarPosicaoNaFila(ListaOcorrencias& lista, const FilasPrioridade& filas);

// Relatórios
void relatorioPorPrioridade(const ListaOcorrencias& lista);
void relatorioPorTipo(const ListaOcorrencias& lista);
void relatorioViaturasDisponiveis(const ListaViaturas& viaturas, const ListaBombeiros& bombeiros);
void resumoGeral(const ListaOcorrencias& lista, const FilasPrioridade& filas,
                 const ListaViaturas& viaturas, const ListaBombeiros& bombeiros);

// Histórico
void exibirHistorico(const Pilha& historico, int quantidade);

// Submenus
void menuFilas(ListaOcorrencias& lista, const FilasPrioridade& filas);
void menuHistorico(ListaOcorrencias& lista, FilasPrioridade& filas, Pilha& historico);
void menuRelatorios(ListaOcorrencias& lista, const FilasPrioridade& filas,
                    const ListaViaturas& viaturas, const ListaBombeiros& bombeiros);

// Pasta onde ficam os arquivos (padrão: "dados/"). Pode ser alterada nos testes.
void definirPastaDados(const std::string& pasta);
std::string caminhoDados(const std::string& arquivo);

std::vector<std::string> dividirCSV(const std::string& linha, char separador = ';');

bool salvarOcorrencias(const ListaOcorrencias& lista);
bool salvarViaturas(const ListaViaturas& viaturas);
bool salvarBombeiros(const ListaBombeiros& bombeiros);
bool salvarTodos(const ListaOcorrencias& lista, const ListaViaturas& viaturas,
                 const ListaBombeiros& bombeiros, bool exibirMensagens);

int carregarOcorrencias(ListaOcorrencias& lista, FilasPrioridade& filas);
int carregarViaturas(ListaViaturas& viaturas);
int carregarBombeiros(ListaBombeiros& bombeiros);
void carregarTodos(ListaOcorrencias& lista, FilasPrioridade& filas, ListaViaturas& viaturas,
                   ListaBombeiros& bombeiros);

void anexarLog(const Operacao& operacao);

// ============================================================================
//  PARTE 3 - UTILITÁRIOS, LISTAS, FILAS, PILHA E CLASSIFICAÇÃO
// ============================================================================

// ============================================================================
//  UTILITÁRIOS GERAIS
// ============================================================================

// Remove espaços e quebras de linha das extremidades
string aparar(const string& texto) {
    size_t inicio = texto.find_first_not_of(" \t\r\n");
    if (inicio == string::npos) return "";
    size_t fim = texto.find_last_not_of(" \t\r\n");
    return texto.substr(inicio, fim - inicio + 1);
}

string paraMinusculas(string texto) {
    for (size_t i = 0; i < texto.size(); ++i)
        texto[i] = static_cast<char>(tolower(static_cast<unsigned char>(texto[i])));
    return texto;
}

string paraMaiusculas(string texto) {
    for (size_t i = 0; i < texto.size(); ++i)
        texto[i] = static_cast<char>(toupper(static_cast<unsigned char>(texto[i])));
    return texto;
}

// O ';' é o separador dos arquivos CSV, portanto não pode aparecer nos textos.
string sanitizar(const string& texto) {
    string resultado = texto;
    for (size_t i = 0; i < resultado.size(); ++i) {
        if (resultado[i] == ';') resultado[i] = ',';
        if (resultado[i] == '\r' || resultado[i] == '\n' || resultado[i] == '\t') resultado[i] = ' ';
    }
    return resultado;
}

// Conta caracteres visíveis em UTF-8 (letras acentuadas ocupam 2 bytes)
size_t comprimentoVisivel(const string& texto) {
    size_t total = 0;
    for (size_t i = 0; i < texto.size(); ++i)
        if ((static_cast<unsigned char>(texto[i]) & 0xC0) != 0x80) ++total;
    return total;
}

// Ajusta o texto a uma largura fixa (corta ou completa com espaços), respeitando UTF-8
string ajustar(const string& texto, size_t largura) {
    if (comprimentoVisivel(texto) <= largura)
        return texto + string(largura - comprimentoVisivel(texto), ' ');
    string resultado;
    size_t visiveis = 0;
    for (size_t i = 0; i < texto.size(); ++i) {
        bool inicioCaractere = (static_cast<unsigned char>(texto[i]) & 0xC0) != 0x80;
        if (inicioCaractere) {
            if (visiveis == largura - 1) break;
            ++visiveis;
        }
        resultado += texto[i];
    }
    return resultado + ".";
}

string lerLinha(const string& mensagem) {
    cout << mensagem;
    cout.flush();
    string linha;
    if (!getline(cin, linha)) throw FimDeEntrada();
    return aparar(sanitizar(linha));
}

string lerTexto(const string& mensagem, size_t minimo, size_t maximo) {
    while (true) {
        string texto = lerLinha(mensagem);
        size_t tamanho = comprimentoVisivel(texto);
        if (tamanho == 0 && minimo > 0) {
            cout << "  [ERRO] Este campo é obrigatório.\n";
        } else if (tamanho < minimo) {
            cout << "  [ERRO] Informe pelo menos " << minimo << " caracteres.\n";
        } else if (tamanho > maximo) {
            cout << "  [ERRO] Informe no máximo " << maximo << " caracteres.\n";
        } else {
            return texto;
        }
    }
}

int lerInteiro(const string& mensagem, int minimo, int maximo) {
    while (true) {
        string texto = lerLinha(mensagem);
        bool valido = !texto.empty() && texto.size() <= 9;
        size_t i = (valido && (texto[0] == '-' || texto[0] == '+')) ? 1 : 0;
        if (i >= texto.size()) valido = false;
        for (; valido && i < texto.size(); ++i)
            if (!isdigit(static_cast<unsigned char>(texto[i]))) valido = false;
        if (valido) {
            int valor = atoi(texto.c_str());
            if (valor >= minimo && valor <= maximo) return valor;
        }
        cout << "  [ERRO] Digite um número inteiro entre " << minimo << " e " << maximo << ".\n";
    }
}

bool lerSimNao(const string& mensagem) {
    while (true) {
        string resposta = paraMinusculas(lerLinha(mensagem + " (S/N): "));
        if (resposta == "s" || resposta == "sim") return true;
        if (resposta == "n" || resposta == "nao" || resposta == "não") return false;
        cout << "  [ERRO] Responda apenas S (sim) ou N (não).\n";
    }
}

void pausar() {
    lerLinha("\nPressione ENTER para continuar...");
}

void imprimirLinha(char caractere, int tamanho) {
    cout << string(tamanho, caractere) << "\n";
}

void imprimirTitulo(const string& titulo) {
    cout << "\n";
    imprimirLinha('=');
    cout << "  " << titulo << "\n";
    imprimirLinha('=');
}

string dataHoraAtual() {
    time_t agora = time(nullptr);
    tm* local = localtime(&agora);
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", local);
    return buffer;
}

string formatarCodigo(int numero) {
    ostringstream saida;
    saida << "OC-" << setw(4) << setfill('0') << numero;
    return saida.str();
}

// Aceita "OC-0007", "oc7", "0007" ou "7". Retorna -1 se inválido.
int extrairNumeroCodigo(const string& codigo) {
    string texto = paraMaiusculas(aparar(codigo));
    if (texto.compare(0, 3, "OC-") == 0) texto = texto.substr(3);
    else if (texto.compare(0, 2, "OC") == 0) texto = texto.substr(2);
    if (texto.empty() || texto.size() > 8) return -1;
    for (size_t i = 0; i < texto.size(); ++i)
        if (!isdigit(static_cast<unsigned char>(texto[i]))) return -1;
    int numero = atoi(texto.c_str());
    return numero > 0 ? numero : -1;
}

string nomeTipo(int tipo) {
    static const char* NOMES[TOTAL_TIPOS] = {
        "Batida de trânsito", "Incêndio", "Parada cardíaca", "Afogamento", "Sufocamento",
        "Vazamento de gás", "Enchente", "Árvore caída", "Animal preso", "Abelhas"
    };
    if (tipo < 1 || tipo > TOTAL_TIPOS) return "Desconhecido";
    return NOMES[tipo - 1];
}

string nomePrioridade(int prioridade) {
    switch (prioridade) {
        case PRIORIDADE_CRITICA: return "CRÍTICA";
        case PRIORIDADE_ALTA:    return "ALTA";
        case PRIORIDADE_MEDIA:   return "MÉDIA";
        case PRIORIDADE_BAIXA:   return "BAIXA";
        default:                 return "INDEFINIDA";
    }
}

string nomeStatus(int status) {
    switch (status) {
        case AGUARDANDO:     return "Aguardando";
        case EM_ATENDIMENTO: return "Em atendimento";
        case ENCERRADA:      return "Encerrada";
        default:             return "Desconhecido";
    }
}

string nomeOperacao(int tipo) {
    switch (tipo) {
        case OP_CADASTRO_OCORRENCIA:     return "CADASTRO DE OCORRÊNCIA";
        case OP_ALTERACAO_PRIORIDADE:    return "ALTERAÇÃO DE PRIORIDADE";
        case OP_DESPACHO_VIATURA:        return "DESPACHO DE VIATURA";
        case OP_RETORNO_VIATURA:         return "RETORNO DE VIATURA";
        case OP_ENCERRAMENTO_OCORRENCIA: return "ENCERRAMENTO DE OCORRÊNCIA";
        case OP_CADASTRO_BOMBEIRO:       return "CADASTRO DE BOMBEIRO";
        case OP_CADASTRO_VIATURA:        return "CADASTRO DE VIATURA";
        case OP_DESFAZER_CADASTRO:       return "DESFAZER CADASTRO";
        case OP_VINCULO_BOMBEIRO:        return "VÍNCULO DE BOMBEIRO";
        case OP_STATUS_VIATURA:          return "STATUS DE VIATURA";
        default:                         return "OPERAÇÃO";
    }
}

// ============================================================================
//  LISTA DUPLAMENTE ENCADEADA DE OCORRÊNCIAS
// ============================================================================

void inicializarLista(ListaOcorrencias& lista) {
    lista.inicio = nullptr;
    lista.fim = nullptr;
    lista.tamanho = 0;
    lista.proximoNumero = 1;
}

// Insere no fim (ordem de chegada) e devolve o endereço estável dos dados
Ocorrencia* inserirOcorrenciaNoFim(ListaOcorrencias& lista, const Ocorrencia& ocorrencia) {
    NoOcorrencia* novo = new NoOcorrencia;
    novo->dados = ocorrencia;
    novo->anterior = lista.fim;
    novo->proximo = nullptr;

    if (lista.fim != nullptr) lista.fim->proximo = novo;
    else lista.inicio = novo;
    lista.fim = novo;
    lista.tamanho++;

    if (ocorrencia.numero >= lista.proximoNumero) lista.proximoNumero = ocorrencia.numero + 1;
    return &novo->dados;
}

Ocorrencia* buscarOcorrencia(ListaOcorrencias& lista, int numero) {
    for (NoOcorrencia* atual = lista.inicio; atual != nullptr; atual = atual->proximo)
        if (atual->dados.numero == numero) return &atual->dados;
    return nullptr;
}

// Remove um nó em qualquer posição usando os ponteiros anterior/próximo
bool removerOcorrencia(ListaOcorrencias& lista, int numero) {
    NoOcorrencia* atual = lista.inicio;
    while (atual != nullptr && atual->dados.numero != numero) atual = atual->proximo;
    if (atual == nullptr) return false;

    if (atual->anterior != nullptr) atual->anterior->proximo = atual->proximo;
    else lista.inicio = atual->proximo;

    if (atual->proximo != nullptr) atual->proximo->anterior = atual->anterior;
    else lista.fim = atual->anterior;

    delete atual;
    lista.tamanho--;
    return true;
}

void liberarLista(ListaOcorrencias& lista) {
    NoOcorrencia* atual = lista.inicio;
    while (atual != nullptr) {
        NoOcorrencia* proximo = atual->proximo;
        delete atual;
        atual = proximo;
    }
    inicializarLista(lista);
}

// ============================================================================
//  FILAS (FIFO) COM PONTEIROS
// ============================================================================

void inicializarFila(Fila& fila) {
    fila.inicio = nullptr;
    fila.fim = nullptr;
    fila.tamanho = 0;
}

bool filaVazia(const Fila& fila) {
    return fila.inicio == nullptr;
}

void enfileirar(Fila& fila, Ocorrencia* ocorrencia) {
    NoFila* novo = new NoFila;
    novo->ocorrencia = ocorrencia;
    novo->proximo = nullptr;
    if (fila.fim != nullptr) fila.fim->proximo = novo;
    else fila.inicio = novo;
    fila.fim = novo;
    fila.tamanho++;
}

Ocorrencia* desenfileirar(Fila& fila) {
    if (filaVazia(fila)) return nullptr;
    NoFila* removido = fila.inicio;
    Ocorrencia* ocorrencia = removido->ocorrencia;
    fila.inicio = removido->proximo;
    if (fila.inicio == nullptr) fila.fim = nullptr;
    delete removido;
    fila.tamanho--;
    return ocorrencia;
}

Ocorrencia* frenteDaFila(const Fila& fila) {
    return filaVazia(fila) ? nullptr : fila.inicio->ocorrencia;
}

// Remoção de um elemento específico (usada na alteração de prioridade e no "desfazer")
bool removerDaFila(Fila& fila, const Ocorrencia* ocorrencia) {
    NoFila* anterior = nullptr;
    NoFila* atual = fila.inicio;
    while (atual != nullptr && atual->ocorrencia != ocorrencia) {
        anterior = atual;
        atual = atual->proximo;
    }
    if (atual == nullptr) return false;

    if (anterior != nullptr) anterior->proximo = atual->proximo;
    else fila.inicio = atual->proximo;
    if (fila.fim == atual) fila.fim = anterior;

    delete atual;
    fila.tamanho--;
    return true;
}

// Posição 1 = primeira da fila; 0 = não encontrada
int posicaoNaFila(const Fila& fila, const Ocorrencia* ocorrencia) {
    int posicao = 1;
    for (NoFila* atual = fila.inicio; atual != nullptr; atual = atual->proximo, ++posicao)
        if (atual->ocorrencia == ocorrencia) return posicao;
    return 0;
}

void liberarFila(Fila& fila) {
    while (!filaVazia(fila)) desenfileirar(fila);
}

void inicializarFilas(FilasPrioridade& filas) {
    for (int i = 0; i < TOTAL_PRIORIDADES; ++i) inicializarFila(filas.filas[i]);
    filas.proximaSequencia = 1;
}

// Insere a ocorrência no FIM da fila da sua prioridade (nova chegada)
void inserirNaFilaDePrioridade(FilasPrioridade& filas, Ocorrencia* ocorrencia) {
    ocorrencia->sequenciaFila = filas.proximaSequencia++;
    enfileirar(filas.filas[ocorrencia->prioridade - 1], ocorrencia);
}

// Recoloca na fila mantendo a sequência original (usado ao carregar do CSV)
void restaurarNaFila(FilasPrioridade& filas, Ocorrencia* ocorrencia) {
    enfileirar(filas.filas[ocorrencia->prioridade - 1], ocorrencia);
    if (ocorrencia->sequenciaFila >= filas.proximaSequencia)
        filas.proximaSequencia = ocorrencia->sequenciaFila + 1;
}

// Retorna 1..4 (fila de maior prioridade não vazia) ou 0 se todas vazias
int filaMaiorPrioridadeComOcorrencias(const FilasPrioridade& filas) {
    for (int i = 0; i < TOTAL_PRIORIDADES; ++i)
        if (!filaVazia(filas.filas[i])) return i + 1;
    return 0;
}

int totalAguardando(const FilasPrioridade& filas) {
    int total = 0;
    for (int i = 0; i < TOTAL_PRIORIDADES; ++i) total += filas.filas[i].tamanho;
    return total;
}

void liberarFilas(FilasPrioridade& filas) {
    for (int i = 0; i < TOTAL_PRIORIDADES; ++i) liberarFila(filas.filas[i]);
}

// ============================================================================
//  LISTA SIMPLESMENTE ENCADEADA DE RESULTADOS
// ============================================================================

void inicializarResultado(ListaResultado& resultado) {
    resultado.inicio = nullptr;
    resultado.fim = nullptr;
    resultado.tamanho = 0;
}

void adicionarResultado(ListaResultado& resultado, Ocorrencia* ocorrencia) {
    NoResultado* novo = new NoResultado;
    novo->ocorrencia = ocorrencia;
    novo->proximo = nullptr;
    if (resultado.fim != nullptr) resultado.fim->proximo = novo;
    else resultado.inicio = novo;
    resultado.fim = novo;
    resultado.tamanho++;
}

void liberarResultado(ListaResultado& resultado) {
    NoResultado* atual = resultado.inicio;
    while (atual != nullptr) {
        NoResultado* proximo = atual->proximo;
        delete atual;
        atual = proximo;
    }
    inicializarResultado(resultado);
}

// ============================================================================
//  PILHA DE HISTÓRICO
// ============================================================================

void inicializarPilha(Pilha& pilha) {
    pilha.topo = nullptr;
    pilha.tamanho = 0;
}

bool pilhaVazia(const Pilha& pilha) {
    return pilha.topo == nullptr;
}

void empilhar(Pilha& pilha, const Operacao& operacao) {
    NoPilha* novo = new NoPilha;
    novo->dados = operacao;
    novo->abaixo = pilha.topo;
    pilha.topo = novo;
    pilha.tamanho++;
}

bool desempilhar(Pilha& pilha, Operacao& operacao) {
    if (pilhaVazia(pilha)) return false;
    NoPilha* removido = pilha.topo;
    operacao = removido->dados;
    pilha.topo = removido->abaixo;
    delete removido;
    pilha.tamanho--;
    return true;
}

void liberarPilha(Pilha& pilha) {
    Operacao descartada;
    while (desempilhar(pilha, descartada)) {}
}

// Empilha a operação e também grava no arquivo de log permanente
void registrarOperacao(Pilha& pilha, int tipo, const string& descricao, int numeroOcorrencia) {
    Operacao operacao;
    operacao.tipo = tipo;
    operacao.descricao = descricao;
    operacao.dataHora = dataHoraAtual();
    operacao.numeroOcorrencia = numeroOcorrencia;
    empilhar(pilha, operacao);
    anexarLog(operacao);
}

// ============================================================================
//  CLASSIFICAÇÃO AUTOMÁTICA DE PRIORIDADE
//  Tabela oficial:
//   1 Crítica: vítima presa em incêndio, afogamento, parada cardíaca, batida com vítima
//   2 Alta   : incêndio em prédio sem vítima, vazamento de gás
//   3 Média  : fogo em vegetação, enchente sem vítima
//   4 Baixa  : árvore caída, animal preso, abelhas, batida sem vítima
//  Situações fora da tabela retornam PRIORIDADE_INDEFINIDA (classificação manual).
// ============================================================================

// Para tipos da tabela que não mencionam vítimas (gás, árvore, animal, abelhas),
// a presença de vítimas torna o caso não previsto: exige classificação manual.
static int classificarSemVitima(const Triagem& triagem, int prioridade, const string& descricao,
                                string& justificativa) {
    if (triagem.haVitimas) {
        justificativa = descricao + " com vítima não está prevista na tabela de classificação.";
        return PRIORIDADE_INDEFINIDA;
    }
    justificativa = descricao + " sem vítima.";
    return prioridade;
}

int classificarPrioridade(const Triagem& triagem, string& justificativa) {
    switch (triagem.tipo) {
        case BATIDA_TRANSITO:
            if (triagem.haVitimas) {
                justificativa = "Batida com vítima (regra adotada: sempre CRÍTICA).";
                return PRIORIDADE_CRITICA;
            }
            justificativa = "Batida sem vítima.";
            return PRIORIDADE_BAIXA;

        case INCENDIO:
            if (triagem.haVitimas && triagem.pessoasPresas) {
                justificativa = "Vítima presa em incêndio.";
                return PRIORIDADE_CRITICA;
            }
            if (triagem.haVitimas) {
                justificativa = "Incêndio com vítima não presa não está previsto na tabela.";
                return PRIORIDADE_INDEFINIDA;
            }
            if (triagem.localIncendio == LOCAL_PREDIO) {
                justificativa = "Incêndio em prédio sem vítima.";
                return PRIORIDADE_ALTA;
            }
            if (triagem.localIncendio == LOCAL_VEGETACAO) {
                justificativa = "Fogo em vegetação sem vítima.";
                return PRIORIDADE_MEDIA;
            }
            justificativa = "Incêndio em local que não é prédio nem vegetação não está previsto na tabela.";
            return PRIORIDADE_INDEFINIDA;

        case PARADA_CARDIACA:
            if (triagem.haVitimas) {
                justificativa = "Parada cardíaca.";
                return PRIORIDADE_CRITICA;
            }
            justificativa = "Parada cardíaca não confirmada: informação insuficiente.";
            return PRIORIDADE_INDEFINIDA;

        case AFOGAMENTO:
            if (triagem.haVitimas) {
                justificativa = "Afogamento com vítima.";
                return PRIORIDADE_CRITICA;
            }
            justificativa = "Afogamento sem vítima confirmada: informação insuficiente.";
            return PRIORIDADE_INDEFINIDA;

        case SUFOCAMENTO:
            justificativa = "Sufocamento não consta na tabela de classificação.";
            return PRIORIDADE_INDEFINIDA;

        case VAZAMENTO_GAS:
            return classificarSemVitima(triagem, PRIORIDADE_ALTA, "Vazamento de gás", justificativa);

        case ENCHENTE:
            return classificarSemVitima(triagem, PRIORIDADE_MEDIA, "Enchente", justificativa);

        case ARVORE_CAIDA:
            return classificarSemVitima(triagem, PRIORIDADE_BAIXA, "Árvore caída", justificativa);

        case ANIMAL_PRESO:
            return classificarSemVitima(triagem, PRIORIDADE_BAIXA, "Animal preso", justificativa);

        case ABELHAS:
            return classificarSemVitima(triagem, PRIORIDADE_BAIXA, "Ocorrência com abelhas", justificativa);
    }
    justificativa = "Tipo de ocorrência desconhecido.";
    return PRIORIDADE_INDEFINIDA;
}

static string simNao(bool valor) { return valor ? "Sim" : "Não"; }

// Gera o texto das respostas da triagem (armazenado no campo "detalhes")
string descreverTriagem(const Triagem& triagem) {
    string texto;
    if (triagem.tipo == INCENDIO) {
        texto += "Local: ";
        texto += (triagem.localIncendio == LOCAL_PREDIO) ? "Prédio"
               : (triagem.localIncendio == LOCAL_VEGETACAO) ? "Vegetação" : "Outro";
        texto += " | ";
    }
    if (triagem.tipo == PARADA_CARDIACA)
        texto += "Vítima em parada cardíaca: " + simNao(triagem.haVitimas);
    else if (triagem.tipo == SUFOCAMENTO)
        texto += "Vítima com dificuldade respiratória/obstrução: " + simNao(triagem.haVitimas);
    else
        texto += "Vítimas: " + simNao(triagem.haVitimas);

    if (triagem.haVitimas && triagem.tipo == BATIDA_TRANSITO)
        texto += " | Presos nas ferragens: " + simNao(triagem.pessoasPresas);
    if (triagem.haVitimas && triagem.tipo == INCENDIO)
        texto += " | Pessoas presas no local: " + simNao(triagem.pessoasPresas);
    if (triagem.haVitimas && triagem.tipo == AFOGAMENTO)
        texto += string(" | Situação: ") +
                 (triagem.situacaoAfogamento == VITIMA_SUBMERSA ? "Submersa" : "Retirada da água");
    return texto;
}

// ============================================================================
//  OPERAÇÕES DE NEGÓCIO
// ============================================================================

// Atribui código, data/hora e status; insere na lista e na fila; registra no histórico.
Ocorrencia* cadastrarOcorrencia(ListaOcorrencias& lista, FilasPrioridade& filas, Pilha& historico,
                                Ocorrencia dados) {
    if (dados.tipo < 1 || dados.tipo > TOTAL_TIPOS) return nullptr;
    if (dados.prioridade < PRIORIDADE_CRITICA || dados.prioridade > PRIORIDADE_BAIXA) return nullptr;
    if (dados.bairro.empty() || dados.endereco.empty() || dados.descricao.empty()) return nullptr;

    dados.numero = lista.proximoNumero;
    dados.dataHora = dataHoraAtual();
    dados.status = AGUARDANDO;
    dados.viatura = "";

    Ocorrencia* nova = inserirOcorrenciaNoFim(lista, dados);
    inserirNaFilaDePrioridade(filas, nova);
    registrarOperacao(historico, OP_CADASTRO_OCORRENCIA,
                      "Cadastro de " + formatarCodigo(nova->numero) + " - " + nomeTipo(nova->tipo) +
                      " (" + nomePrioridade(nova->prioridade) + ") - " + nova->bairro,
                      nova->numero);
    return nova;
}

// Move a ocorrência para o fim da fila da nova prioridade
bool alterarPrioridade(FilasPrioridade& filas, Pilha& historico, Ocorrencia* ocorrencia,
                       int novaPrioridade, const string& motivo, string& erro) {
    if (ocorrencia == nullptr) { erro = "Ocorrência inexistente."; return false; }
    if (ocorrencia->status != AGUARDANDO) {
        erro = "Só é possível alterar a prioridade de ocorrências aguardando despacho.";
        return false;
    }
    if (novaPrioridade < PRIORIDADE_CRITICA || novaPrioridade > PRIORIDADE_BAIXA) {
        erro = "Prioridade inválida.";
        return false;
    }
    if (novaPrioridade == ocorrencia->prioridade) {
        erro = "A ocorrência já possui essa prioridade.";
        return false;
    }

    int antiga = ocorrencia->prioridade;
    removerDaFila(filas.filas[antiga - 1], ocorrencia);
    ocorrencia->prioridade = novaPrioridade;
    ocorrencia->classificacaoManual = true;
    inserirNaFilaDePrioridade(filas, ocorrencia);

    registrarOperacao(historico, OP_ALTERACAO_PRIORIDADE,
                      formatarCodigo(ocorrencia->numero) + ": " + nomePrioridade(antiga) + " -> " +
                      nomePrioridade(novaPrioridade) + ". Motivo: " + motivo,
                      ocorrencia->numero);
    return true;
}

/*
 * Desfaz o cadastro de ocorrência mais recente desta sessão.
 * Usa uma PILHA AUXILIAR: desempilha as operações até encontrar o último
 * cadastro e depois devolve as demais na ordem original.
 * Só é seguro desfazer se a ocorrência ainda estiver AGUARDANDO (não foi
 * despachada nem encerrada).
 */
bool desfazerUltimoCadastro(ListaOcorrencias& lista, FilasPrioridade& filas, Pilha& historico,
                            string& mensagem) {
    Pilha auxiliar;
    inicializarPilha(auxiliar);
    Operacao operacao, alvo;
    bool encontrou = false;

    while (desempilhar(historico, operacao)) {
        if (operacao.tipo == OP_CADASTRO_OCORRENCIA) {
            alvo = operacao;
            encontrou = true;
            break;
        }
        empilhar(auxiliar, operacao);
    }

    bool sucesso = false;
    if (!encontrou) {
        mensagem = "Não há cadastro de ocorrência nesta sessão que possa ser desfeito.";
    } else {
        Ocorrencia* ocorrencia = buscarOcorrencia(lista, alvo.numeroOcorrencia);
        string codigo = formatarCodigo(alvo.numeroOcorrencia);
        if (ocorrencia == nullptr) {
            mensagem = "A ocorrência " + codigo + " não existe mais.";
        } else if (ocorrencia->status != AGUARDANDO) {
            mensagem = "Não é seguro desfazer: " + codigo + " já está " +
                       paraMinusculas(nomeStatus(ocorrencia->status)) + ".";
        } else {
            removerDaFila(filas.filas[ocorrencia->prioridade - 1], ocorrencia);
            int numero = ocorrencia->numero;
            removerOcorrencia(lista, numero);
            // Se era o último código gerado, ele é liberado para reutilização
            if (numero == lista.proximoNumero - 1) lista.proximoNumero = numero;
            mensagem = "Cadastro da ocorrência " + codigo + " desfeito com sucesso.";
            sucesso = true;
        }
        if (!sucesso) empilhar(historico, alvo);   // devolve a operação à pilha
    }

    // Restaura as operações retiradas, mantendo a ordem original
    while (desempilhar(auxiliar, operacao)) empilhar(historico, operacao);

    if (sucesso)
        registrarOperacao(historico, OP_DESFAZER_CADASTRO,
                          "Cadastro de " + formatarCodigo(alvo.numeroOcorrencia) + " desfeito",
                          alvo.numeroOcorrencia);
    return sucesso;
}

// ============================================================================
//  INTERFACE DE CONSOLE
// ============================================================================

void exibirCabecalhoOcorrencias() {
    cout << ajustar("CÓDIGO", 8) << " " << ajustar("TIPO", 19) << " " << ajustar("BAIRRO", 15) << " "
         << ajustar("PRIOR.", 10) << " " << ajustar("STATUS", 14) << " " << ajustar("VIATURA", 9) << " "
         << "REGISTRO\n";
    imprimirLinha('-', 98);
}

void exibirOcorrenciaResumida(const Ocorrencia& oc) {
    cout << ajustar(formatarCodigo(oc.numero), 8) << " " << ajustar(nomeTipo(oc.tipo), 19) << " "
         << ajustar(oc.bairro, 15) << " " << ajustar(nomePrioridade(oc.prioridade), 10) << " "
         << ajustar(nomeStatus(oc.status), 14) << " "
         << ajustar(oc.viatura.empty() ? "-" : oc.viatura, 9) << " " << oc.dataHora.substr(0, 16)
         << "\n";
}

void exibirOcorrenciaDetalhada(const Ocorrencia& oc) {
    imprimirLinha('-');
    cout << "  Código ......: " << formatarCodigo(oc.numero) << "\n"
         << "  Tipo ........: " << nomeTipo(oc.tipo) << "\n"
         << "  Bairro ......: " << oc.bairro << "\n"
         << "  Endereço ....: " << oc.endereco << "\n"
         << "  Descrição ...: " << oc.descricao << "\n"
         << "  Vítimas .....: " << (oc.haVitimas ? "Sim" : "Não") << "\n"
         << "  Triagem .....: " << oc.detalhes << "\n"
         << "  Prioridade ..: " << oc.prioridade << " - " << nomePrioridade(oc.prioridade)
         << (oc.classificacaoManual ? " (definida manualmente)" : " (automática)") << "\n"
         << "  Registro ....: " << oc.dataHora << "\n"
         << "  Status ......: " << nomeStatus(oc.status) << "\n"
         << "  Viatura .....: " << (oc.viatura.empty() ? "Nenhuma" : oc.viatura) << "\n";
    imprimirLinha('-');
}

// Pede um código e devolve a ocorrência (ou nullptr com mensagem de erro)
Ocorrencia* solicitarOcorrencia(ListaOcorrencias& lista) {
    string codigo = lerLinha("Informe o código da ocorrência (ex.: OC-0001): ");
    int numero = extrairNumeroCodigo(codigo);
    if (numero < 0) {
        cout << "  [ERRO] Código inválido.\n";
        return nullptr;
    }
    Ocorrencia* ocorrencia = buscarOcorrencia(lista, numero);
    if (ocorrencia == nullptr) cout << "  [ERRO] Ocorrência " << formatarCodigo(numero) << " não encontrada.\n";
    return ocorrencia;
}

static void exibirTabelaPrioridades() {
    cout << "  1 - CRÍTICA: vítima presa em incêndio, afogamento, parada cardíaca, batida com vítima\n"
         << "  2 - ALTA   : incêndio em prédio sem vítima, vazamento de gás\n"
         << "  3 - MÉDIA  : fogo em vegetação, enchente sem vítima\n"
         << "  4 - BAIXA  : árvore caída, animal preso, abelhas, batida sem vítima\n";
}

// Faz as perguntas específicas de cada tipo de ocorrência
static Triagem realizarTriagem(int tipo) {
    Triagem t;
    t.tipo = tipo;
    t.haVitimas = false;
    t.pessoasPresas = false;
    t.localIncendio = LOCAL_NAO_SE_APLICA;
    t.situacaoAfogamento = AFOGAMENTO_NAO_SE_APLICA;

    cout << "\n--- Triagem: " << nomeTipo(tipo) << " ---\n";
    switch (tipo) {
        case BATIDA_TRANSITO:
            t.haVitimas = lerSimNao("Existem vítimas?");
            if (t.haVitimas) t.pessoasPresas = lerSimNao("Há pessoas presas nas ferragens?");
            break;
        case INCENDIO:
            cout << "O incêndio ocorre em:\n  1 - Prédio / edificação\n  2 - Vegetação\n  3 - Outro local\n";
            t.localIncendio = lerInteiro("Opção: ", 1, 3);
            t.haVitimas = lerSimNao("Existem vítimas?");
            if (t.haVitimas) t.pessoasPresas = lerSimNao("Há pessoas presas no local?");
            break;
        case PARADA_CARDIACA:
            t.haVitimas = lerSimNao("Existe uma vítima em parada cardíaca?");
            break;
        case AFOGAMENTO:
            t.haVitimas = lerSimNao("Existe vítima?");
            if (t.haVitimas) {
                cout << "A vítima:\n  1 - Está submersa\n  2 - Já foi retirada da água\n";
                t.situacaoAfogamento = lerInteiro("Opção: ", 1, 2);
            }
            break;
        case SUFOCAMENTO:
            t.haVitimas = lerSimNao("Existe uma vítima com dificuldade respiratória ou obstrução das vias aéreas?");
            break;
        default:
            t.haVitimas = lerSimNao("Existem vítimas?");
            break;
    }
    return t;
}

// Usada quando a tabela não prevê a situação: o operador decide a prioridade
static int classificacaoManual(const string& motivo) {
    cout << "\n  [ATENÇÃO] " << motivo << "\n"
         << "  O sistema não atribui prioridade a situações fora da tabela.\n"
         << "  Classifique manualmente com base na tabela abaixo:\n\n";
    exibirTabelaPrioridades();
    return lerInteiro("\nPrioridade definida pelo operador (1 a 4): ", 1, 4);
}

void menuRegistrarOcorrencia(ListaOcorrencias& lista, FilasPrioridade& filas, Pilha& historico) {
    imprimirTitulo("REGISTRAR OCORRÊNCIA");
    for (int i = 1; i <= TOTAL_TIPOS; ++i)
        cout << "  " << setw(2) << i << " - " << nomeTipo(i) << "\n";
    cout << "   0 - Cancelar\n";
    int tipo = lerInteiro("Tipo da ocorrência: ", 0, TOTAL_TIPOS);
    if (tipo == 0) { cout << "Cadastro cancelado.\n"; return; }

    Triagem triagem = realizarTriagem(tipo);

    Ocorrencia dados;
    dados.tipo = tipo;
    dados.haVitimas = triagem.haVitimas;
    dados.detalhes = descreverTriagem(triagem);
    dados.classificacaoManual = false;
    dados.sequenciaFila = 0;

    string justificativa;
    dados.prioridade = classificarPrioridade(triagem, justificativa);
    if (dados.prioridade == PRIORIDADE_INDEFINIDA) {
        dados.prioridade = classificacaoManual(justificativa);
        dados.classificacaoManual = true;
        dados.detalhes += " | Classificação manual: " + justificativa;
    } else {
        cout << "\n  >> Prioridade identificada: " << nomePrioridade(dados.prioridade)
             << " (" << dados.prioridade << ") - " << justificativa << "\n";
    }

    cout << "\n--- Localização e descrição ---\n";
    dados.bairro = lerTexto("Bairro: ", 2, 40);
    dados.endereco = lerTexto("Endereço: ", 3, 80);
    dados.descricao = lerTexto("Descrição da situação: ", 3, 150);

    // Validação final: o operador revisa os dados antes de concluir
    cout << "\n--- Confirme os dados ---\n"
         << "  Tipo .......: " << nomeTipo(dados.tipo) << "\n"
         << "  Triagem ....: " << dados.detalhes << "\n"
         << "  Prioridade .: " << nomePrioridade(dados.prioridade) << " (" << dados.prioridade << ")\n"
         << "  Bairro .....: " << dados.bairro << "\n"
         << "  Endereço ...: " << dados.endereco << "\n"
         << "  Descrição ..: " << dados.descricao << "\n";
    if (!lerSimNao("Confirmar cadastro?")) {
        cout << "Cadastro cancelado. Nenhum dado foi gravado.\n";
        return;
    }

    Ocorrencia* nova = cadastrarOcorrencia(lista, filas, historico, dados);
    if (nova == nullptr) {
        cout << "  [ERRO] Dados inválidos. Ocorrência não cadastrada.\n";
        return;
    }
    cout << "\n  [OK] Ocorrência " << formatarCodigo(nova->numero) << " registrada em "
         << nova->dataHora << ".\n"
         << "       Inserida na fila " << nova->prioridade << " (" << nomePrioridade(nova->prioridade)
         << "), posição " << posicaoNaFila(filas.filas[nova->prioridade - 1], nova) << ".\n";
}

void menuAlterarPrioridade(ListaOcorrencias& lista, FilasPrioridade& filas, Pilha& historico) {
    imprimirTitulo("ALTERAR PRIORIDADE");
    Ocorrencia* ocorrencia = solicitarOcorrencia(lista);
    if (ocorrencia == nullptr) return;
    exibirOcorrenciaDetalhada(*ocorrencia);
    if (ocorrencia->status != AGUARDANDO) {
        cout << "  [ERRO] Só é possível alterar a prioridade de ocorrências aguardando despacho.\n";
        return;
    }
    exibirTabelaPrioridades();
    int nova = lerInteiro("Nova prioridade (1 a 4, 0 para cancelar): ", 0, 4);
    if (nova == 0) { cout << "Operação cancelada.\n"; return; }
    string motivo = lerTexto("Motivo da alteração: ", 3, 100);

    string erro;
    if (alterarPrioridade(filas, historico, ocorrencia, nova, motivo, erro))
        cout << "  [OK] Prioridade alterada para " << nomePrioridade(nova)
             << ". A ocorrência foi para o fim da fila " << nova << ".\n";
    else
        cout << "  [ERRO] " << erro << "\n";
}

// ============================================================================
//  PARTE 4 - VIATURAS, BOMBEIROS E DESPACHO
// ============================================================================

// ============================================================================
//  CONSULTAS
// ============================================================================

string nomeStatusViatura(int status) {
    switch (status) {
        case VIATURA_DISPONIVEL:    return "Disponível";
        case VIATURA_EM_OCORRENCIA: return "Em ocorrência";
        case VIATURA_EM_MANUTENCAO: return "Em manutenção";
        default:                    return "Desconhecido";
    }
}

// Prefixos são comparados sem diferenciar maiúsculas/minúsculas
Viatura* buscarViatura(ListaViaturas& viaturas, const string& prefixo) {
    string procurado = paraMaiusculas(aparar(prefixo));
    for (ListaViaturas::iterator it = viaturas.begin(); it != viaturas.end(); ++it)
        if (it->prefixo == procurado) return &(*it);
    return nullptr;
}

Bombeiro* buscarBombeiro(ListaBombeiros& bombeiros, const string& matricula) {
    string procurada = paraMaiusculas(aparar(matricula));
    for (ListaBombeiros::iterator it = bombeiros.begin(); it != bombeiros.end(); ++it)
        if (it->matricula == procurada) return &(*it);
    return nullptr;
}

int contarEquipe(const ListaBombeiros& bombeiros, const string& prefixo) {
    int total = 0;
    for (ListaBombeiros::const_iterator it = bombeiros.begin(); it != bombeiros.end(); ++it)
        if (!prefixo.empty() && it->prefixoViatura == prefixo) ++total;
    return total;
}

// Apta = disponível e com equipe mínima
bool viaturaApta(const Viatura& viatura, const ListaBombeiros& bombeiros) {
    return viatura.status == VIATURA_DISPONIVEL && contarEquipe(bombeiros, viatura.prefixo) >= EQUIPE_MINIMA;
}

// Identificadores: letras, números e hífen
static bool identificadorValido(const string& texto) {
    if (texto.empty()) return false;
    for (size_t i = 0; i < texto.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(texto[i]);
        if (!isalnum(c) && c != '-') return false;
    }
    return true;
}

// ============================================================================
//  CADASTROS E VÍNCULOS
// ============================================================================

bool cadastrarViatura(ListaViaturas& viaturas, Pilha& historico, Viatura dados, string& erro) {
    dados.prefixo = paraMaiusculas(aparar(dados.prefixo));
    dados.placa = paraMaiusculas(aparar(dados.placa));
    if (!identificadorValido(dados.prefixo) || dados.prefixo.size() > 12) {
        erro = "Prefixo inválido (use até 12 letras, números ou hífen).";
        return false;
    }
    if (buscarViatura(viaturas, dados.prefixo) != nullptr) {
        erro = "Já existe uma viatura com o prefixo " + dados.prefixo + ".";
        return false;
    }
    for (ListaViaturas::iterator it = viaturas.begin(); it != viaturas.end(); ++it)
        if (it->placa == dados.placa) {
            erro = "A placa " + dados.placa + " já pertence à viatura " + it->prefixo + ".";
            return false;
        }
    if (dados.tipo.empty() || dados.placa.empty()) {
        erro = "Tipo e placa são obrigatórios.";
        return false;
    }
    dados.status = VIATURA_DISPONIVEL;
    dados.numeroOcorrencia = 0;
    viaturas.push_back(dados);
    registrarOperacao(historico, OP_CADASTRO_VIATURA,
                      "Viatura " + dados.prefixo + " (" + dados.tipo + ", placa " + dados.placa + ") cadastrada");
    return true;
}

bool cadastrarBombeiro(ListaBombeiros& bombeiros, Pilha& historico, Bombeiro dados, string& erro) {
    dados.matricula = paraMaiusculas(aparar(dados.matricula));
    if (!identificadorValido(dados.matricula) || dados.matricula.size() < 3 || dados.matricula.size() > 12) {
        erro = "Matrícula inválida (3 a 12 letras, números ou hífen).";
        return false;
    }
    if (buscarBombeiro(bombeiros, dados.matricula) != nullptr) {
        erro = "Já existe um bombeiro com a matrícula " + dados.matricula + ".";
        return false;
    }
    if (comprimentoVisivel(dados.nome) < 3) {
        erro = "Nome inválido.";
        return false;
    }
    dados.prefixoViatura = "";
    bombeiros.push_back(dados);
    registrarOperacao(historico, OP_CADASTRO_BOMBEIRO,
                      dados.posto + " " + dados.nome + " (mat. " + dados.matricula + ") cadastrado");
    return true;
}

bool vincularBombeiro(ListaBombeiros& bombeiros, ListaViaturas& viaturas, Pilha& historico,
                      const string& matricula, const string& prefixo, string& erro) {
    Bombeiro* bombeiro = buscarBombeiro(bombeiros, matricula);
    if (bombeiro == nullptr) { erro = "Bombeiro não encontrado."; return false; }
    Viatura* viatura = buscarViatura(viaturas, prefixo);
    if (viatura == nullptr) { erro = "Viatura não encontrada."; return false; }

    if (bombeiro->prefixoViatura == viatura->prefixo) {
        erro = "O bombeiro já está vinculado a essa viatura.";
        return false;
    }
    if (viatura->status == VIATURA_EM_OCORRENCIA) {
        erro = "Não é possível alterar a equipe de uma viatura em ocorrência.";
        return false;
    }
    if (!bombeiro->prefixoViatura.empty()) {
        Viatura* atual = buscarViatura(viaturas, bombeiro->prefixoViatura);
        if (atual != nullptr && atual->status == VIATURA_EM_OCORRENCIA) {
            erro = "O bombeiro está em atendimento com a viatura " + atual->prefixo + ".";
            return false;
        }
    }
    if (contarEquipe(bombeiros, viatura->prefixo) >= EQUIPE_MAXIMA) {
        erro = "A viatura já está com a equipe completa (máximo de " + to_string(EQUIPE_MAXIMA) + ").";
        return false;
    }

    string anterior = bombeiro->prefixoViatura;
    bombeiro->prefixoViatura = viatura->prefixo;
    registrarOperacao(historico, OP_VINCULO_BOMBEIRO,
                      bombeiro->nome + " (mat. " + bombeiro->matricula + ") vinculado à viatura " +
                      viatura->prefixo + (anterior.empty() ? "" : " (antes: " + anterior + ")"));
    return true;
}

bool desvincularBombeiro(ListaBombeiros& bombeiros, ListaViaturas& viaturas, Pilha& historico,
                         const string& matricula, string& erro) {
    Bombeiro* bombeiro = buscarBombeiro(bombeiros, matricula);
    if (bombeiro == nullptr) { erro = "Bombeiro não encontrado."; return false; }
    if (bombeiro->prefixoViatura.empty()) { erro = "O bombeiro não está vinculado a nenhuma viatura."; return false; }
    Viatura* viatura = buscarViatura(viaturas, bombeiro->prefixoViatura);
    if (viatura != nullptr && viatura->status == VIATURA_EM_OCORRENCIA) {
        erro = "Não é possível desvincular: a viatura " + viatura->prefixo + " está em ocorrência.";
        return false;
    }
    string prefixo = bombeiro->prefixoViatura;
    bombeiro->prefixoViatura = "";
    registrarOperacao(historico, OP_VINCULO_BOMBEIRO,
                      bombeiro->nome + " (mat. " + bombeiro->matricula + ") desvinculado da viatura " + prefixo);
    return true;
}

// Permite alternar apenas entre Disponível e Em manutenção.
// "Em ocorrência" só é definido pelo despacho e desfeito pelo retorno à base.
bool alterarStatusViatura(ListaViaturas& viaturas, Pilha& historico, const string& prefixo,
                          int novoStatus, string& erro) {
    Viatura* viatura = buscarViatura(viaturas, prefixo);
    if (viatura == nullptr) { erro = "Viatura não encontrada."; return false; }
    if (novoStatus != VIATURA_DISPONIVEL && novoStatus != VIATURA_EM_MANUTENCAO) {
        erro = "O status \"Em ocorrência\" só é atribuído pelo despacho.";
        return false;
    }
    if (viatura->status == VIATURA_EM_OCORRENCIA) {
        erro = "A viatura está em ocorrência. Registre primeiro o retorno à base.";
        return false;
    }
    if (viatura->status == novoStatus) {
        erro = "A viatura já está com o status " + nomeStatusViatura(novoStatus) + ".";
        return false;
    }
    string antigo = nomeStatusViatura(viatura->status);
    viatura->status = novoStatus;
    registrarOperacao(historico, OP_STATUS_VIATURA,
                      "Viatura " + viatura->prefixo + ": " + antigo + " -> " + nomeStatusViatura(novoStatus));
    return true;
}

// ============================================================================
//  DESPACHO
// ============================================================================

/*
 * Algoritmo:
 *  1. Verifica se existem ocorrências aguardando.
 *  2. Identifica a fila de maior prioridade com ocorrências.
 *  3. Consulta a primeira ocorrência dessa fila (sem retirá-la ainda).
 *  4. Procura uma viatura disponível (ou valida a escolhida).
 *  5. Verifica a equipe mínima de dois bombeiros.
 *  6-8. Só então retira a ocorrência da fila e atualiza os status.
 *  9. Registra no histórico.
 * Se faltar viatura ou equipe, a ocorrência permanece na frente da fila original:
 * nada é perdido.
 */
InfoDespacho despacharOcorrencia(FilasPrioridade& filas, ListaViaturas& viaturas,
                                 ListaBombeiros& bombeiros, Pilha& historico,
                                 const string& prefixoEscolhido) {
    InfoDespacho info;
    info.ocorrencia = nullptr;
    info.viatura = nullptr;

    int prioridade = filaMaiorPrioridadeComOcorrencias(filas);
    if (prioridade == 0) {
        info.resultado = DESPACHO_SEM_OCORRENCIAS;
        return info;
    }
    Fila& fila = filas.filas[prioridade - 1];
    info.ocorrencia = frenteDaFila(fila);

    Viatura* escolhida = nullptr;
    if (!prefixoEscolhido.empty()) {
        // Viatura indicada pelo operador
        escolhida = buscarViatura(viaturas, prefixoEscolhido);
        info.viatura = escolhida;
        if (escolhida == nullptr) { info.resultado = DESPACHO_VIATURA_INEXISTENTE; return info; }
        if (escolhida->status != VIATURA_DISPONIVEL) { info.resultado = DESPACHO_SEM_VIATURA_DISPONIVEL; return info; }
        if (contarEquipe(bombeiros, escolhida->prefixo) < EQUIPE_MINIMA) {
            info.resultado = DESPACHO_EQUIPE_INCOMPLETA;
            return info;
        }
    } else {
        // Seleção automática: primeira viatura disponível com equipe mínima
        bool existeDisponivel = false;
        for (ListaViaturas::iterator it = viaturas.begin(); it != viaturas.end(); ++it) {
            if (it->status != VIATURA_DISPONIVEL) continue;
            existeDisponivel = true;
            if (contarEquipe(bombeiros, it->prefixo) >= EQUIPE_MINIMA) { escolhida = &(*it); break; }
        }
        if (!existeDisponivel) { info.resultado = DESPACHO_SEM_VIATURA_DISPONIVEL; return info; }
        if (escolhida == nullptr) { info.resultado = DESPACHO_EQUIPE_INCOMPLETA; return info; }
    }

    // Recursos confirmados: agora a ocorrência sai da fila
    Ocorrencia* ocorrencia = desenfileirar(fila);
    ocorrencia->status = EM_ATENDIMENTO;
    ocorrencia->viatura = escolhida->prefixo;
    escolhida->status = VIATURA_EM_OCORRENCIA;
    escolhida->numeroOcorrencia = ocorrencia->numero;

    registrarOperacao(historico, OP_DESPACHO_VIATURA,
                      "Viatura " + escolhida->prefixo + " despachada para " + formatarCodigo(ocorrencia->numero) +
                      " (" + nomeTipo(ocorrencia->tipo) + ", " + nomePrioridade(ocorrencia->prioridade) + ")",
                      ocorrencia->numero);

    info.resultado = DESPACHO_REALIZADO;
    info.ocorrencia = ocorrencia;
    info.viatura = escolhida;
    return info;
}

// Encerra uma ocorrência em atendimento e devolve sua viatura à base
bool finalizarAtendimento(ListaOcorrencias& lista, ListaViaturas& viaturas, Pilha& historico,
                          int numeroOcorrencia, string& erro) {
    Ocorrencia* ocorrencia = buscarOcorrencia(lista, numeroOcorrencia);
    if (ocorrencia == nullptr) { erro = "Ocorrência não encontrada."; return false; }
    if (ocorrencia->status != EM_ATENDIMENTO) {
        erro = "A ocorrência não está em atendimento (status: " + nomeStatus(ocorrencia->status) + ").";
        return false;
    }

    Viatura* viatura = buscarViatura(viaturas, ocorrencia->viatura);
    if (viatura != nullptr && viatura->status == VIATURA_EM_OCORRENCIA &&
        viatura->numeroOcorrencia == ocorrencia->numero) {
        viatura->status = VIATURA_DISPONIVEL;
        viatura->numeroOcorrencia = 0;
        registrarOperacao(historico, OP_RETORNO_VIATURA,
                          "Viatura " + viatura->prefixo + " retornou à base após " +
                          formatarCodigo(ocorrencia->numero), ocorrencia->numero);
    }
    ocorrencia->status = ENCERRADA;
    registrarOperacao(historico, OP_ENCERRAMENTO_OCORRENCIA,
                      formatarCodigo(ocorrencia->numero) + " encerrada (atendida pela viatura " +
                      ocorrencia->viatura + ")", ocorrencia->numero);
    return true;
}

// Encerra uma ocorrência que ainda estava na fila (ex.: trote, cancelamento)
bool encerrarSemAtendimento(ListaOcorrencias& lista, FilasPrioridade& filas, Pilha& historico,
                            int numeroOcorrencia, const string& motivo, string& erro) {
    Ocorrencia* ocorrencia = buscarOcorrencia(lista, numeroOcorrencia);
    if (ocorrencia == nullptr) { erro = "Ocorrência não encontrada."; return false; }
    if (ocorrencia->status != AGUARDANDO) { erro = "A ocorrência não está aguardando."; return false; }

    removerDaFila(filas.filas[ocorrencia->prioridade - 1], ocorrencia);
    ocorrencia->status = ENCERRADA;
    ocorrencia->detalhes += " | Encerrada sem atendimento: " + motivo;
    registrarOperacao(historico, OP_ENCERRAMENTO_OCORRENCIA,
                      formatarCodigo(ocorrencia->numero) + " encerrada sem atendimento. Motivo: " + motivo,
                      ocorrencia->numero);
    return true;
}

bool registrarRetornoViatura(ListaOcorrencias& lista, ListaViaturas& viaturas, Pilha& historico,
                             const string& prefixo, string& erro) {
    Viatura* viatura = buscarViatura(viaturas, prefixo);
    if (viatura == nullptr) { erro = "Viatura não encontrada."; return false; }
    if (viatura->status != VIATURA_EM_OCORRENCIA) { erro = "A viatura não está em ocorrência."; return false; }

    if (buscarOcorrencia(lista, viatura->numeroOcorrencia) == nullptr) {
        // Situação inconsistente: libera a viatura mesmo assim
        viatura->status = VIATURA_DISPONIVEL;
        viatura->numeroOcorrencia = 0;
        registrarOperacao(historico, OP_RETORNO_VIATURA, "Viatura " + viatura->prefixo + " retornou à base");
        return true;
    }
    return finalizarAtendimento(lista, viaturas, historico, viatura->numeroOcorrencia, erro);
}

// ============================================================================
//  INTERFACE - VIATURAS
// ============================================================================

void listarViaturas(const ListaViaturas& viaturas, const ListaBombeiros& bombeiros, bool apenasDisponiveis) {
    cout << ajustar("PREFIXO", 10) << " " << ajustar("TIPO", 24) << " " << ajustar("PLACA", 9) << " "
         << ajustar("STATUS", 14) << " " << ajustar("EQUIPE", 7) << " " << "OCORRÊNCIA/APTA\n";
    imprimirLinha('-', 80);
    int exibidas = 0;
    for (ListaViaturas::const_iterator it = viaturas.begin(); it != viaturas.end(); ++it) {
        if (apenasDisponiveis && it->status != VIATURA_DISPONIVEL) continue;
        int equipe = contarEquipe(bombeiros, it->prefixo);
        string situacao;
        if (it->status == VIATURA_EM_OCORRENCIA) situacao = formatarCodigo(it->numeroOcorrencia);
        else if (it->status == VIATURA_DISPONIVEL)
            situacao = equipe >= EQUIPE_MINIMA ? "Apta p/ despacho" : "Equipe incompleta";
        else situacao = "-";
        cout << ajustar(it->prefixo, 10) << " " << ajustar(it->tipo, 24) << " " << ajustar(it->placa, 9) << " "
             << ajustar(nomeStatusViatura(it->status), 14) << " " << ajustar(to_string(equipe), 7) << " "
             << situacao << "\n";
        ++exibidas;
    }
    if (exibidas == 0)
        cout << (apenasDisponiveis ? "Nenhuma viatura disponível.\n" : "Nenhuma viatura cadastrada.\n");
    else
        cout << "Total: " << exibidas << " viatura(s).\n";
}

void exibirEquipe(const ListaBombeiros& bombeiros, const string& prefixo) {
    int total = 0;
    for (ListaBombeiros::const_iterator it = bombeiros.begin(); it != bombeiros.end(); ++it) {
        if (it->prefixoViatura != prefixo) continue;
        cout << "   - " << ajustar(it->matricula, 12) << " " << it->posto << " " << it->nome << "\n";
        ++total;
    }
    if (total == 0) cout << "   (nenhum bombeiro vinculado)\n";
    cout << "   Equipe: " << total << " bombeiro(s) - "
         << (total >= EQUIPE_MINIMA ? "equipe mínima atendida" : "ABAIXO da equipe mínima de 2") << "\n";
}

static void exibirViaturaDetalhada(const Viatura& v, const ListaBombeiros& bombeiros) {
    imprimirLinha('-');
    cout << "  Prefixo ....: " << v.prefixo << "\n"
         << "  Tipo .......: " << v.tipo << "\n"
         << "  Placa ......: " << v.placa << "\n"
         << "  Status .....: " << nomeStatusViatura(v.status) << "\n";
    if (v.status == VIATURA_EM_OCORRENCIA)
        cout << "  Ocorrência .: " << formatarCodigo(v.numeroOcorrencia) << "\n";
    cout << "  Equipe:\n";
    exibirEquipe(bombeiros, v.prefixo);
    imprimirLinha('-');
}

// Mostra o resultado de uma tentativa de despacho com mensagens claras
static void informarResultadoDespacho(const InfoDespacho& info, const FilasPrioridade& filas) {
    switch (info.resultado) {
        case DESPACHO_REALIZADO:
            cout << "\n  [OK] DESPACHO REALIZADO\n"
                 << "       Ocorrência: " << formatarCodigo(info.ocorrencia->numero) << " - "
                 << nomeTipo(info.ocorrencia->tipo) << " (" << nomePrioridade(info.ocorrencia->prioridade) << ")\n"
                 << "       Local ....: " << info.ocorrencia->endereco << " - " << info.ocorrencia->bairro << "\n"
                 << "       Viatura ..: " << info.viatura->prefixo << " (" << info.viatura->tipo << ")\n"
                 << "       Status ...: ocorrência Em atendimento / viatura Em ocorrência\n";
            return;
        case DESPACHO_SEM_OCORRENCIAS:
            cout << "  [INFO] Não há ocorrências aguardando despacho.\n";
            return;
        case DESPACHO_VIATURA_INEXISTENTE:
            cout << "  [ERRO] Viatura não encontrada.\n";
            break;
        case DESPACHO_SEM_VIATURA_DISPONIVEL:
            cout << "  [ERRO] Não há viatura disponível"
                 << (info.viatura != nullptr ? " (a viatura escolhida está " +
                     paraMinusculas(nomeStatusViatura(info.viatura->status)) + ")" : "") << ".\n";
            break;
        case DESPACHO_EQUIPE_INCOMPLETA:
            cout << "  [ERRO] Equipe incompleta: "
                 << (info.viatura != nullptr ? "a viatura " + info.viatura->prefixo + " não possui"
                                             : "nenhuma viatura disponível possui")
                 << " o mínimo de " << EQUIPE_MINIMA << " bombeiros vinculados.\n";
            break;
    }
    if (info.ocorrencia != nullptr) {
        int p = info.ocorrencia->prioridade;
        cout << "  A ocorrência " << formatarCodigo(info.ocorrencia->numero)
             << " NÃO foi perdida: permanece aguardando na fila " << p << " (" << nomePrioridade(p)
             << "), posição " << posicaoNaFila(filas.filas[p - 1], info.ocorrencia) << ".\n";
    }
}

void menuDespachar(ListaOcorrencias& lista, FilasPrioridade& filas, ListaViaturas& viaturas,
                   ListaBombeiros& bombeiros, Pilha& historico) {
    (void)lista;
    imprimirTitulo("DESPACHAR OCORRÊNCIA");
    int prioridade = filaMaiorPrioridadeComOcorrencias(filas);
    if (prioridade == 0) {
        cout << "  [INFO] Não há ocorrências aguardando despacho.\n";
        return;
    }
    Ocorrencia* proxima = frenteDaFila(filas.filas[prioridade - 1]);
    cout << "Próxima ocorrência a ser atendida (fila " << prioridade << " - " << nomePrioridade(prioridade) << "):\n";
    exibirOcorrenciaDetalhada(*proxima);
    cout << "Ocorrências aguardando no total: " << totalAguardando(filas) << "\n\n";

    cout << "  1 - Despachar com a primeira viatura apta (automático)\n"
         << "  2 - Escolher a viatura\n"
         << "  0 - Cancelar\n";
    int opcao = lerInteiro("Opção: ", 0, 2);
    if (opcao == 0) { cout << "Despacho cancelado.\n"; return; }

    string prefixo;
    if (opcao == 2) {
        cout << "\nViaturas disponíveis:\n";
        listarViaturas(viaturas, bombeiros, true);
        prefixo = lerTexto("Prefixo da viatura: ", 1, 12);
    }
    InfoDespacho info = despacharOcorrencia(filas, viaturas, bombeiros, historico, prefixo);
    informarResultadoDespacho(info, filas);
}

void menuEncerrarOcorrencia(ListaOcorrencias& lista, FilasPrioridade& filas, ListaViaturas& viaturas,
                            Pilha& historico) {
    imprimirTitulo("ENCERRAR OCORRÊNCIA");
    Ocorrencia* ocorrencia = solicitarOcorrencia(lista);
    if (ocorrencia == nullptr) return;
    exibirOcorrenciaDetalhada(*ocorrencia);

    string erro;
    if (ocorrencia->status == ENCERRADA) {
        cout << "  [ERRO] Esta ocorrência já está encerrada.\n";
    } else if (ocorrencia->status == EM_ATENDIMENTO) {
        if (!lerSimNao("Confirmar encerramento e retorno da viatura " + ocorrencia->viatura + " à base?")) {
            cout << "Operação cancelada.\n";
            return;
        }
        if (finalizarAtendimento(lista, viaturas, historico, ocorrencia->numero, erro))
            cout << "  [OK] Ocorrência encerrada. Viatura " << ocorrencia->viatura << " disponível na base.\n";
        else
            cout << "  [ERRO] " << erro << "\n";
    } else {
        cout << "  [ATENÇÃO] Esta ocorrência ainda não foi atendida (está na fila).\n";
        if (!lerSimNao("Deseja encerrá-la sem atendimento (ex.: trote, cancelamento)?")) {
            cout << "Operação cancelada.\n";
            return;
        }
        string motivo = lerTexto("Motivo: ", 3, 100);
        if (encerrarSemAtendimento(lista, filas, historico, ocorrencia->numero, motivo, erro))
            cout << "  [OK] Ocorrência encerrada e retirada da fila.\n";
        else
            cout << "  [ERRO] " << erro << "\n";
    }
}

static void telaCadastrarViatura(ListaViaturas& viaturas, Pilha& historico) {
    imprimirTitulo("CADASTRAR VIATURA");
    static const char* TIPOS[] = { "Auto Bomba Tanque", "Unidade de Resgate", "Auto Busca e Salvamento",
                                   "Auto Escada Mecânica", "Viatura de Apoio" };
    Viatura dados;
    dados.prefixo = lerTexto("Prefixo (ex.: ABT-01): ", 2, 12);
    for (int i = 0; i < 5; ++i) cout << "  " << i + 1 << " - " << TIPOS[i] << "\n";
    dados.tipo = TIPOS[lerInteiro("Tipo: ", 1, 5) - 1];
    dados.placa = lerTexto("Placa: ", 7, 8);

    string erro;
    if (cadastrarViatura(viaturas, historico, dados, erro))
        cout << "  [OK] Viatura " << paraMaiusculas(dados.prefixo)
             << " cadastrada como Disponível. Vincule ao menos " << EQUIPE_MINIMA << " bombeiros para despachá-la.\n";
    else
        cout << "  [ERRO] " << erro << "\n";
}

static void telaAlterarStatus(ListaViaturas& viaturas, Pilha& historico, int statusFixo) {
    string prefixo = lerTexto("Prefixo da viatura: ", 1, 12);
    int novo = statusFixo;
    if (novo == 0) {
        cout << "  1 - Disponível\n  2 - Em manutenção\n";
        novo = lerInteiro("Novo status: ", 1, 2) == 1 ? VIATURA_DISPONIVEL : VIATURA_EM_MANUTENCAO;
    }
    string erro;
    if (alterarStatusViatura(viaturas, historico, prefixo, novo, erro))
        cout << "  [OK] Viatura " << paraMaiusculas(prefixo) << " agora está " << nomeStatusViatura(novo) << ".\n";
    else
        cout << "  [ERRO] " << erro << "\n";
}

static void telaRetornoViatura(ListaOcorrencias& lista, ListaViaturas& viaturas, Pilha& historico) {
    string prefixo = lerTexto("Prefixo da viatura que retornou: ", 1, 12);
    Viatura* viatura = buscarViatura(viaturas, prefixo);
    if (viatura == nullptr) { cout << "  [ERRO] Viatura não encontrada.\n"; return; }
    if (viatura->status != VIATURA_EM_OCORRENCIA) { cout << "  [ERRO] A viatura não está em ocorrência.\n"; return; }
    cout << "  A viatura atende " << formatarCodigo(viatura->numeroOcorrencia)
         << ". O retorno à base encerra essa ocorrência.\n";
    if (!lerSimNao("Confirmar retorno?")) { cout << "Operação cancelada.\n"; return; }
    string erro;
    if (registrarRetornoViatura(lista, viaturas, historico, prefixo, erro))
        cout << "  [OK] Viatura " << viatura->prefixo << " de volta à base e disponível.\n";
    else
        cout << "  [ERRO] " << erro << "\n";
}

void menuViaturas(ListaOcorrencias& lista, FilasPrioridade& filas, ListaViaturas& viaturas,
                  ListaBombeiros& bombeiros, Pilha& historico) {
    while (true) {
        imprimirTitulo("GERENCIAR VIATURAS");
        cout << "  1 - Cadastrar viatura\n"
             << "  2 - Consultar viatura\n"
             << "  3 - Listar todas as viaturas\n"
             << "  4 - Listar viaturas disponíveis\n"
             << "  5 - Alterar status\n"
             << "  6 - Registrar saída para atendimento\n"
             << "  7 - Registrar retorno à base\n"
             << "  8 - Enviar viatura para manutenção\n"
             << "  9 - Consultar equipe de uma viatura\n"
             << "  0 - Voltar\n";
        int opcao = lerInteiro("Escolha uma opção: ", 0, 9);
        if (opcao == 0) return;

        switch (opcao) {
            case 1: telaCadastrarViatura(viaturas, historico); break;
            case 2: case 9: {
                Viatura* v = buscarViatura(viaturas, lerTexto("Prefixo da viatura: ", 1, 12));
                if (v == nullptr) cout << "  [ERRO] Viatura não encontrada.\n";
                else if (opcao == 2) exibirViaturaDetalhada(*v, bombeiros);
                else { cout << "Equipe da viatura " << v->prefixo << ":\n"; exibirEquipe(bombeiros, v->prefixo); }
                break;
            }
            case 3: listarViaturas(viaturas, bombeiros, false); break;
            case 4: listarViaturas(viaturas, bombeiros, true); break;
            case 5: telaAlterarStatus(viaturas, historico, 0); break;
            case 6: {
                // Saída para atendimento: a viatura escolhida assume a próxima ocorrência da fila
                listarViaturas(viaturas, bombeiros, true);
                string prefixo = lerTexto("Prefixo da viatura que sairá: ", 1, 12);
                InfoDespacho info = despacharOcorrencia(filas, viaturas, bombeiros, historico, prefixo);
                informarResultadoDespacho(info, filas);
                break;
            }
            case 7: telaRetornoViatura(lista, viaturas, historico); break;
            case 8: telaAlterarStatus(viaturas, historico, VIATURA_EM_MANUTENCAO); break;
        }
        pausar();
    }
}

// ============================================================================
//  INTERFACE - BOMBEIROS
// ============================================================================

static void listarBombeiros(const ListaBombeiros& bombeiros) {
    cout << ajustar("MATRÍCULA", 12) << " " << ajustar("POSTO", 14) << " " << ajustar("NOME", 30) << " VIATURA\n";
    imprimirLinha('-', 70);
    for (ListaBombeiros::const_iterator it = bombeiros.begin(); it != bombeiros.end(); ++it)
        cout << ajustar(it->matricula, 12) << " " << ajustar(it->posto, 14) << " " << ajustar(it->nome, 30) << " "
             << (it->prefixoViatura.empty() ? "-" : it->prefixoViatura) << "\n";
    if (bombeiros.empty()) cout << "Nenhum bombeiro cadastrado.\n";
    else cout << "Total: " << bombeiros.size() << " bombeiro(s).\n";
}

static void telaCadastrarBombeiro(ListaBombeiros& bombeiros, Pilha& historico) {
    imprimirTitulo("CADASTRAR BOMBEIRO");
    static const char* POSTOS[] = { "Soldado", "Cabo", "3º Sargento", "2º Sargento", "1º Sargento",
                                    "Subtenente", "Tenente", "Capitão" };
    Bombeiro dados;
    dados.matricula = lerTexto("Matrícula: ", 3, 12);
    dados.nome = lerTexto("Nome completo: ", 3, 60);
    for (int i = 0; i < 8; ++i) cout << "  " << i + 1 << " - " << POSTOS[i] << "\n";
    dados.posto = POSTOS[lerInteiro("Posto/graduação: ", 1, 8) - 1];

    string erro;
    if (cadastrarBombeiro(bombeiros, historico, dados, erro))
        cout << "  [OK] Bombeiro cadastrado. Use a opção \"Vincular\" para associá-lo a uma viatura.\n";
    else
        cout << "  [ERRO] " << erro << "\n";
}

void menuBombeiros(ListaViaturas& viaturas, ListaBombeiros& bombeiros, Pilha& historico) {
    while (true) {
        imprimirTitulo("GERENCIAR BOMBEIROS");
        cout << "  1 - Cadastrar bombeiro\n"
             << "  2 - Consultar bombeiro\n"
             << "  3 - Listar bombeiros\n"
             << "  4 - Vincular bombeiro a uma viatura\n"
             << "  5 - Desvincular bombeiro\n"
             << "  6 - Consultar equipe de uma viatura\n"
             << "  0 - Voltar\n";
        int opcao = lerInteiro("Escolha uma opção: ", 0, 6);
        if (opcao == 0) return;

        string erro;
        switch (opcao) {
            case 1: telaCadastrarBombeiro(bombeiros, historico); break;
            case 2: {
                Bombeiro* b = buscarBombeiro(bombeiros, lerTexto("Matrícula: ", 1, 12));
                if (b == nullptr) { cout << "  [ERRO] Bombeiro não encontrado.\n"; break; }
                cout << "  Matrícula: " << b->matricula << "\n  Nome: " << b->nome << "\n  Posto: " << b->posto
                     << "\n  Viatura: " << (b->prefixoViatura.empty() ? "Sem vínculo" : b->prefixoViatura) << "\n";
                break;
            }
            case 3: listarBombeiros(bombeiros); break;
            case 4: {
                if (viaturas.empty()) { cout << "  [ERRO] Cadastre uma viatura primeiro.\n"; break; }
                string matricula = lerTexto("Matrícula do bombeiro: ", 1, 12);
                listarViaturas(viaturas, bombeiros, false);
                string prefixo = lerTexto("Prefixo da viatura: ", 1, 12);
                if (vincularBombeiro(bombeiros, viaturas, historico, matricula, prefixo, erro))
                    cout << "  [OK] Vínculo realizado. Equipe da viatura " << paraMaiusculas(prefixo) << ": "
                         << contarEquipe(bombeiros, paraMaiusculas(prefixo)) << " bombeiro(s).\n";
                else
                    cout << "  [ERRO] " << erro << "\n";
                break;
            }
            case 5: {
                string matricula = lerTexto("Matrícula do bombeiro: ", 1, 12);
                if (desvincularBombeiro(bombeiros, viaturas, historico, matricula, erro))
                    cout << "  [OK] Bombeiro desvinculado.\n";
                else
                    cout << "  [ERRO] " << erro << "\n";
                break;
            }
            case 6: {
                Viatura* v = buscarViatura(viaturas, lerTexto("Prefixo da viatura: ", 1, 12));
                if (v == nullptr) cout << "  [ERRO] Viatura não encontrada.\n";
                else { cout << "Equipe da viatura " << v->prefixo << ":\n"; exibirEquipe(bombeiros, v->prefixo); }
                break;
            }
        }
        pausar();
    }
}

// ============================================================================
//  PARTE 5 - CONSULTAS, RELATÓRIOS E HISTÓRICO
// ============================================================================

// ============================================================================
//  BUSCAS (geram listas simplesmente encadeadas de resultados)
// ============================================================================

ListaResultado filtrarPorTipo(ListaOcorrencias& lista, int tipo) {
    ListaResultado resultado;
    inicializarResultado(resultado);
    for (NoOcorrencia* no = lista.inicio; no != nullptr; no = no->proximo)
        if (no->dados.tipo == tipo) adicionarResultado(resultado, &no->dados);
    return resultado;
}

// Busca parcial e sem diferenciar maiúsculas/minúsculas ("cen" encontra "Centro")
ListaResultado filtrarPorBairro(ListaOcorrencias& lista, const string& bairro) {
    ListaResultado resultado;
    inicializarResultado(resultado);
    string procurado = paraMinusculas(bairro);
    for (NoOcorrencia* no = lista.inicio; no != nullptr; no = no->proximo)
        if (paraMinusculas(no->dados.bairro).find(procurado) != string::npos)
            adicionarResultado(resultado, &no->dados);
    return resultado;
}

ListaResultado filtrarPorStatus(ListaOcorrencias& lista, int status) {
    ListaResultado resultado;
    inicializarResultado(resultado);
    for (NoOcorrencia* no = lista.inicio; no != nullptr; no = no->proximo)
        if (no->dados.status == status) adicionarResultado(resultado, &no->dados);
    return resultado;
}

// Exibe e libera a lista de resultados
static void exibirResultado(ListaResultado& resultado, const string& vazio) {
    if (resultado.tamanho == 0) {
        cout << vazio << "\n";
    } else {
        exibirCabecalhoOcorrencias();
        for (NoResultado* no = resultado.inicio; no != nullptr; no = no->proximo)
            exibirOcorrenciaResumida(*no->ocorrencia);
        cout << "Total encontrado: " << resultado.tamanho << "\n";
    }
    liberarResultado(resultado);
}

// ============================================================================
//  CONSULTAS
// ============================================================================

void listarTodasOcorrencias(ListaOcorrencias& lista) {
    imprimirTitulo("TODAS AS OCORRÊNCIAS (ordem de registro)");
    if (lista.tamanho == 0) { cout << "Nenhuma ocorrência registrada.\n"; return; }
    exibirCabecalhoOcorrencias();
    for (NoOcorrencia* no = lista.inicio; no != nullptr; no = no->proximo)
        exibirOcorrenciaResumida(no->dados);
    cout << "Total: " << lista.tamanho << " ocorrência(s).\n";
}

void listarPorStatus(ListaOcorrencias& lista, int status) {
    imprimirTitulo("OCORRÊNCIAS - " + paraMaiusculas(nomeStatus(status)));
    ListaResultado resultado = filtrarPorStatus(lista, status);
    exibirResultado(resultado, "Nenhuma ocorrência com esse status.");
}

void consultarPorCodigo(ListaOcorrencias& lista, const FilasPrioridade& filas) {
    imprimirTitulo("CONSULTAR OCORRÊNCIA POR CÓDIGO");
    Ocorrencia* oc = solicitarOcorrencia(lista);
    if (oc == nullptr) return;
    exibirOcorrenciaDetalhada(*oc);
    if (oc->status == AGUARDANDO)
        cout << "  Posição na fila " << oc->prioridade << ": "
             << posicaoNaFila(filas.filas[oc->prioridade - 1], oc) << "º\n";
}

void buscarPorTipo(ListaOcorrencias& lista) {
    imprimirTitulo("BUSCAR POR TIPO");
    for (int i = 1; i <= TOTAL_TIPOS; ++i) cout << "  " << setw(2) << i << " - " << nomeTipo(i) << "\n";
    int tipo = lerInteiro("Tipo: ", 1, TOTAL_TIPOS);
    ListaResultado resultado = filtrarPorTipo(lista, tipo);
    exibirResultado(resultado, "Nenhuma ocorrência do tipo " + nomeTipo(tipo) + ".");
}

void buscarPorBairro(ListaOcorrencias& lista) {
    imprimirTitulo("BUSCAR POR BAIRRO");
    string bairro = lerTexto("Bairro (ou parte do nome): ", 1, 40);
    ListaResultado resultado = filtrarPorBairro(lista, bairro);
    exibirResultado(resultado, "Nenhuma ocorrência encontrada no bairro \"" + bairro + "\".");
}

// ============================================================================
//  FILAS
// ============================================================================

void visualizarFilas(const FilasPrioridade& filas) {
    imprimirTitulo("FILAS DE PRIORIDADE (atendimento: 1 > 2 > 3 > 4, FIFO em cada fila)");
    for (int i = 0; i < TOTAL_PRIORIDADES; ++i) {
        const Fila& fila = filas.filas[i];
        cout << "\nFILA " << i + 1 << " - " << nomePrioridade(i + 1) << " (" << fila.tamanho << " aguardando)\n";
        imprimirLinha('-', 70);
        if (filaVazia(fila)) { cout << "   (vazia)\n"; continue; }
        int posicao = 1;
        for (NoFila* no = fila.inicio; no != nullptr; no = no->proximo, ++posicao) {
            const Ocorrencia& oc = *no->ocorrencia;
            cout << "  " << setw(2) << posicao << "º  " << formatarCodigo(oc.numero) << "  "
                 << ajustar(nomeTipo(oc.tipo), 19) << " " << ajustar(oc.bairro, 15) << " "
                 << oc.dataHora.substr(0, 16) << "\n";
        }
    }
    cout << "\nTotal aguardando despacho: " << totalAguardando(filas) << "\n";
}

void consultarPosicaoNaFila(ListaOcorrencias& lista, const FilasPrioridade& filas) {
    imprimirTitulo("POSIÇÃO DE UMA OCORRÊNCIA NA FILA");
    Ocorrencia* oc = solicitarOcorrencia(lista);
    if (oc == nullptr) return;
    if (oc->status != AGUARDANDO) {
        cout << "  A ocorrência " << formatarCodigo(oc->numero) << " não está em fila (status: "
             << nomeStatus(oc->status) << ").\n";
        return;
    }
    int posicao = posicaoNaFila(filas.filas[oc->prioridade - 1], oc);
    int aFrente = posicao - 1;
    for (int i = 0; i < oc->prioridade - 1; ++i) aFrente += filas.filas[i].tamanho;
    cout << "  " << formatarCodigo(oc->numero) << " está na fila " << oc->prioridade << " ("
         << nomePrioridade(oc->prioridade) << "), posição " << posicao << "º.\n"
         << "  Ocorrências que serão despachadas antes dela (todas as filas): " << aFrente << "\n";
}

// ============================================================================
//  RELATÓRIOS
// ============================================================================

void relatorioPorPrioridade(const ListaOcorrencias& lista) {
    imprimirTitulo("QUANTIDADE DE OCORRÊNCIAS POR PRIORIDADE");
    int contagem[TOTAL_PRIORIDADES][4] = {{0}};   // [prioridade][status 1..3], coluna 0 = total
    for (NoOcorrencia* no = lista.inicio; no != nullptr; no = no->proximo) {
        contagem[no->dados.prioridade - 1][0]++;
        contagem[no->dados.prioridade - 1][no->dados.status]++;
    }
    cout << ajustar("PRIORIDADE", 14) << ajustar("TOTAL", 8) << ajustar("AGUARDANDO", 12)
         << ajustar("EM ATEND.", 11) << "ENCERRADAS\n";
    imprimirLinha('-', 58);
    for (int i = 0; i < TOTAL_PRIORIDADES; ++i)
        cout << ajustar(to_string(i + 1) + " - " + nomePrioridade(i + 1), 14) << ajustar(to_string(contagem[i][0]), 8)
             << ajustar(to_string(contagem[i][AGUARDANDO]), 12) << ajustar(to_string(contagem[i][EM_ATENDIMENTO]), 11)
             << contagem[i][ENCERRADA] << "\n";
    imprimirLinha('-', 58);
    cout << "Total geral: " << lista.tamanho << "\n";
}

void relatorioPorTipo(const ListaOcorrencias& lista) {
    imprimirTitulo("QUANTIDADE DE OCORRÊNCIAS POR TIPO");
    int contagem[TOTAL_TIPOS][4] = {{0}};
    for (NoOcorrencia* no = lista.inicio; no != nullptr; no = no->proximo) {
        contagem[no->dados.tipo - 1][0]++;
        contagem[no->dados.tipo - 1][no->dados.status]++;
    }
    cout << ajustar("TIPO", 20) << ajustar("TOTAL", 8) << ajustar("AGUARDANDO", 12)
         << ajustar("EM ATEND.", 11) << "ENCERRADAS\n";
    imprimirLinha('-', 64);
    for (int i = 0; i < TOTAL_TIPOS; ++i)
        cout << ajustar(nomeTipo(i + 1), 20) << ajustar(to_string(contagem[i][0]), 8)
             << ajustar(to_string(contagem[i][AGUARDANDO]), 12) << ajustar(to_string(contagem[i][EM_ATENDIMENTO]), 11)
             << contagem[i][ENCERRADA] << "\n";
    imprimirLinha('-', 64);
    cout << "Total geral: " << lista.tamanho << "\n";
}

void relatorioViaturasDisponiveis(const ListaViaturas& viaturas, const ListaBombeiros& bombeiros) {
    imprimirTitulo("VIATURAS DISPONÍVEIS");
    listarViaturas(viaturas, bombeiros, true);
    int aptas = 0;
    for (ListaViaturas::const_iterator it = viaturas.begin(); it != viaturas.end(); ++it)
        if (viaturaApta(*it, bombeiros)) ++aptas;
    cout << "Aptas para despacho (disponíveis com equipe mínima): " << aptas << "\n";
}

void resumoGeral(const ListaOcorrencias& lista, const FilasPrioridade& filas,
                 const ListaViaturas& viaturas, const ListaBombeiros& bombeiros) {
    imprimirTitulo("RESUMO GERAL");
    int porStatus[4] = {0};
    for (NoOcorrencia* no = lista.inicio; no != nullptr; no = no->proximo) porStatus[no->dados.status]++;
    int porStatusViatura[4] = {0};
    for (ListaViaturas::const_iterator it = viaturas.begin(); it != viaturas.end(); ++it) porStatusViatura[it->status]++;
    int semVinculo = 0;
    for (ListaBombeiros::const_iterator it = bombeiros.begin(); it != bombeiros.end(); ++it)
        if (it->prefixoViatura.empty()) ++semVinculo;

    cout << "  Ocorrências registradas ..: " << lista.tamanho << "\n"
         << "     Aguardando ............: " << porStatus[AGUARDANDO] << "\n"
         << "     Em atendimento ........: " << porStatus[EM_ATENDIMENTO] << "\n"
         << "     Encerradas ............: " << porStatus[ENCERRADA] << "\n"
         << "  Filas (C/A/M/B) ..........: " << filas.filas[0].tamanho << " / " << filas.filas[1].tamanho
         << " / " << filas.filas[2].tamanho << " / " << filas.filas[3].tamanho << "\n"
         << "  Viaturas cadastradas .....: " << viaturas.size() << "\n"
         << "     Disponíveis ...........: " << porStatusViatura[VIATURA_DISPONIVEL] << "\n"
         << "     Em ocorrência .........: " << porStatusViatura[VIATURA_EM_OCORRENCIA] << "\n"
         << "     Em manutenção .........: " << porStatusViatura[VIATURA_EM_MANUTENCAO] << "\n"
         << "  Bombeiros cadastrados ....: " << bombeiros.size() << " (" << semVinculo << " sem viatura)\n";
}

// ============================================================================
//  HISTÓRICO (percorre a pilha a partir do topo, sem removê-la)
// ============================================================================

void exibirHistorico(const Pilha& historico, int quantidade) {
    imprimirTitulo("ÚLTIMAS OPERAÇÕES (mais recente primeiro)");
    if (pilhaVazia(historico)) {
        cout << "Nenhuma operação registrada nesta sessão.\n"
             << "(O histórico completo de todas as sessões fica em dados/log_chamados.txt)\n";
        return;
    }
    int contador = 0;
    for (NoPilha* no = historico.topo; no != nullptr && contador < quantidade; no = no->abaixo) {
        ++contador;
        cout << setw(3) << contador << ". [" << no->dados.dataHora << "] " << nomeOperacao(no->dados.tipo) << "\n"
             << "      " << no->dados.descricao << "\n";
    }
    cout << "\nExibidas " << contador << " de " << historico.tamanho << " operação(ões) da sessão.\n";
}

// ============================================================================
//  SUBMENUS
// ============================================================================

void menuFilas(ListaOcorrencias& lista, const FilasPrioridade& filas) {
    while (true) {
        visualizarFilas(filas);
        cout << "\n  1 - Consultar a posição de uma ocorrência\n  0 - Voltar\n";
        int opcao = lerInteiro("Escolha uma opção: ", 0, 1);
        if (opcao == 0) return;
        consultarPosicaoNaFila(lista, filas);
        pausar();
    }
}

void menuHistorico(ListaOcorrencias& lista, FilasPrioridade& filas, Pilha& historico) {
    while (true) {
        imprimirTitulo("HISTÓRICO DE OPERAÇÕES");
        cout << "  1 - Ver as últimas 10 operações\n"
             << "  2 - Desfazer o último cadastro de ocorrência\n"
             << "  0 - Voltar\n";
        int opcao = lerInteiro("Escolha uma opção: ", 0, 2);
        if (opcao == 0) return;
        if (opcao == 1) {
            exibirHistorico(historico, 10);
        } else {
            string mensagem;
            if (!lerSimNao("O último cadastro de ocorrência será removido. Confirmar?")) {
                cout << "Operação cancelada.\n";
            } else if (desfazerUltimoCadastro(lista, filas, historico, mensagem)) {
                cout << "  [OK] " << mensagem << "\n";
            } else {
                cout << "  [ERRO] " << mensagem << "\n";
            }
        }
        pausar();
    }
}

void menuRelatorios(ListaOcorrencias& lista, const FilasPrioridade& filas,
                    const ListaViaturas& viaturas, const ListaBombeiros& bombeiros) {
    while (true) {
        imprimirTitulo("RELATÓRIOS");
        cout << "  1 - Quantidade de ocorrências por prioridade\n"
             << "  2 - Quantidade de ocorrências por tipo\n"
             << "  3 - Viaturas disponíveis\n"
             << "  4 - Resumo geral\n"
             << "  5 - Visualizar as quatro filas\n"
             << "  6 - Posição de uma ocorrência na fila\n"
             << "  0 - Voltar\n";
        int opcao = lerInteiro("Escolha uma opção: ", 0, 6);
        if (opcao == 0) return;
        switch (opcao) {
            case 1: relatorioPorPrioridade(lista); break;
            case 2: relatorioPorTipo(lista); break;
            case 3: relatorioViaturasDisponiveis(viaturas, bombeiros); break;
            case 4: resumoGeral(lista, filas, viaturas, bombeiros); break;
            case 5: visualizarFilas(filas); break;
            case 6: consultarPosicaoNaFila(lista, filas); break;
        }
        pausar();
    }
}

// ============================================================================
//  PARTE 6 - PERSISTÊNCIA EM ARQUIVOS CSV
// ============================================================================

static string pastaDados = "dados/";

void definirPastaDados(const string& pasta) {
    pastaDados = pasta;
    if (!pastaDados.empty() && pastaDados[pastaDados.size() - 1] != '/') pastaDados += '/';
}

string caminhoDados(const string& arquivo) {
    return pastaDados + arquivo;
}

// Cria a pasta de dados caso ela não exista (comando "mkdir" existe no Windows e no Linux)
static void criarPastaDados() {
    string pasta = pastaDados.substr(0, pastaDados.size() - 1);
#ifdef _WIN32
    replace(pasta.begin(), pasta.end(), '/', '\\');
    string comando = "mkdir \"" + pasta + "\" 2>nul";
#else
    string comando = "mkdir -p \"" + pasta + "\" 2>/dev/null";
#endif
    int retorno = system(comando.c_str());
    (void)retorno;
}

static bool abrirParaEscrita(ofstream& arquivo, const string& caminho, ios::openmode modo = ios::out) {
    arquivo.open(caminho.c_str(), modo);
    if (!arquivo.is_open()) {
        criarPastaDados();
        arquivo.clear();
        arquivo.open(caminho.c_str(), modo);
    }
    return arquivo.is_open();
}

// Grava primeiro em um arquivo temporário e só depois substitui o original:
// se algo falhar no meio da gravação, o arquivo anterior não é corrompido.
static bool substituirArquivo(const string& temporario, const string& definitivo) {
    remove(definitivo.c_str());
    return rename(temporario.c_str(), definitivo.c_str()) == 0;
}

vector<string> dividirCSV(const string& linha, char separador) {
    vector<string> campos;
    string atual;
    for (size_t i = 0; i < linha.size(); ++i) {
        if (linha[i] == separador) { campos.push_back(atual); atual.clear(); }
        else if (linha[i] != '\r') atual += linha[i];
    }
    campos.push_back(atual);
    return campos;
}

// Conversão segura de texto para número (retorna false se inválido)
static bool converterInteiro(const string& texto, long& valor) {
    if (texto.empty()) return false;
    try {
        size_t lidos = 0;
        valor = stol(texto, &lidos);
        return lidos == texto.size();
    } catch (...) {
        return false;
    }
}

// ============================================================================
//  OCORRÊNCIAS
// ============================================================================
static const char* CABECALHO_OCORRENCIAS =
    "numero;tipo;bairro;endereco;descricao;vitimas;detalhes;prioridade;manual;dataHora;status;viatura;sequencia";

bool salvarOcorrencias(const ListaOcorrencias& lista) {
    string caminho = caminhoDados("ocorrencias.csv");
    ofstream arquivo;
    if (!abrirParaEscrita(arquivo, caminho + ".tmp")) return false;

    arquivo << CABECALHO_OCORRENCIAS << "\n";
    for (NoOcorrencia* no = lista.inicio; no != nullptr; no = no->proximo) {
        const Ocorrencia& o = no->dados;
        arquivo << o.numero << ';' << o.tipo << ';' << o.bairro << ';' << o.endereco << ';' << o.descricao << ';'
                << (o.haVitimas ? 1 : 0) << ';' << o.detalhes << ';' << o.prioridade << ';'
                << (o.classificacaoManual ? 1 : 0) << ';' << o.dataHora << ';' << o.status << ';'
                << o.viatura << ';' << o.sequenciaFila << "\n";
    }
    arquivo.close();
    if (arquivo.fail()) return false;
    return substituirArquivo(caminho + ".tmp", caminho);
}

static bool porSequencia(const Ocorrencia* a, const Ocorrencia* b) {
    return a->sequenciaFila < b->sequenciaFila;
}

// Retorna a quantidade de registros carregados (-1 se o arquivo não existir)
int carregarOcorrencias(ListaOcorrencias& lista, FilasPrioridade& filas) {
    ifstream arquivo(caminhoDados("ocorrencias.csv").c_str());
    if (!arquivo.is_open()) return -1;

    string linha;
    int carregadas = 0, ignoradas = 0;
    vector<Ocorrencia*> aguardando;   // recolocadas nas filas depois, na ordem original

    while (getline(arquivo, linha)) {
        if (aparar(linha).empty() || linha.compare(0, 6, "numero") == 0) continue;
        vector<string> c = dividirCSV(linha);
        long numero, tipo, vitimas, prioridade, manual, status, sequencia;
        bool valido = c.size() == 13 &&
                      converterInteiro(c[0], numero) && numero > 0 &&
                      converterInteiro(c[1], tipo) && tipo >= 1 && tipo <= TOTAL_TIPOS &&
                      converterInteiro(c[5], vitimas) &&
                      converterInteiro(c[7], prioridade) && prioridade >= 1 && prioridade <= 4 &&
                      converterInteiro(c[8], manual) &&
                      converterInteiro(c[10], status) && status >= AGUARDANDO && status <= ENCERRADA &&
                      converterInteiro(c[12], sequencia);
        if (!valido || buscarOcorrencia(lista, static_cast<int>(numero)) != nullptr) {
            ++ignoradas;
            continue;
        }

        Ocorrencia o;
        o.numero = static_cast<int>(numero);
        o.tipo = static_cast<int>(tipo);
        o.bairro = c[2];
        o.endereco = c[3];
        o.descricao = c[4];
        o.haVitimas = vitimas != 0;
        o.detalhes = c[6];
        o.prioridade = static_cast<int>(prioridade);
        o.classificacaoManual = manual != 0;
        o.dataHora = c[9];
        o.status = static_cast<int>(status);
        o.viatura = c[11];
        o.sequenciaFila = sequencia;

        Ocorrencia* inserida = inserirOcorrenciaNoFim(lista, o);
        if (inserida->status == AGUARDANDO) aguardando.push_back(inserida);
        ++carregadas;
    }

    // Reconstrói as quatro filas respeitando a ordem de entrada original (FIFO)
    stable_sort(aguardando.begin(), aguardando.end(), porSequencia);
    for (size_t i = 0; i < aguardando.size(); ++i) restaurarNaFila(filas, aguardando[i]);

    if (ignoradas > 0)
        cout << "  [AVISO] " << ignoradas << " linha(s) inválida(s) ignorada(s) em ocorrencias.csv\n";
    return carregadas;
}

// ============================================================================
//  VIATURAS
// ============================================================================
bool salvarViaturas(const ListaViaturas& viaturas) {
    string caminho = caminhoDados("viaturas.csv");
    ofstream arquivo;
    if (!abrirParaEscrita(arquivo, caminho + ".tmp")) return false;
    arquivo << "prefixo;tipo;placa;status;ocorrencia\n";
    for (ListaViaturas::const_iterator it = viaturas.begin(); it != viaturas.end(); ++it)
        arquivo << it->prefixo << ';' << it->tipo << ';' << it->placa << ';' << it->status << ';'
                << it->numeroOcorrencia << "\n";
    arquivo.close();
    if (arquivo.fail()) return false;
    return substituirArquivo(caminho + ".tmp", caminho);
}

int carregarViaturas(ListaViaturas& viaturas) {
    ifstream arquivo(caminhoDados("viaturas.csv").c_str());
    if (!arquivo.is_open()) return -1;
    string linha;
    int carregadas = 0, ignoradas = 0;
    while (getline(arquivo, linha)) {
        if (aparar(linha).empty() || linha.compare(0, 7, "prefixo") == 0) continue;
        vector<string> c = dividirCSV(linha);
        long status, ocorrencia;
        if (c.size() != 5 || c[0].empty() || !converterInteiro(c[3], status) ||
            status < VIATURA_DISPONIVEL || status > VIATURA_EM_MANUTENCAO ||
            !converterInteiro(c[4], ocorrencia) || buscarViatura(viaturas, c[0]) != nullptr) {
            ++ignoradas;
            continue;
        }
        Viatura v;
        v.prefixo = c[0];
        v.tipo = c[1];
        v.placa = c[2];
        v.status = static_cast<int>(status);
        v.numeroOcorrencia = static_cast<int>(ocorrencia);
        viaturas.push_back(v);
        ++carregadas;
    }
    if (ignoradas > 0)
        cout << "  [AVISO] " << ignoradas << " linha(s) inválida(s) ignorada(s) em viaturas.csv\n";
    return carregadas;
}

// ============================================================================
//  BOMBEIROS
// ============================================================================
bool salvarBombeiros(const ListaBombeiros& bombeiros) {
    string caminho = caminhoDados("bombeiros.csv");
    ofstream arquivo;
    if (!abrirParaEscrita(arquivo, caminho + ".tmp")) return false;
    arquivo << "matricula;nome;posto;viatura\n";
    for (ListaBombeiros::const_iterator it = bombeiros.begin(); it != bombeiros.end(); ++it)
        arquivo << it->matricula << ';' << it->nome << ';' << it->posto << ';' << it->prefixoViatura << "\n";
    arquivo.close();
    if (arquivo.fail()) return false;
    return substituirArquivo(caminho + ".tmp", caminho);
}

int carregarBombeiros(ListaBombeiros& bombeiros) {
    ifstream arquivo(caminhoDados("bombeiros.csv").c_str());
    if (!arquivo.is_open()) return -1;
    string linha;
    int carregados = 0, ignorados = 0;
    while (getline(arquivo, linha)) {
        if (aparar(linha).empty() || linha.compare(0, 9, "matricula") == 0) continue;
        vector<string> c = dividirCSV(linha);
        if (c.size() != 4 || c[0].empty() || buscarBombeiro(bombeiros, c[0]) != nullptr) {
            ++ignorados;
            continue;
        }
        Bombeiro b;
        b.matricula = c[0];
        b.nome = c[1];
        b.posto = c[2];
        b.prefixoViatura = c[3];
        bombeiros.push_back(b);
        ++carregados;
    }
    if (ignorados > 0)
        cout << "  [AVISO] " << ignorados << " linha(s) inválida(s) ignorada(s) em bombeiros.csv\n";
    return carregados;
}

// ============================================================================
//  OPERAÇÕES EM CONJUNTO
// ============================================================================
bool salvarTodos(const ListaOcorrencias& lista, const ListaViaturas& viaturas,
                 const ListaBombeiros& bombeiros, bool exibirMensagens) {
    bool okOcorrencias = salvarOcorrencias(lista);
    bool okViaturas = salvarViaturas(viaturas);
    bool okBombeiros = salvarBombeiros(bombeiros);
    bool sucesso = okOcorrencias && okViaturas && okBombeiros;

    if (!sucesso)
        cout << "  [ERRO] Falha ao salvar:" << (okOcorrencias ? "" : " ocorrências") << (okViaturas ? "" : " viaturas")
             << (okBombeiros ? "" : " bombeiros") << ". Verifique a pasta \"" << pastaDados << "\".\n";
    else if (exibirMensagens)
        cout << "  [OK] Dados salvos em \"" << pastaDados << "\": " << lista.tamanho << " ocorrência(s), "
             << viaturas.size() << " viatura(s), " << bombeiros.size() << " bombeiro(s).\n";
    return sucesso;
}

// Corrige referências quebradas entre os arquivos (ex.: CSV editado à mão)
static void verificarConsistencia(ListaOcorrencias& lista, ListaViaturas& viaturas, ListaBombeiros& bombeiros) {
    for (ListaViaturas::iterator v = viaturas.begin(); v != viaturas.end(); ++v) {
        if (v->status != VIATURA_EM_OCORRENCIA) continue;
        Ocorrencia* o = buscarOcorrencia(lista, v->numeroOcorrencia);
        if (o == nullptr || o->status != EM_ATENDIMENTO || o->viatura != v->prefixo) {
            cout << "  [AVISO] Viatura " << v->prefixo << " estava em ocorrência inválida e foi liberada.\n";
            v->status = VIATURA_DISPONIVEL;
            v->numeroOcorrencia = 0;
        }
    }
    for (ListaBombeiros::iterator b = bombeiros.begin(); b != bombeiros.end(); ++b) {
        if (!b->prefixoViatura.empty() && buscarViatura(viaturas, b->prefixoViatura) == nullptr) {
            cout << "  [AVISO] Bombeiro " << b->matricula << " estava vinculado a viatura inexistente.\n";
            b->prefixoViatura = "";
        }
    }
}

void carregarTodos(ListaOcorrencias& lista, FilasPrioridade& filas, ListaViaturas& viaturas,
                   ListaBombeiros& bombeiros) {
    int nOcorrencias = carregarOcorrencias(lista, filas);
    int nViaturas = carregarViaturas(viaturas);
    int nBombeiros = carregarBombeiros(bombeiros);
    verificarConsistencia(lista, viaturas, bombeiros);

    if (nOcorrencias < 0 && nViaturas < 0 && nBombeiros < 0) {
        cout << "  [INFO] Nenhum arquivo de dados encontrado. Iniciando com o sistema vazio.\n";
        return;
    }
    cout << "  [OK] Dados carregados: " << max(nOcorrencias, 0) << " ocorrência(s), " << max(nViaturas, 0)
         << " viatura(s), " << max(nBombeiros, 0) << " bombeiro(s). Aguardando despacho: "
         << totalAguardando(filas) << ".\n";
}

// ============================================================================
//  LOG PERMANENTE (complementa a pilha, que existe apenas durante a sessão)
// ============================================================================
void anexarLog(const Operacao& operacao) {
    ofstream arquivo;
    if (!abrirParaEscrita(arquivo, caminhoDados("log_chamados.txt"), ios::out | ios::app)) return;
    arquivo << "[" << operacao.dataHora << "] " << nomeOperacao(operacao.tipo) << " | " << operacao.descricao << "\n";
}

// ============================================================================
//  PARTE 7 - MENU PRINCIPAL
// ============================================================================

static void exibirMenuPrincipal() {
    cout << "\n========================================\n"
         << "               FOGOZERO\n"
         << "   SISTEMA DE CONTROLE DE BOMBEIROS\n"
         << "========================================\n\n"
         << "1. Registrar ocorrência\n"
         << "2. Consultar ocorrências\n"
         << "3. Visualizar filas de prioridade\n"
         << "4. Despachar ocorrência\n"
         << "5. Gerenciar viaturas\n"
         << "6. Gerenciar bombeiros\n"
         << "7. Histórico de operações\n"
         << "8. Relatórios\n"
         << "9. Salvar dados\n"
         << "0. Sair\n\n";
}

// Submenu 2: consultas e gerenciamento de ocorrências
static void menuOcorrencias(ListaOcorrencias& lista, FilasPrioridade& filas, ListaViaturas& viaturas,
                            Pilha& historico) {
    while (true) {
        imprimirTitulo("CONSULTAR OCORRÊNCIAS");
        cout << "  1 - Listar todas as ocorrências\n"
             << "  2 - Consultar ocorrência pelo código\n"
             << "  3 - Buscar por tipo\n"
             << "  4 - Buscar por bairro\n"
             << "  5 - Ocorrências em aberto (aguardando despacho)\n"
             << "  6 - Ocorrências em atendimento\n"
             << "  7 - Ocorrências encerradas\n"
             << "  8 - Alterar prioridade de uma ocorrência\n"
             << "  9 - Encerrar ocorrência\n"
             << "  0 - Voltar\n";
        int opcao = lerInteiro("Escolha uma opção: ", 0, 9);
        if (opcao == 0) return;
        switch (opcao) {
            case 1: listarTodasOcorrencias(lista); break;
            case 2: consultarPorCodigo(lista, filas); break;
            case 3: buscarPorTipo(lista); break;
            case 4: buscarPorBairro(lista); break;
            case 5: listarPorStatus(lista, AGUARDANDO); break;
            case 6: listarPorStatus(lista, EM_ATENDIMENTO); break;
            case 7: listarPorStatus(lista, ENCERRADA); break;
            case 8: menuAlterarPrioridade(lista, filas, historico); break;
            case 9: menuEncerrarOcorrencia(lista, filas, viaturas, historico); break;
        }
        pausar();
    }
}

int main() {
#ifdef _WIN32
    // Ativa UTF-8 no console do Windows para exibir corretamente os acentos
    int retorno = system("chcp 65001 > nul");
    (void)retorno;
#endif

    // Estruturas principais (passadas por referência a todos os módulos)
    ListaOcorrencias ocorrencias;
    FilasPrioridade filas;
    Pilha historico;
    ListaViaturas viaturas;
    ListaBombeiros bombeiros;

    inicializarLista(ocorrencias);
    inicializarFilas(filas);
    inicializarPilha(historico);

    cout << "Iniciando FogoZero...\n";
    carregarTodos(ocorrencias, filas, viaturas, bombeiros);

    try {
        bool executando = true;
        while (executando) {
            exibirMenuPrincipal();
            int opcao = lerInteiro("Escolha uma opção: ", 0, 9);
            switch (opcao) {
                case 1: menuRegistrarOcorrencia(ocorrencias, filas, historico); pausar(); break;
                case 2: menuOcorrencias(ocorrencias, filas, viaturas, historico); break;
                case 3: menuFilas(ocorrencias, filas); break;
                case 4: menuDespachar(ocorrencias, filas, viaturas, bombeiros, historico); pausar(); break;
                case 5: menuViaturas(ocorrencias, filas, viaturas, bombeiros, historico); break;
                case 6: menuBombeiros(viaturas, bombeiros, historico); break;
                case 7: menuHistorico(ocorrencias, filas, historico); break;
                case 8: menuRelatorios(ocorrencias, filas, viaturas, bombeiros); break;
                case 9: salvarTodos(ocorrencias, viaturas, bombeiros, true); pausar(); break;
                case 0: executando = false; break;
            }
            // Salvamento automático após cada operação: nada se perde se a janela for fechada
            if (executando) salvarTodos(ocorrencias, viaturas, bombeiros, false);
        }
    } catch (const FimDeEntrada&) {
        cout << "\n\nEntrada encerrada.\n";
    }

    cout << "\nSalvando dados antes de sair...\n";
    salvarTodos(ocorrencias, viaturas, bombeiros, true);

    // Libera toda a memória alocada dinamicamente (std::list se libera sozinha)
    liberarFilas(filas);
    liberarLista(ocorrencias);
    liberarPilha(historico);

    cout << "FogoZero encerrado. Bom serviço!\n";
    return 0;
}
