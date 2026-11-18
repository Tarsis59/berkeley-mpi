# Roteiro da Apresentação: Algoritmo de Berkeley com MPI

Duração sugerida: **10 a 12 minutos** (cerca de 6 min de explicação, 4 min de demonstração e o tempo restante para perguntas).
Material: os 16 slides (link do deck), `berkeley.c` aberto no editor e um terminal na pasta do trabalho.

---

## 0. Antes de começar (5 min antes, sem o professor olhando)

1. Abra o **Prompt de Comando** (cmd) e entre na pasta:
   ```bat
   cd berkeley-mpi
   set "PATH=C:\Program Files\Microsoft MPI\Bin;%PATH%"
   ```
2. Teste rápido (deve aparecer `RESULTADO: relogios SINCRONIZADOS`):
   ```bat
   mpiexec -n 3 berkeley.exe -e -r 1
   ```
3. Se `berkeley.exe` sumir ou der erro, recompile com `build.bat`.
4. Se o Windows bloquear o .exe ("política de Controle de Aplicativo"), use o **plano B**, o Docker:
   ```bash
   docker run --rm -it -v "%cd%:/src" -w /src ubuntu:24.04 bash -c "apt update && apt install -y openmpi-bin libopenmpi-dev gcc make && export OMPI_ALLOW_RUN_AS_ROOT=1 OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1 && make exemplo && make falha"
   ```
   **Plano C:** mostre o arquivo `saida_exemplo.txt`, que tem uma saída real já gravada.
5. Aumente a fonte do terminal (Ctrl + roda do mouse) e deixe a janela larga, porque a tabela tem cerca de 95 colunas.

---

## 1. Abertura (≈ 30 s), slide 1

> "Nosso trabalho é o algoritmo de **Berkeley** de sincronização de relógios, implementado em **C com MPI**, funcionando para **qualquer quantidade de processos**. Usamos o mesmo modelo Mestre‑Escravo e as mesmas primitivas `MPI_Send` e `MPI_Recv` vistas em aula."

## 2. O problema (≈ 1 min), slides 2 e 3

> "Em um sistema distribuído cada máquina tem o seu próprio relógio físico. Esses relógios nunca marcam exatamente a mesma hora: existe um **desvio** inicial e uma **deriva**, porque cada cristal oscila numa frequência um pouco diferente. Sem sincronização, eventos em máquinas diferentes ficam com horários incoerentes. Isso atrapalha logs, `make`, transações, timeouts, etc."

> "O Berkeley serve para o caso em que **não existe** uma fonte de hora confiável (UTC, GPS, NTP). O objetivo não é acertar a hora 'real', e sim fazer **todos concordarem** sobre uma hora comum."

## 3. O algoritmo (≈ 2 min), slides 4 a 7 (passos, exemplo, RTT, tolerância a falhas)

> "Um processo é eleito **coordenador**, o *time daemon*. A cada rodada:"

1. **Coleta.** "O coordenador pergunta a hora a cada processo, um `MPI_Send`, e recebe a resposta com um `MPI_Recv`. Ele mede o **RTT**, o tempo de ida e volta, e compensa a viagem da mensagem somando **RTT/2**, a técnica de Cristian."
2. **Média tolerante a falhas.** "Ele calcula a diferença de cada relógio para o dele e tira a média. Relógios muito discrepantes, longe da mediana, ficam **fora** da média, para um relógio quebrado não estragar a hora de todos."
3. **Ajuste.** "Ele manda a **cada** processo a **sua** correção, que pode ser positiva ou negativa. Envia a correção e não a hora, porque a correção não fica desatualizada enquanto a mensagem viaja."

Exemplo do livro no quadro: **3:00, 3:25, 2:50 → diferenças 0, +25, −10 → média +5 → ajustes +5, −20, +15 → todos 3:05.**

## 4. A implementação (≈ 2 min), slides 8 a 11 (arquitetura, MPI, código, relógio simulado), com o código aberto

Mostre no `berkeley.c`:

