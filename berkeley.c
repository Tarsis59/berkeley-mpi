#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define TAG_REQ     1
#define TAG_RESP    2
#define TAG_AJUSTE  3
#define TAG_FIM     4

typedef struct {
    int    rodadas;
    double max_offset;
    double max_drift;
    double tolerancia;
    double max_atraso_ms;
    double intervalo_ms;
    int    coordenador;
    int    defeituoso;
    unsigned semente;
    int    exemplo;
    int    amostras;
} Config;

typedef struct {
    double base;
    double offset;
    double drift;
    double t0;
} Relogio;

static double relogio_ler(const Relogio *r)
{
    double real = MPI_Wtime() - r->t0;
    return r->base + r->offset + real * (1.0 + r->drift);
}

static void relogio_ajustar(Relogio *r, double ajuste)
{
    r->offset += ajuste;
}

static unsigned long long rng_estado;

static void rng_iniciar(unsigned semente, int rank)
{
    rng_estado = 0x9E3779B97F4A7C15ULL ^ ((unsigned long long)semente << 20)
                 ^ ((unsigned long long)(rank + 1) * 0xBF58476D1CE4E5B9ULL);
    if (rng_estado == 0) rng_estado = 88172645463325252ULL;
    for (int i = 0; i < 8; i++) {
        rng_estado ^= rng_estado << 13;
        rng_estado ^= rng_estado >> 7;
        rng_estado ^= rng_estado << 17;
    }
}

static double rng_uniforme(void)
{
    rng_estado ^= rng_estado << 13;
    rng_estado ^= rng_estado >> 7;
    rng_estado ^= rng_estado << 17;
    return (double)(rng_estado >> 11) / 9007199254740992.0;
}

static double rng_simetrico(double a)
{
    return (2.0 * rng_uniforme() - 1.0) * a;
}

static void esperar_ms(double ms)
{
    if (ms <= 0) return;
    double fim = MPI_Wtime() + ms / 1000.0;
    while (MPI_Wtime() < fim) { }
}

static void formatar_hora(double s, char *buf, size_t n)
{
    double dia = 86400.0;
    s = fmod(s, dia);
    if (s < 0) s += dia;
    long long ms = (long long)llround(s * 1000.0);
    if (ms >= 86400000LL) ms -= 86400000LL;
    int h  = (int)(ms / 3600000LL);
    int m  = (int)((ms / 60000LL) % 60);
    int sg = (int)((ms / 1000LL) % 60);
    int mi = (int)(ms % 1000LL);
    snprintf(buf, n, "%02d:%02d:%02d.%03d", h, m, sg, mi);
}

static void formatar_delta(double d, char *buf, size_t n)
{
    double a = fabs(d);
    long long ms = (long long)llround(a * 1000.0);
    char sinal = (d < 0 && ms > 0) ? '-' : '+';
    if (ms >= 60000LL)
        snprintf(buf, n, "%c%lldm%02lld.%03llds", sinal, ms / 60000LL,
                 (ms / 1000LL) % 60, ms % 1000LL);
    else
        snprintf(buf, n, "%c%lld.%03llds", sinal, ms / 1000LL, ms % 1000LL);
}

static int comparar_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static double mediana(const double *v, int n)
{
    double *c = (double *)malloc((size_t)n * sizeof(double));
    if (!c) MPI_Abort(MPI_COMM_WORLD, 2);
    memcpy(c, v, (size_t)n * sizeof(double));
    qsort(c, (size_t)n, sizeof(double), comparar_double);

    double m = (n % 2) ? c[n / 2] : 0.5 * (c[n / 2 - 1] + c[n / 2]);
    free(c);
    return m;
}

