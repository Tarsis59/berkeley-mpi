# Trabalho 2 – Algoritmo de Berkeley com MPI

> **Enunciado:** Programar o algoritmo de Berkeley de sincronização de relógios usando MPI para uma quantidade qualquer de processos.

Implementação em **C + MPI** (`berkeley.c`). Funciona com **qualquer número de processos** (testado com N = 1, 2, 3, 4, 6, 8, 12, 16, 32 e 64). A comunicação do algoritmo usa **somente `MPI_Send` e `MPI_Recv`**, no modelo **Mestre‑Escravo** visto em aula.

## 1. O algoritmo de Berkeley

O algoritmo foi proposto por Gusella e Zatti (Berkeley UNIX, 1989). Ele sincroniza os relógios de um grupo de máquinas **sem fonte externa de tempo** (UTC/NTP). O objetivo é que todos **concordem** sobre um horário comum, e não que acertem a hora "real".

| Passo | O que acontece | Implementação |
|---|---|---|
| 1. Coleta (*polling*) | O coordenador (*time daemon*) pergunta a hora a cada processo | `MPI_Send(TAG_REQ)` → escravo responde com `MPI_Send(TAG_RESP)` |
| 2. Compensação da rede | O coordenador mede o RTT e estima o relógio do escravo como `leitura + RTT/2` (Cristian) | Ele faz 3 leituras e fica com a de **menor RTT**, a mais precisa |
| 3. Média tolerante a falhas | Calcula a diferença Δᵢ de cada relógio para o seu (Δ do coordenador = 0). A média só usa os relógios com \|Δᵢ − mediana\| ≤ tolerância | Um relógio defeituoso não "puxa" a média |
| 4. Ajuste | Envia a **cada** processo a sua correção `ajusteᵢ = média − Δᵢ` (positiva ou negativa) e corrige o próprio relógio | `MPI_Send(TAG_AJUSTE)`. Os processos descartados da média também são corrigidos |
| 5. Verificação | Lê de novo todos os relógios e mostra a dispersão (maior − menor) antes e depois | Critério de sucesso impresso no fim |

**Por que enviar a correção, e não a hora?** A correção é um valor relativo. Ela continua certa mesmo que a mensagem demore para chegar, enquanto um horário absoluto já estaria atrasado ao ser recebido.

### Exemplo clássico (Tanenbaum)

O coordenador marca 3:00 e os outros marcam 3:25 e 2:50. As diferenças são 0, +25 e −10 min, e a média é **+5 min**. Os ajustes ficam +5, −20 e +15, e todos passam a marcar **3:05**. O programa reproduz esse exemplo com a opção `-e`:

```
rank  relogio lido  diferenca     RTT(ms)  media? ajuste        dif. depois
0     03:00:00.000  +0.000s       0.000    sim    +5m00.000s    +0.000s   <- coordenador
1     03:25:00.000  +25m00.000s   0.010    sim    -20m00.000s   +0.000s
2     02:50:00.000  -10m00.000s   0.004    sim    +15m00.000s   +0.000s
Media tolerante a falhas: +5m00.000s (3 de 3 relogios usados)
Horario comum apos o ajuste: 03:05:00.000
```

## 2. Arquitetura (Mestre‑Escravo)

```
                    TAG_REQ  ──────────►  Escravo 1
                    ◄──────  TAG_RESP
   Coordenador      TAG_AJUSTE ────────►
  (rank 0, padrão)        ...                ...
                    TAG_REQ  ──────────►  Escravo N-1
                    ◄──────  TAG_RESP
                    TAG_AJUSTE ────────►
                    TAG_FIM  ──────────►  (todos, no fim)
```

* `MPI_Comm_size` descobre N em tempo de execução. Não há nenhum número fixo de processos no código.
* O escravo fica num laço `MPI_Recv(MPI_ANY_TAG)` e responde conforme a *tag* da mensagem recebida.
* O coordenador pode ser qualquer rank (opção `-c`).

### Relógios simulados

Em um único computador todos os processos compartilham o mesmo relógio físico. Por isso cada processo tem um **relógio simulado**:

```
relogio(t) = 12:00:00 + offset + (MPI_Wtime() − t0) · (1 + drift)
```

* `offset`: desvio inicial aleatório (±60 s por padrão).
* `drift`: deriva aleatória (±0,5 % por padrão). Ela faz os relógios se afastarem de novo com o tempo, e por isso o algoritmo roda em **várias rodadas**.
* Os atrasos de rede são simulados, com ida e volta diferentes. Assim a estimativa RTT/2 tem um pequeno erro, como numa rede real.
* O gerador aleatório é próprio e usa uma semente por rank, então os resultados são **reprodutíveis** em qualquer sistema.

## 3. Como compilar e executar

### Windows (MS-MPI + gcc)

```bat
winget install Microsoft.msmpi
winget install Microsoft.msmpisdk
winget install BrechtSanders.WinLibs.POSIX.UCRT

build.bat
mpiexec -n 5 berkeley.exe
executar.bat 8          :: demonstração completa com 8 processos
```

> Se o Windows bloquear o `berkeley.exe` com "Uma política de Controle de Aplicativo bloqueou este arquivo", a causa é o *Smart App Control*, que bloqueia executáveis recém-compilados sem assinatura. Nesse caso use WSL ou Docker (abaixo).

### Linux / WSL (OpenMPI ou MPICH)

```bash
sudo apt install openmpi-bin libopenmpi-dev gcc make
make                    # compila
make run N=8            # 8 processos
make exemplo            # exemplo do livro (3:00, 3:25, 2:50 -> 3:05)
make falha              # 6 processos, rank 3 com relógio defeituoso
```

### Docker (sem instalar nada)

