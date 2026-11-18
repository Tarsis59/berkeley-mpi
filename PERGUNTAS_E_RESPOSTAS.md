# Perguntas que o professor pode fazer, com as respostas

As referências de linha são do arquivo `berkeley.c`.

---

## A. Teoria: sincronização de relógios

**1. Por que os relógios de computadores diferentes discordam?**
Cada computador conta o tempo com um cristal de quartzo, e cada cristal oscila numa frequência levemente diferente. Isso gera a **deriva** (*drift*, tipicamente cerca de 10⁻⁶, ou seja, aproximadamente 1 s a cada 11,6 dias). Além disso, os relógios começam com um **desvio** (*offset/skew*) diferente. Com o tempo, a diferença só aumenta.

**2. O que é o algoritmo de Berkeley?**
É um algoritmo de **sincronização interna** de relógios físicos, proposto por Gusella e Zatti (Berkeley UNIX 4.3BSD, 1989). Um coordenador (*time daemon*) consulta periodicamente todos os processos, calcula a média dos relógios (descartando os discrepantes) e envia a cada um a correção necessária. O resultado é que todos concordam sobre uma hora comum.

**3. Qual a diferença entre sincronização interna e externa?**
- **Externa**: os relógios são acertados com uma fonte de referência (UTC, GPS, servidor NTP). Exemplos: algoritmo de Cristian e NTP.
- **Interna**: os relógios só precisam concordar entre si, mesmo que a hora comum esteja "errada" em relação à real. Exemplo: Berkeley.

**4. Qual a diferença entre Berkeley e Cristian?**
| | Cristian | Berkeley |
|---|---|---|
| Servidor | **passivo** (o cliente pergunta) | coordenador **ativo** (ele pergunta a todos) |
| Fonte de tempo | servidor com hora UTC confiável | nenhuma: usa a média |
| Tipo | externa | interna |
| Resultado | cada cliente copia a hora do servidor | todos vão para a média |

O nosso código **usa a ideia de Cristian dentro do Berkeley**: a compensação `leitura + RTT/2` para estimar o relógio do escravo.

**5. E o NTP?**
O NTP é um protocolo hierárquico (estratos) de sincronização externa pela Internet. Ele usa 4 marcas de tempo por troca e filtragem estatística. O Berkeley é mais simples e serve para uma rede local sem fonte UTC.

**6. Qual a diferença para os relógios lógicos de Lamport?**
Lamport e os relógios vetoriais **não medem tempo**. Eles só ordenam eventos pela relação "aconteceu‑antes". O Berkeley sincroniza **relógios físicos**, ou seja, a hora de verdade em segundos.

**7. Por que o coordenador envia a correção e não a hora certa?**
Porque a mensagem leva um tempo para chegar. Se ele enviasse "agora são 3:05", ao chegar já seriam 3:05 e mais alguma coisa, e o escravo ficaria atrasado. A **correção** (ex.: "adiante 5 min") é relativa e continua correta, não importa quanto a mensagem demore.

**8. Por que a média é "tolerante a falhas"?**
Um relógio defeituoso (ex.: 1 hora adiantado) puxaria a média de todos. Com 6 processos, 1 hora a mais desloca a média em 10 minutos. Por isso só entram na média os relógios próximos dos outros. Os descartados **também recebem a correção**, para ficarem certos. No código, `media_tolerante()` é chamada em `coordenador()` com a mediana como referência.

**9. Por que usar a mediana como referência?**
A mediana é **robusta**: um valor absurdo não a move, ao contrário da média. Assim "perto da mediana" significa "perto da maioria". O artigo original escolhe o maior subconjunto de relógios que concordam entre si dentro de uma tolerância, e a mediana é uma forma simples e equivalente na prática de achar esse grupo.

**10. O que é RTT e por que dividir por 2?**
RTT (*round‑trip time*) é o tempo entre o coordenador enviar o pedido e receber a resposta. Supondo que a ida e a volta demoram o mesmo, a resposta foi gerada no **meio** do RTT. Então, quando chega, o relógio do escravo já andou cerca de RTT/2: `estimativa = leitura + RTT/2`.

**11. Qual é o erro dessa estimativa?**
No pior caso, quando toda a demora foi na ida ou toda na volta, o erro é de **±RTT/2**. Por isso o nosso código faz 3 leituras e usa a de **menor RTT** (em `coletar()`): menor RTT significa menor erro máximo. Esse erro residual de alguns ms é o que aparece em "dif. depois", e nenhum algoritmo baseado em mensagens consegue zerá-lo.