| O que dizer | Onde apontar |
|---|---|
| "Descobrimos N em tempo de execução, então funciona para qualquer quantidade" | `main`: `MPI_Comm_size` |
| "O mesmo programa roda em todos (SPMD). O rank decide o papel" | fim do `main`: `if (rank == cfg.coordenador) coordenador(); else escravo();` |
| "O escravo fica num laço `MPI_Recv` com `MPI_ANY_TAG` e age conforme a *tag*" | `escravo()` |
| "Coleta: Send REQ, Recv RESP, mede RTT, `delta = resp + RTT/2 − agora`" | `coletar()` |
| "Média tolerante a falhas pela mediana" | `coordenador()`: `mediana()` e `media_tolerante()` |
| "Ajuste individual `media − delta[i]`" | `coordenador()`: `ajuste[i] = media - delta[i]` |
| "Os relógios são **simulados**: num PC só, todos veem o mesmo relógio. Cada processo ganha um desvio e uma deriva aleatórios" | struct `Relogio` e `relogio_ler()` |

## 5. Demonstração (≈ 4 min), slide 12, depois slides 13 e 14 (resultados e validação)

Rode `executar.bat` (ele pausa entre as etapas) ou digite os comandos abaixo.

### Demo 1: exemplo do livro
```bat
mpiexec -n 3 berkeley.exe -e -r 1
```
**Aponte:** coluna *relogio lido* (03:00, 03:25, 02:50), coluna *ajuste* (+5m, −20m, +15m) e a linha "Horario comum apos o ajuste: **03:05:00.000**".
> "É exatamente o exemplo do livro. Dispersão: 2100 segundos antes, zero depois."

### Demo 2: quantidade qualquer de processos
```bat
mpiexec -n 5 berkeley.exe -r 2
```
**Aponte:** dispersão de ≈ 97 s antes e de poucos milissegundos depois. Na rodada 2 os relógios já tinham se afastado de novo alguns ms por causa da **deriva**.
> "Por isso o algoritmo é **periódico**: os relógios voltam a se afastar."

Troque o número ao vivo para mostrar que qualquer N funciona:
```bat
mpiexec -n 12 berkeley.exe -r 1
```

### Demo 3: tolerância a falhas
```bat
mpiexec -n 6 berkeley.exe -f 3 -r 1
```
**Aponte:** o rank 3 está 1 hora adiantado, coluna *media?* = **NAO**, e o aviso "descartado da media … mas tambem foi corrigido".
> "Se ele entrasse na média, puxaria todos 10 minutos para frente. Ele fica fora da média, mas recebe a correção dele."

### Demo 4 (opcional): outro coordenador
```bat
mpiexec -n 4 berkeley.exe -c 2 -r 1
```
> "O coordenador pode ser qualquer rank, não precisa ser o 0."

## 6. Fechamento (≈ 1 min), slides 15 (limitações) e 16 (conclusão)

> "Resumindo: o algoritmo de Berkeley completo (coleta com compensação de RTT, média tolerante a falhas e ajuste individual), só com `MPI_Send` e `MPI_Recv`, para qualquer número de processos. Testamos de 1 a 64 processos, no Windows e no Linux, e depois do ajuste os relógios ficam a poucos milissegundos uns dos outros. Esse erro residual é o limite teórico RTT/2. Obrigado, ficamos abertos a perguntas."

---

## Como ler a tabela de saída

```
rank  relogio lido  diferenca     RTT(ms)  media? ajuste        dif. depois
```

| Coluna | Significado |
|---|---|
| `rank` | identificador MPI do processo |
| `relogio lido` | hora que o processo informou ao coordenador |
| `diferenca` | Δ = relógio do processo − relógio do coordenador (com a compensação RTT/2) |
| `RTT(ms)` | tempo de ida e volta da melhor de 3 leituras (o do coordenador é 0) |
| `media?` | se o relógio entrou na média (`NAO` = descartado como defeituoso) |
| `ajuste` | correção enviada = média − Δ |
| `dif. depois` | diferença medida de novo após o ajuste (deve ser ≈ 0) |
| `Dispersao` | maior Δ − menor Δ: o quanto os relógios discordam entre si |

## Se algo der errado na hora

| Problema | Solução |
|---|---|
| `'mpiexec' não é reconhecido` | `set "PATH=C:\Program Files\Microsoft MPI\Bin;%PATH%"` |
| `berkeley.exe` não existe | `build.bat` |
| "política de Controle de Aplicativo bloqueou" | Plano B (Docker) ou plano C (`saida_exemplo.txt`) |
| Tabela quebrada em várias linhas | aumente a largura da janela do terminal |
| Demora com muitos processos | use no máximo cerca de 16 processos no notebook |