```bash
docker run --rm -it -v "$PWD:/src" -w /src ubuntu:24.04 bash -c \
 "apt update && apt install -y openmpi-bin libopenmpi-dev gcc make && \
  export OMPI_ALLOW_RUN_AS_ROOT=1 OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1 && make run N=10"
```

### Opções

| Opção | Significado | Padrão |
|---|---|---|
| `-r <int>` | número de rodadas | 3 |
| `-o <seg>` | desvio inicial máximo dos relógios | 60 |
| `-D <frac>` | deriva máxima (0.005 = 0,5 %) | 0.005 |
| `-t <seg>` | tolerância da média tolerante a falhas | 300 |
| `-d <ms>` | atraso máximo simulado da rede | 5 |
| `-i <ms>` | intervalo entre rodadas | 500 |
| `-a <int>` | leituras por processo (usa a de menor RTT) | 3 |
| `-c <rank>` | rank do coordenador | 0 |
| `-f <rank>` | processo com relógio defeituoso (+1 h) | nenhum |
| `-s <int>` | semente aleatória | 42 |
| `-e` | exemplo clássico do livro (use `-n 3`) | – |
| `-h` | ajuda | – |

## 4. Resultados

A saída completa está em [`saida_exemplo.txt`](saida_exemplo.txt). Resumo dos testes:

| Cenário | Dispersão antes | Dispersão depois |
|---|---|---|
| 3 processos, exemplo do livro | 2100 s (35 min) | ≈ 0 s, todos às 3:05 |
| 5 processos, aleatório | ≈ 97 s | ≈ 1–5 ms |
| 6 processos, rank 3 defeituoso (+1 h) | ≈ 3700 s | ≈ 3–7 ms (o rank 3 fica fora da média e também é corrigido) |
| N = 1 … 64 | até ~120 s | sempre < 60 ms (critério de sucesso) |

Depois do ajuste, a dispersão que sobra (alguns milissegundos) é o **erro de estimativa da rede**. O limite teórico desse erro é RTT/2, e nenhum algoritmo baseado em troca de mensagens consegue eliminá-lo.

### Validação feita

* Compila sem nenhum aviso com `-std=c99 -Wall -Wextra -pedantic -Werror` (Linux/OpenMPI) e com `gcc -Wall -Wextra` + MS-MPI (Windows).
* Testado com AddressSanitizer + UndefinedBehaviorSanitizer: **0 erros de memória e 0 comportamentos indefinidos**. Os únicos "vazamentos" relatados ficam dentro de `MPI_Init`/`MPI_Finalize`, isto é, na própria biblioteca OpenMPI, e não no nosso código.
* Testado no **Windows (MS-MPI)** e no **Linux (OpenMPI)** com N = 1, 2, 3, 4, 5, 7, 8, 12, 13, 16, 32 e 64, com coordenador em ranks diferentes, com relógio defeituoso, com deriva alta, com tolerância 0 e com argumentos inválidos.

## 5. Checklist do enunciado

| Exigência | Onde está no código |
|---|---|
| Algoritmo de **Berkeley** | `coordenador()`: coleta → média tolerante a falhas → ajuste individual |
| Usando **MPI** | `MPI_Init`, `MPI_Comm_rank`, `MPI_Comm_size`, `MPI_Send`, `MPI_Recv`, `MPI_Finalize` |
| `MPI_Send` / `MPI_Recv` (visto em aula) | Toda a comunicação do algoritmo: funções `coletar()`, `escravo()` e o laço de ajuste |
| **Quantidade qualquer** de processos | `MPI_Comm_size(MPI_COMM_WORLD, &n)`, com vetores alocados com `malloc(n)` |
| Mestre-Escravo (aula) | `if (rank == coordenador) coordenador(); else escravo();` |

## 6. Casos especiais tratados

* **N = 1**: só existe o coordenador, que já está sincronizado consigo mesmo e encerra normalmente.
* **Empate na mediana** (N par com dois grupos muito distantes, ex.: `-n 2 -f 1`): a mediana cai no meio e nenhum relógio ficaria perto dela. Nesse caso o relógio do coordenador vira a referência, o que garante pelo menos 1 relógio na média e nunca divide por zero.
* **Coordenador inválido** (`-c` ≥ N) e **argumentos inválidos**: mensagem de erro, sem travar o MPI.
* **Relógio defeituoso**: fica fora da média pela comparação com a mediana, mas também recebe o ajuste.
* **Encerramento limpo**: o coordenador envia `TAG_FIM` a todos os escravos e cada processo chama `MPI_Finalize`.

## 7. Arquivos

| Arquivo | Conteúdo |
|---|---|
| `berkeley.c` | código-fonte em C |
| `build.bat` | compilação no Windows (MS-MPI + gcc) |
| `executar.bat` | demonstração para a apresentação, em 5 etapas com pausa |
| `ROTEIRO_APRESENTACAO.md` | roteiro da apresentação, fala por fala, com a demonstração |
| `PERGUNTAS_E_RESPOSTAS.md` | perguntas que o professor pode fazer, com as respostas |
| `Makefile` | compilação e execução no Linux/WSL |
| `saida_exemplo.txt` | saída real de execução |

## Referências

* R. Gusella, S. Zatti. *The Accuracy of the Clock Synchronization Achieved by TEMPO in Berkeley UNIX 4.3BSD*. IEEE Trans. Software Eng., 1989.
* A. S. Tanenbaum, M. van Steen. *Sistemas Distribuídos: Princípios e Paradigmas*, cap. 6 (Sincronização).
* G. Coulouris et al. *Sistemas Distribuídos: Conceitos e Projeto*, cap. 14.
* MPI Forum. *MPI: A Message-Passing Interface Standard* (`MPI_Send`, `MPI_Recv`).