static void ajuda(void)
{
    printf(
        "Uso: mpiexec -n <N> berkeley [opcoes]\n\n"
        "  -r <int>    numero de rodadas de sincronizacao      (padrao 3)\n"
        "  -o <seg>    desvio inicial maximo dos relogios       (padrao 60)\n"
        "  -D <frac>   deriva maxima dos relogios (0.005=0,5%%)  (padrao 0.005)\n"
        "  -t <seg>    tolerancia da media tolerante a falhas   (padrao 300)\n"
        "  -d <ms>     atraso maximo simulado da rede           (padrao 5)\n"
        "  -i <ms>     intervalo entre rodadas                  (padrao 500)\n"
        "  -a <int>    leituras por processo (usa a de menor RTT) (padrao 3)\n"
        "  -c <rank>   rank do coordenador                      (padrao 0)\n"
        "  -f <rank>   processo com relogio defeituoso (+1h)    (padrao nenhum)\n"
        "  -s <int>    semente aleatoria                        (padrao 42)\n"
        "  -e          exemplo classico do livro (use -n 3):\n"
        "              relogios 3:00, 3:25 e 2:50 -> todos para 3:05\n"
        "  -h          mostra esta ajuda\n");
}

static int ler_argumentos(int argc, char **argv, Config *c)
{
    c->rodadas = 3;      c->max_offset = 60.0;  c->max_drift = 0.005;
    c->tolerancia = 300; c->max_atraso_ms = 5;  c->intervalo_ms = 500;
    c->coordenador = 0;  c->defeituoso = -1;    c->semente = 42;
    c->exemplo = 0;      c->amostras = 3;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) return 1;
        if (strcmp(a, "-e") == 0) { c->exemplo = 1; continue; }

        if (a[0] != '-' || a[1] == '\0' || a[2] != '\0' || i + 1 >= argc)
            return -1;
        const char *v = argv[++i];
        char *fim;
        double x = strtod(v, &fim);
        if (fim == v || *fim != '\0') return -1;
        switch (a[1]) {
            case 'r': c->rodadas = (int)x;        break;
            case 'o': c->max_offset = x;          break;
            case 'D': c->max_drift = x;           break;
            case 't': c->tolerancia = x;          break;
            case 'd': c->max_atraso_ms = x;       break;
            case 'i': c->intervalo_ms = x;        break;
            case 'c': c->coordenador = (int)x;    break;
            case 'f': c->defeituoso = (int)x;     break;
            case 's': if (x < 0 || x > 4e9) return -1;
                      c->semente = (unsigned)x;   break;
            case 'a': c->amostras = (int)x;       break;
            default: return -1;
        }
    }

    if (c->rodadas < 1 || c->rodadas > 100000 || c->amostras < 1 ||
        c->amostras > 1000 || c->max_offset < 0 || c->max_drift < 0 ||
        c->max_drift >= 1 || c->tolerancia < 0 || c->max_atraso_ms < 0 ||
        c->intervalo_ms < 0)
        return -1;
    return 0;
}

static void escravo(int rank, const Config *cfg, Relogio *rel)
{
    for (;;) {
        double msg;
        MPI_Status st;

        MPI_Recv(&msg, 1, MPI_DOUBLE, cfg->coordenador, MPI_ANY_TAG,
                 MPI_COMM_WORLD, &st);

        if (st.MPI_TAG == TAG_REQ) {
            esperar_ms(rng_uniforme() * cfg->max_atraso_ms);
            double agora = relogio_ler(rel);
            esperar_ms(rng_uniforme() * cfg->max_atraso_ms);
            MPI_Send(&agora, 1, MPI_DOUBLE, cfg->coordenador, TAG_RESP,
                     MPI_COMM_WORLD);
        } else if (st.MPI_TAG == TAG_AJUSTE) {
            relogio_ajustar(rel, msg);
        } else if (st.MPI_TAG == TAG_FIM) {
            break;
        } else {
            fprintf(stderr, "[rank %d] tag inesperada %d\n", rank, st.MPI_TAG);
            MPI_Abort(MPI_COMM_WORLD, 3);
        }
    }
}

static void coletar(int n, const Config *cfg, const Relogio *rel,
                    double *delta, double *leitura, double *rtt)
{
    int coord = cfg->coordenador;
    for (int i = 0; i < n; i++) {
        if (i == coord) {
            delta[i] = 0.0;
            leitura[i] = relogio_ler(rel);
            rtt[i] = 0.0;
            continue;
        }
        rtt[i] = -1.0;
        for (int a = 0; a < cfg->amostras; a++) {
            double t_envio = relogio_ler(rel);
            MPI_Send(&t_envio, 1, MPI_DOUBLE, i, TAG_REQ, MPI_COMM_WORLD);

            double resp;
            MPI_Recv(&resp, 1, MPI_DOUBLE, i, TAG_RESP, MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);
            double t_receb = relogio_ler(rel);

            double r = t_receb - t_envio;
            if (rtt[i] < 0 || r < rtt[i]) {
                rtt[i] = r;
                leitura[i] = resp;
                delta[i] = (resp + r / 2.0) - t_receb;
            }
        }
    }
}

