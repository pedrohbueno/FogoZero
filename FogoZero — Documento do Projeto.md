# FogoZero — Sistema de Controle de Bombeiros

Oct 26, 2026 · @Pedro Bueno

## Visão geral

O FogoZero é um sistema de console em C++ que recebe chamados de emergência, organiza os chamados por gravidade e despacha viaturas e equipes do corpo de bombeiros. Todos os dados são fictícios.

- **Problema:** vários chamados chegam ao mesmo tempo. O mais grave precisa sair primeiro, e só pode sair uma viatura disponível e com equipe mínima.
- **Objetivo acadêmico:** aplicar lista simplesmente encadeada, lista duplamente encadeada, fila e pilha, usando `struct`, ponteiros, passagem por referência e `std::list`.
- **Fora do escopo:** interface gráfica, mapa, GPS e comunicação em rede. Isso mantém o projeto simples e focado nas estruturas de dados.

## Escopo ampliado

O escopo original foi mantido e cada módulo ganhou uma ou duas funções pequenas. Também entraram três módulos novos (Despacho, Relatórios e Persistência/Menu), que fecham os itens cobrados na avaliação.

| Módulo | Responsável | Funcionalidades | Ampliação |
| --- | --- | --- | --- |
| Registro de ocorrências | Camilli | Cadastrar (tipo, bairro, endereço, prioridade), buscar por código, tipo ou bairro, atualizar status, encerrar, listar em ordem direta e inversa | Código automático (OC-0001) e hora de abertura e encerramento |
| Prioridade de filas | Camilli | Três filas (Crítica, Alta, Normal), inserir pela prioridade, mostrar o tamanho de cada fila | Consultar a posição de uma ocorrência na fila e reclassificar a prioridade |
| Sistema de histórico | Camilli | Pilha com as últimas operações (data, hora e descrição) | Desfazer o último cadastro e exibir as 10 últimas operações |
| Leitura de logs | Pedro Bueno | Ler `log_chamados.txt` linha a linha, cadastrar e enfileirar | Validar as linhas e mostrar quantas foram importadas e rejeitadas |
| Controle de viaturas | Pedro Bueno | Cadastrar (prefixo, tipo, capacidade), status Disponível / Em ocorrência / Manutenção, buscar por prefixo, listar por status | Registrar retorno à base e envio para manutenção |
| Controle de funcionários | Pedro Bueno | Cadastrar bombeiro (matrícula, nome, função, turno), buscar, listar por turno | Vincular bombeiro a uma viatura e exigir equipe mínima de 2 para o despacho |
| Despacho (novo) | Camilli e Pedro | Retirar a ocorrência mais grave, escolher a viatura livre, atualizar status e registrar no histórico | Liga todos os módulos |
| Relatórios (novo) | Membro 3 (ou Camilli) | Ocorrências por tipo e por bairro, ocorrências em aberto, viaturas mais acionadas, tempo médio de espera | Atualizados durante a execução |
| Persistência e menu (novo) | Membro 4 (ou Pedro) | Salvar e carregar CSV ao sair e ao abrir, menu principal com submenus | Atende ao item de criatividade e extensões |

Se a equipe tiver só duas pessoas, os módulos novos ficam com o responsável indicado entre parênteses.

## Tecnologias

O projeto usa só a biblioteca padrão do C++, sem nenhuma instalação extra, então roda em qualquer computador da equipe.

| Item | Escolha | Por quê |
| --- | --- | --- |
| Linguagem | C++ (C++11 ou superior) | Exigência do trabalho; suporta `struct`, ponteiros e `std::list` |
| Compilador | g++ (MinGW no Windows, GCC no Linux) | Gratuito e igual para todos |
| Editor | VS Code com extensão C/C++ ou Code::Blocks | Leves e com depuração |
| Bibliotecas | `iostream`, `fstream`, `sstream`, `string`, `list`, `ctime`, `iomanip` | Entrada e saída, arquivos, texto, listas, horário e formatação de tabelas |
| Versionamento | Git e GitHub | Cada membro trabalha no seu módulo sem sobrescrever o outro |
| Quadro de tarefas | GitHub Projects ou Trello | Kanban da seção abaixo |
| Dados de entrada | `log_chamados.txt` | Simula os chamados do dia |
| Persistência | Arquivos CSV | Abrem no Excel, o que facilita mostrar na apresentação |
| Interface | Menu de console com submenus | Simples e atende ao item de interface |

Para compilar: `g++ -std=c++11 *.cpp -o fogozero`

## Estruturas de dados e justificativa