**12. Que precisão o Berkeley consegue?**
No experimento original de Gusella e Zatti, 15 computadores ficaram sincronizados com uma diferença de cerca de 20–25 ms (segundo Coulouris). Na nossa simulação local a dispersão final fica entre 1 e 20 ms.

**13. Por que o algoritmo precisa ser repetido periodicamente?**
Por causa da **deriva**: mesmo depois de sincronizados, os relógios continuam andando em velocidades diferentes e voltam a se afastar. No programa, a rodada 2 mostra os relógios alguns milissegundos afastados de novo. Teste com `-D 0.2` (deriva de 20%) para ver isso bem claro.

**14. Atrasar um relógio (ajuste negativo) não é perigoso?**
É, sim. A hora "voltar" pode quebrar programas que supõem que o tempo só avança (ex.: `make`, logs). Na prática se usa **ajuste gradual** (*slewing*, como o `adjtime()` do UNIX): o relógio anda um pouco mais devagar até absorver a correção. No trabalho aplicamos o ajuste direto, porque o foco é o algoritmo de sincronização. Seria fácil trocar `relogio_ajustar()` por um ajuste gradual alterando temporariamente o `drift`.

**15. E se o coordenador falhar?**
O Berkeley não trata isso sozinho. Executa-se um **algoritmo de eleição** (Valentão/*Bully* ou Anel) para escolher outro coordenador. O nosso programa já permite que qualquer rank seja coordenador (`-c`), então a eleição só precisaria escolher esse rank.

**16. O coordenador entra na média?**
Sim. O Δ dele é 0 (em `coletar()`), e ele também se ajusta (em `coordenador()`). No exemplo do livro o coordenador estava em 3:00 e foi para 3:05.

---

## B. MPI

**17. O que é MPI?**
*Message Passing Interface*: um padrão de biblioteca para programação paralela e distribuída por **troca de mensagens**. Os processos não compartilham memória e só se comunicam por mensagens. Implementações: OpenMPI, MPICH, MS‑MPI.

**18. Quais funções MPI vocês usaram e para quê?**
| Função | Uso |
|---|---|
| `MPI_Init` / `MPI_Finalize` | iniciar e encerrar o ambiente MPI |
| `MPI_Comm_rank` | descobrir o próprio identificador (0..N−1) |
| `MPI_Comm_size` | descobrir **quantos** processos existem (N qualquer) |
| `MPI_Send` / `MPI_Recv` | **toda a comunicação do algoritmo** |
| `MPI_Wtime` | relógio de alta resolução (base do relógio simulado) |
| `MPI_Barrier` | uma vez, só para todos "ligarem" o relógio juntos (fora do algoritmo) |
| `MPI_Abort` | encerrar tudo em caso de erro grave (falta de memória) |

**19. Explique os parâmetros do `MPI_Send`.**
`MPI_Send(&ajuste[i], 1, MPI_DOUBLE, i, TAG_AJUSTE, MPI_COMM_WORLD)` significa: endereço do dado, **quantidade** de elementos, **tipo** MPI, **rank de destino**, **tag** (tipo da mensagem) e **comunicador** (grupo de processos).

**20. E do `MPI_Recv`?**
`MPI_Recv(&msg, 1, MPI_DOUBLE, origem, MPI_ANY_TAG, MPI_COMM_WORLD, &st)` tem os mesmos parâmetros, mais o **status** (`st`), que informa quem enviou e qual a tag. O escravo usa `MPI_ANY_TAG` e depois olha `st.MPI_TAG` para saber se é pedido, ajuste ou fim (em `escravo()`).

**21. O que é bloqueante?**
`MPI_Recv` só retorna quando a mensagem chegou. `MPI_Send` só retorna quando o buffer pode ser reutilizado, o que pode exigir que o destino já tenha postado o `Recv`. Foi isso que permitiu medir o RTT: o coordenador fica parado no `Recv` até a resposta chegar.

**22. Pode dar deadlock (impasse)?**
Não. A comunicação é sempre **em pares e na mesma ordem**: o coordenador faz `Send` para o escravo i e depois `Recv` do escravo i. O escravo está sempre parado num `Recv` esperando o coordenador, faz o que foi pedido e, se for um pedido, responde com um `Send`. Nunca dois processos ficam esperando um pelo outro ao mesmo tempo, então não existe ciclo de espera.

**23. A mensagem de ajuste pode chegar depois do próximo pedido?**
Não. O padrão MPI garante a **ordem (não ultrapassagem)** de mensagens entre o **mesmo par** de processos no mesmo comunicador. Logo o `TAG_AJUSTE` sempre chega antes do `TAG_REQ` seguinte.

**24. Para que servem as tags?**
Para identificar o tipo de mensagem: `TAG_REQ` (pedido de hora), `TAG_RESP` (resposta), `TAG_AJUSTE` (correção) e `TAG_FIM` (encerrar). Ficam nos `#define` do início do arquivo.

**25. Por que não usaram `MPI_Gather`/`MPI_Scatter`/`MPI_Bcast`?**
Eles funcionariam: um `MPI_Gather` para coletar as horas e um `MPI_Scatter` para distribuir os ajustes. Mas (1) o enunciado e a aula pedem `MPI_Send`/`MPI_Recv`, (2) o modelo Mestre‑Escravo da aula é ponto a ponto e (3) com o `Gather` não dá para medir o **RTT de cada processo** separadamente, que é o que permite compensar o atraso da rede.

**26. O que é `MPI_COMM_WORLD`?**
É o comunicador padrão, que contém **todos** os processos iniciados pelo `mpiexec`.

**27. O que acontece no `mpiexec -n 5 berkeley.exe`?**
O MPI cria **5 processos** executando o **mesmo programa** (modelo **SPMD**, *Single Program Multiple Data*). Cada um recebe um rank de 0 a 4. O `if (rank == coordenador)` no fim do `main` decide quem é mestre e quem é escravo.

**28. Isso funciona em várias máquinas de verdade?**
Sim, sem mudar o código. Basta informar os hosts (ex.: `mpiexec -hosts 2 pc1 2 pc2 2 berkeley.exe` no MS‑MPI ou `mpirun --hostfile hosts` no OpenMPI). Nesse caso poderíamos trocar o relógio simulado pela hora real do sistema.

---

## C. Implementação

**29. Onde fica garantido o requisito "quantidade qualquer de processos"?**
`MPI_Comm_size(MPI_COMM_WORLD, &n)` (no `main`) descobre N em tempo de execução. Todos os vetores são alocados com `malloc(n * sizeof(double))` e os laços vão de `0` a `n‑1`. Não existe nenhum número fixo de processos no código. Testamos com 1, 2, 3, 4, 5, 7, 8, 12, 13, 16, 32 e 64 processos.

**30. Por que simular os relógios?**
Porque, rodando num computador só, todos os processos leem o **mesmo** relógio físico, e não haveria nada para sincronizar. Cada processo tem então: `hora = 12:00 + offset + tempo_real × (1 + drift)` (função `relogio_ler`). O `offset` começa aleatório (±60 s), o `drift` é aleatório (±0,5%) e o ajuste soma no `offset`.

**31. Como funciona a função `coletar()`?**
Para cada escravo i (função `coletar()`):
1. `T0 = relogio_ler()` e `MPI_Send(TAG_REQ)`;
2. `MPI_Recv(TAG_RESP)` recebe a hora do escravo;
3. `T1 = relogio_ler()` e `RTT = T1 − T0`;
4. `Δᵢ = (resposta + RTT/2) − T1`.

Repete 3 vezes e guarda a de menor RTT. Para o próprio coordenador, Δ = 0.

**32. Mostre a fórmula do ajuste.**
`ajuste[i] = media − delta[i]` (em `coordenador()`). Exemplo: Δ = +25 min e média = +5 min dão ajuste = −20 min, então o relógio atrasa 20 minutos.

**33. Por que o resultado do ajuste é a média?**
O relógio i marca `C_coord + Δᵢ`. Somando o ajuste `média − Δᵢ`, ele passa a marcar `C_coord + média`, que é o **mesmo valor para todos os i**.

**34. Como vocês simulam o atraso da rede?**
O escravo espera um tempo aleatório antes de ler o relógio (ida) e outro depois (volta) (em `escravo()`). Como os dois são diferentes, a hipótese "ida = volta" do RTT/2 não é perfeita, o que gera um erro pequeno e realista.

**35. Por que gerador aleatório próprio e não `rand()`?**
`rand()` dá sequências diferentes no Windows e no Linux. Com o xorshift próprio e uma semente por rank, os resultados são **reprodutíveis**: a mesma semente produz os mesmos relógios iniciais em qualquer sistema.

**36. O que acontece com N = 1?**
Só existe o coordenador. Ele imprime que já está sincronizado consigo mesmo e encerra normalmente, sem erro e sem travar.

**37. E se a mediana ficar no meio de dois grupos distantes?**
Isso pode acontecer com N par, por exemplo 2 processos com 1 hora de diferença. Aí nenhum relógio fica perto da mediana. Nesse caso o relógio do coordenador (Δ = 0) vira a referência e entra sempre, então a média nunca divide por zero (em `coordenador()`). Encontramos esse caso nos nossos próprios testes e o corrigimos.

**38. Como o programa termina?**
O coordenador envia `TAG_FIM` a cada escravo (no fim de `coordenador()`). Cada escravo sai do laço e todos chamam `MPI_Finalize`.

**39. Por que a "dif. depois" não é exatamente zero?**
Por causa do erro de estimativa RTT/2 (pergunta 11) e porque a própria verificação também é uma medição pela rede. São alguns milissegundos, abaixo do limite de sucesso de 60 ms. No exemplo do livro (`-e`), sem atraso de rede, dá zero.

**40. Quantas mensagens por rodada?**
O Berkeley puro usa **3(N−1)** mensagens por rodada: N−1 pedidos, N−1 respostas e N−1 ajustes. Com as nossas 3 amostras e a verificação, são `2·3·(N−1) + (N−1) + 2·3·(N−1) = 13(N−1)`. A complexidade é **O(N)** por rodada.

**41. Isso escala para muitos processos?**
O coordenador é um **gargalo**, porque conversa com um processo de cada vez e o tempo da rodada cresce linearmente com N. Para sistemas grandes se usam hierarquias, como no NTP. Para uma rede local, que é o caso de uso do Berkeley, funciona bem. Testamos até 64 processos.

**42. Por que linguagem C?**
É a linguagem "nativa" do MPI (a API padrão é em C), é a usada em aula e não depende de bibliotecas extras como `mpi4py`. Compila igual com MS‑MPI (Windows) e OpenMPI (Linux).

**43. Por que `double` para a hora?**
Para representar segundos com fração (precisão de microssegundos). O `MPI_Wtime` também retorna `double`.

**44. O que é o `MPI_Barrier` no `main`? Não deveria ser só Send/Recv?**
O algoritmo usa **somente** Send/Recv. A barreira (no `main`) é preparação da simulação: faz todos os relógios simulados "ligarem" no mesmo instante. Ela poderia ser removida sem afetar o algoritmo.

**45. O que as opções da linha de comando fazem?**
`-r` rodadas, `-o` desvio inicial, `-D` deriva, `-t` tolerância, `-d` atraso de rede, `-i` intervalo entre rodadas, `-a` amostras, `-c` coordenador, `-f` relógio defeituoso, `-s` semente, `-e` exemplo do livro, `-h` ajuda. Valores inválidos geram mensagem de erro e o programa encerra sem travar.

**46. Como vocês testaram?**
- Compilação sem avisos com `-Wall -Wextra -pedantic -Werror`.
- AddressSanitizer e UndefinedBehaviorSanitizer: 0 erros.
- Windows (MS‑MPI) e Linux (OpenMPI).
- N de 1 a 64, coordenador em ranks diferentes, relógio defeituoso, deriva alta, tolerância 0 e argumentos inválidos.
- Exemplo do livro conferido à mão (3:05).

---

## D. Perguntas "pegadinha"

**47. Depois do Berkeley os relógios ficam com a hora certa?**
Não necessariamente. Eles ficam **iguais entre si**, mas a hora comum pode estar errada em relação ao UTC. Essa é a definição de sincronização interna.

**48. Se 2 de 3 relógios estiverem errados do mesmo jeito, o algoritmo percebe?**
Não. A tolerância a falhas descarta a **minoria** discrepante. Se a maioria estiver errada, a média segue a maioria. Para tolerar f relógios defeituosos é preciso ter a maioria correta.

**49. Dá para o escravo calcular o próprio ajuste?**
No Berkeley, não: quem tem a visão global (todas as diferenças) é o coordenador. Por isso ele calcula e envia o ajuste individual de cada processo.

**50. Qual a diferença entre o diagrama de matrizes da aula e este trabalho?**
É o **mesmo padrão Mestre‑Escravo**. Nas matrizes, o mestre envia submatrizes e recebe blocos do resultado. No Berkeley, o mestre envia pedidos de hora e recebe as horas, depois envia ajustes. Em ambos, as "setas de ida e volta" são `MPI_Send`/`MPI_Recv`.
