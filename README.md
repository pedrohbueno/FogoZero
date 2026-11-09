# FogoZero — Sistema de Controle de Ocorrências do Corpo de Bombeiros

Sistema de console em C++11 (somente biblioteca padrão) para registrar, classificar,
enfileirar e despachar ocorrências de emergência.

## Estrutura

```
fogozero/
├── main.cpp      Todo o sistema em um único arquivo, dividido em 7 partes
└── dados/        ocorrencias.csv, viaturas.csv, bombeiros.csv, log_chamados.txt
```

## Compilar e executar

```bash
cd fogozero
g++ -std=c++11 main.cpp -o fogozero
./fogozero            # Windows: fogozero.exe
```

Execute sempre **de dentro da pasta `fogozero/`**, para que a pasta `dados/` seja encontrada.
No Windows, o programa ativa UTF-8 no console (`chcp 65001`) para exibir os acentos.

## Estruturas de dados

| Estrutura | Onde | Uso |
|---|---|---|
| Lista duplamente encadeada | `ListaOcorrencias` | Todas as ocorrências, em ordem de registro |
| 4 filas encadeadas (FIFO) | `FilasPrioridade` | Crítica, Alta, Média, Baixa |
| Lista simplesmente encadeada | `ListaResultado` | Resultados das buscas por tipo, bairro e status |
| Pilha encadeada | `Pilha` | Histórico de operações e "desfazer" (com pilha auxiliar) |
| `std::list` | `ListaViaturas`, `ListaBombeiros` | Cadastro de viaturas e bombeiros |

## Regras de classificação adotadas

| Situação | Prioridade |
|---|---|
| Batida **com** vítima | 1 – Crítica (regra adotada para a inconsistência da tabela) |
| Batida **sem** vítima | 4 – Baixa |
| Incêndio com pessoas presas (qualquer local) | 1 – Crítica |
| Incêndio em prédio sem vítima | 2 – Alta |
| Fogo em vegetação sem vítima | 3 – Média |
| Parada cardíaca / afogamento com vítima | 1 – Crítica |
| Vazamento de gás sem vítima | 2 – Alta |
| Enchente sem vítima | 3 – Média |
| Árvore caída, animal preso, abelhas sem vítima | 4 – Baixa |
| **Fora da tabela** → classificação manual pelo operador | sufocamento; incêndio com vítima não presa; incêndio em "outro local"; gás/enchente/árvore/animal/abelhas com vítima; parada ou afogamento sem vítima confirmada |

## Comportamentos importantes

- **Despacho:** a ocorrência só sai da fila depois de confirmadas a viatura disponível e a equipe mínima
  de 2 bombeiros. Se faltar algo, ela continua na frente da fila original.
- **Retorno à base / encerramento:** encerrar uma ocorrência em atendimento libera a viatura, e registrar o
  retorno da viatura encerra a ocorrência. Uma ocorrência ainda na fila pode ser encerrada sem atendimento
  (trote ou cancelamento).
- **Alteração de prioridade:** a ocorrência vai para o fim da nova fila e passa a constar como manual.
- **Desfazer:** remove o último cadastro de ocorrência da sessão, apenas se ela ainda estiver aguardando.
- **Persistência:** os dados são carregados ao iniciar, salvos automaticamente após cada operação do menu
  e também ao sair. O caractere `;` digitado pelo usuário é trocado por `,` para não corromper o CSV.