O sistema usa as quatro estruturas da disciplina. As de ocorrências são feitas à mão, com ponteiros, para mostrar domínio do conteúdo. Os cadastros usam `std::list`, o que mostra uso da biblioteca.

| Estrutura | Onde é usada | Implementação | Justificativa |
| --- | --- | --- | --- |
| Lista duplamente encadeada | Registro de ocorrências | Manual, com ponteiros `prox` e `ant` | Permite remover do meio e listar do mais antigo ao mais recente e ao contrário |
| Fila (3 instâncias) | Prioridade: Crítica, Alta, Normal | Manual, com ponteiros `inicio` e `fim` | Dentro de cada nível vale a ordem de chegada (FIFO); o despacho esvazia primeiro a fila mais grave |
| Pilha | Histórico de operações | Manual, com ponteiro `topo` | A operação mais recente fica no topo, o que permite exibir o histórico recente e desfazer a última ação |
| Lista simplesmente encadeada | Viaturas | `std::list<Viatura>` | Cadastro sequencial com busca por prefixo e filtro por status |
| Lista simplesmente encadeada | Bombeiros | `std::list<Bombeiro>` | Cadastro com busca por matrícula e por turno |

Todas as funções que alteram uma estrutura recebem o parâmetro por referência, por exemplo `void enfileirar(Fila &f, Ocorrencia *oc)`.

## Modelo de dados e arquivos

São seis structs, uma para cada conceito do sistema. Os campos são poucos de propósito, para manter o código legível.

```cpp
struct Ocorrencia {
    int id;                 // gerado automaticamente
    string tipo;            // Incendio, Resgate, Vazamento, Arvore
    string bairro, endereco; // ficticios
    int prioridade;         // 1 = Critica, 2 = Alta, 3 = Normal
    string status;          // Aguardando, Em atendimento, Encerrada
    string viatura;         // prefixo da viatura despachada
    time_t abertura, encerramento;
    Ocorrencia *prox, *ant; // lista duplamente encadeada
};

struct NoFila   { Ocorrencia *oc; NoFila *prox; };
struct Fila     { NoFila *inicio = nullptr, *fim = nullptr; int tam = 0; };

struct Operacao { string dataHora, descricao; Operacao *prox; };
struct Pilha    { Operacao *topo = nullptr; };

struct Viatura  { string prefixo, tipo, status; int capacidade; int acionamentos; };
struct Bombeiro { int matricula; string nome, funcao, turno, viatura; };
```

**Organização das pastas**

```text
fogozero/
  main.cpp              menu principal e submenus
  ocorrencias.h / .cpp  lista dupla, filas, pilha, despacho
  recursos.h / .cpp     viaturas, bombeiros, leitura de log
  relatorios.h / .cpp   estatísticas
  arquivos.h / .cpp     salvar e carregar CSV
  dados/
    log_chamados.txt
    ocorrencias.csv  viaturas.csv  bombeiros.csv
  README.md
```

**Formato do log** (um chamado por linha, separado por ponto e vírgula)

```text
hora;tipo;bairro;endereco;prioridade
08:15;Incendio;Centro;Rua das Flores 100;1
08:40;Arvore;Jardim Azul;Av. Brasil 250;3
```

## Fluxo de despacho e menu

O despacho liga todos os módulos: a ocorrência sai da fila mais grave e só é atendida se houver viatura livre com pelo menos 2 bombeiros.

&#91;embedded content: fluxo de despacho · 8 etapas, 1 decisão\]

Se não houver viatura, a ocorrência não se perde. Ela volta ao início da sua fila e é a próxima a sair quando uma viatura for liberada.

**Menu principal**

1. Ocorrências: cadastrar, buscar, atualizar status, encerrar, listar (direta e inversa)
2. Filas: ver filas, posição de uma ocorrência, reclassificar prioridade
3. Despachar próxima ocorrência
4. Viaturas: cadastrar, buscar, listar por status, retorno à base, manutenção
5. Bombeiros: cadastrar, buscar, listar por turno, vincular a viatura
6. Importar log de chamados
7. Histórico: últimas 10 operações, desfazer
8. Relatórios
9. Salvar e sair

## Metodologia de desenvolvimento

A equipe trabalha em ciclos semanais, de forma incremental e com Kanban. Cada membro entrega o seu módulo testado antes da integração.