static double dispersao(const double *delta, int n)
{
    double mn = delta[0], mx = delta[0];
    for (int i = 1; i < n; i++) {
        if (delta[i] < mn) mn = delta[i];
        if (delta[i] > mx) mx = delta[i];
    }
    return mx - mn;
}

static double media_tolerante(const double *delta, int n, double ref,
                              double tol, int *usado, int *k)
{
    double soma = 0.0;
    *k = 0;
    for (int i = 0; i < n; i++) {
        usado[i] = fabs(delta[i] - ref) <= tol;
        if (usado[i]) { soma += delta[i]; (*k)++; }
    }
    return (*k > 0) ? soma / *k : 0.0;
}

static void coordenador(int n, const Config *cfg, Relogio *rel)
{
    int coord = cfg->coordenador;
    size_t tam = (size_t)n * sizeof(double);
    double *delta   = (double *)malloc(tam);
    double *leitura = (double *)malloc(tam);
    double *rtt     = (double *)malloc(tam);
    double *ajuste  = (double *)malloc(tam);
    double *depois  = (double *)malloc(tam);
    double *leit2   = (double *)malloc(tam);
    double *rtt2    = (double *)malloc(tam);
    int    *usado   = (int *)malloc((size_t)n * sizeof(int));
    if (!delta || !leitura || !rtt || !ajuste || !depois || !leit2 ||
        !rtt2 || !usado)
        MPI_Abort(MPI_COMM_WORLD, 2);

    char h1[32], h2[32], d1[32], d2[32], d3[32];
    double disp_final = 0.0;
    int sucesso = 1;

    printf("==============================================================\n");
    printf(" ALGORITMO DE BERKELEY - sincronizacao de relogios com MPI\n");
    printf("==============================================================\n");
    printf(" Processos: %d  (coordenador = rank %d, escravos = %d)\n",
           n, coord, n - 1);
    printf(" Rodadas: %d | tolerancia: %.3fs | atraso max. rede: %.1fms\n",
           cfg->rodadas, cfg->tolerancia, cfg->max_atraso_ms);
    if (cfg->defeituoso >= 0 && cfg->defeituoso < n)
        printf(" Processo com relogio defeituoso (+1h): rank %d\n",
               cfg->defeituoso);
    printf("==============================================================\n");

    if (n == 1) {
        formatar_hora(relogio_ler(rel), h1, sizeof h1);
        printf("\nSo existe 1 processo: o coordenador ja esta sincronizado "
               "consigo mesmo (%s).\n", h1);
    }

    for (int r = 1; r <= cfg->rodadas && n > 1; r++) {
        if (r > 1) esperar_ms(cfg->intervalo_ms);

        coletar(n, cfg, rel, delta, leitura, rtt);

        double med = mediana(delta, n);
        int k;
        double media = media_tolerante(delta, n, med, cfg->tolerancia,
                                       usado, &k);
        if (k == 0) {
            media = media_tolerante(delta, n, 0.0, cfg->tolerancia,
                                    usado, &k);
        }

        for (int i = 0; i < n; i++) {
            ajuste[i] = media - delta[i];
            if (i == coord)
                relogio_ajustar(rel, ajuste[i]);
            else
                MPI_Send(&ajuste[i], 1, MPI_DOUBLE, i, TAG_AJUSTE,
                         MPI_COMM_WORLD);
        }

        coletar(n, cfg, rel, depois, leit2, rtt2);
        double disp_antes  = dispersao(delta, n);
        double disp_depois = dispersao(depois, n);
        disp_final = disp_depois;

        printf("\n---------------------------- RODADA %d "
               "----------------------------\n", r);
        printf("%-5s %-13s %-13s %-8s %-6s %-13s %-13s\n", "rank",
               "relogio lido", "diferenca", "RTT(ms)", "media?",
               "ajuste", "dif. depois");
        for (int i = 0; i < n; i++) {
            formatar_hora(leitura[i], h1, sizeof h1);
            formatar_delta(delta[i], d1, sizeof d1);
            formatar_delta(ajuste[i], d2, sizeof d2);
            formatar_delta(depois[i], d3, sizeof d3);
            printf("%-5d %-13s %-13s %-8.3f %-6s %-13s %-13s%s\n", i, h1, d1,
                   rtt[i] * 1000.0, usado[i] ? "sim" : "NAO", d2, d3,
                   i == coord ? "  <- coordenador" : "");
        }
        formatar_delta(media, d1, sizeof d1);
        formatar_hora(relogio_ler(rel), h2, sizeof h2);
        printf("Media tolerante a falhas: %s (%d de %d relogios usados)\n",
               d1, k, n);
        printf("Dispersao (maior - menor): antes = %.6f s | depois = %.6f s\n",
               disp_antes, disp_depois);
        printf("Horario comum apos o ajuste: %s\n", h2);
        for (int i = 0; i < n; i++)
            if (!usado[i])
                printf("Aviso: rank %d descartado da media (relogio "
                       "discrepante), mas tambem foi corrigido.\n", i);
    }

    for (int i = 0; i < n; i++) {
        if (i == coord) continue;
        double nada = 0.0;
        MPI_Send(&nada, 1, MPI_DOUBLE, i, TAG_FIM, MPI_COMM_WORLD);
    }

    double limite = (2.0 * cfg->max_atraso_ms) / 1000.0 + 0.050;
    if (!isfinite(disp_final) || disp_final > limite) sucesso = 0;

    printf("\n==============================================================\n");
    if (sucesso)
        printf(" RESULTADO: relogios SINCRONIZADOS (dispersao final %.6f s"
               " <= %.3f s)\n", disp_final, limite);
    else
        printf(" RESULTADO: FALHOU - dispersao final %.6f s acima do limite"
               " %.3f s\n", disp_final, limite);
    printf("==============================================================\n");
    fflush(stdout);

    free(delta); free(leitura); free(rtt); free(ajuste);
    free(depois); free(leit2); free(rtt2); free(usado);
}

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int rank, n;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &n);

    Config cfg;
    int st = ler_argumentos(argc, argv, &cfg);
    if (st != 0) {
        if (rank == 0) {
            if (st < 0) fprintf(stderr, "Argumentos invalidos.\n\n");
            ajuda();
        }
        MPI_Finalize();
        return st < 0 ? 1 : 0;
    }
    if (cfg.coordenador < 0 || cfg.coordenador >= n) {
        if (rank == 0)
            fprintf(stderr, "Coordenador %d invalido: use 0..%d\n",
                    cfg.coordenador, n - 1);
        MPI_Finalize();
        return 1;
    }

    rng_iniciar(cfg.semente, rank);
    Relogio rel;
    rel.base   = 12.0 * 3600.0;
    rel.offset = rng_simetrico(cfg.max_offset);
    rel.drift  = rng_simetrico(cfg.max_drift);
    if (rank == cfg.defeituoso) rel.offset += 3600.0;

    if (cfg.exemplo) {
        static const double ex[3] = { 0.0, 25 * 60.0, -10 * 60.0 };
        int pos = (rank - cfg.coordenador + n) % n;
        rel.base   = 3.0 * 3600.0;
        rel.offset = ex[pos % 3];
        rel.drift  = 0.0;
        cfg.tolerancia    = 3600.0;
        cfg.max_atraso_ms = 0.0;
        if (rank == 0 && n != 3)
            printf("Obs.: o exemplo classico foi pensado para -n 3.\n");
    }

    MPI_Barrier(MPI_COMM_WORLD);
    rel.t0 = MPI_Wtime();

    if (rank == cfg.coordenador)
        coordenador(n, &cfg, &rel);
    else
        escravo(rank, &cfg, &rel);

    MPI_Finalize();
    return 0;
}