1. **Incremental e modular:** cada módulo é feito e testado sozinho, com um `main` de teste, antes de entrar no `main.cpp` final.
2. **Kanban:** as tarefas passam pelas colunas A fazer, Fazendo, Testando e Pronto. Cada pessoa tem no máximo 2 tarefas em Fazendo ao mesmo tempo.
3. **Git:** um branch por membro (`camilli`, `pedro`), com merge na `main` uma vez por semana. As mensagens de commit dizem o que mudou, por exemplo "adiciona fila de prioridade".
4. **Revisão em par:** antes de uma tarefa ir para Pronto, outro membro roda os testes dela.
5. **Reunião semanal curta:** 15 minutos para revisar o quadro e redistribuir o que atrasou.

## Kanban do projeto

O quadro tem 17 tarefas em 4 semanas. Para mover um cartão, basta trocar o valor da coluna **Coluna** na tabela. O mesmo quadro pode ser copiado para o GitHub Projects ou o Trello.

| # | Tarefa | Responsável | Semana | Coluna |
| --- | --- | --- | --- | --- |
| 1 | Definir escopo e tecnologias | Equipe | 1 | Pronto |
| 2 | Criar repositório no GitHub e estrutura de pastas | Pedro Bueno | 1 | A fazer |
| 3 | Escrever as structs e os arquivos .h | Equipe | 1 | A fazer |
| 4 | Lista duplamente encadeada de ocorrências | Camilli | 1 | A fazer |
| 5 | Cadastro de viaturas com std::list | Pedro Bueno | 1 | A fazer |
| 6 | Filas de prioridade (Crítica, Alta, Normal) | Camilli | 2 | A fazer |
| 7 | Pilha de histórico e desfazer | Camilli | 2 | A fazer |
| 8 | Cadastro de bombeiros e vínculo com viatura | Pedro Bueno | 2 | A fazer |
| 9 | Leitura do log\_chamados.txt | Pedro Bueno | 2 | A fazer |
| 10 | Massa de dados fictícios (log e CSVs) | Membro 4 ou Pedro | 2 | A fazer |
| 11 | Função de despacho (integração) | Camilli e Pedro | 3 | A fazer |
| 12 | Persistência em CSV | Membro 4 ou Pedro | 3 | A fazer |
| 13 | Relatórios e estatísticas | Membro 3 ou Camilli | 3 | A fazer |
| 14 | Menu principal e submenus | Membro 3 ou Camilli | 3 | A fazer |
| 15 | Executar o plano de testes | Equipe | 4 | A fazer |
| 16 | README com justificativa das estruturas | Equipe | 4 | A fazer |
| 17 | Ensaiar a apresentação | Equipe | 4 | A fazer |

## Plano de testes

São 10 casos, todos com dados fictícios. Eles cobrem o funcionamento normal e os erros mais comuns em estruturas encadeadas.

| # | Caso | Entrada | Resultado esperado |
| --- | --- | --- | --- |
| 1 | Despacho com filas vazias | Opção Despachar sem chamados | Mensagem "Nenhuma ocorrência aguardando", sem travar |
| 2 | Ordem de prioridade | 1 Normal, depois 1 Crítica | A Crítica sai primeiro |
| 3 | Ordem de chegada | 2 ocorrências Alta | A primeira cadastrada sai primeiro |
| 4 | Sem viatura livre | Todas as viaturas Em ocorrência | A ocorrência volta ao início da sua fila |
| 5 | Equipe incompleta | Viatura com 1 bombeiro | A viatura não é despachada |
| 6 | Busca inexistente | Código OC-9999 ou prefixo inválido | Mensagem "não encontrado" |
| 7 | Log com linha errada | Linha sem prioridade | Linha rejeitada e contada no resumo da importação |
| 8 | Desfazer com pilha vazia | Opção Desfazer logo ao abrir | Mensagem "Nada para desfazer" |
| 9 | Listagem inversa | 3 ocorrências cadastradas | A lista aparece da mais recente para a mais antiga |
| 10 | Persistência | Cadastrar, sair e abrir de novo | Os dados aparecem carregados dos CSVs |

## Relação com os critérios de avaliação

Cada item da avaliação tem pelo menos uma parte do FogoZero que responde a ele diretamente.

| Critério | Peso | Como o FogoZero atende |
| --- | --- | --- |
| Estrutura de dados e funcionamento | 2,0 | Lista dupla, três filas, pilha e `std::list`, cada uma com justificativa própria |
| Funcionalidades implementadas | 1,0 | Cadastro, busca, listagem, despacho, encerramento e desfazer |
| Apresentação e arguição | 3,0 | Este documento, o README, a demonstração com o log fictício e os 10 casos de teste |
| Criatividade e extensões | 1,0 | Leitura de log, persistência em CSV, menu com submenus, equipe mínima e relatórios em tempo real |
