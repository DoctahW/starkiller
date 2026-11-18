#include <math.h>
#include <stddef.h>
#include <string.h>

#include "aliados.h"
#include "audio.h"
#include "combate.h"
#include "destroier.h"
#include "ia.h"
#include "nave.h"
#include "raymath.h"

#define DIST_MINIMA 1e-4f

Vector3 pontoNascimentoAliado(int indice)
{
    int coluna = indice % 4;
    int linha = indice / 4;

    return (Vector3){ ((float)coluna - 1.5f) * 135.0f,
                      SPAWN_ALA_Y - (float)linha * 165.0f,
                      (float)linha * 180.0f };
}

static Vector3 pontoPatrulha(int indice, PapelAliado papel, float tempo)
{
    float raio = papel == ALIADO_DEFENDER ? RAIO_PATRULHA : 6500.0f;
    float angulo = 2.0f * PI * (float)(indice % ALIADOS_INICIAIS) / (float)ALIADOS_INICIAIS
        + (papel == ALIADO_INTERCEPTAR ? PI / 3.0f : 0.0f) + tempo * VEL_ALIADO / raio;

    return (Vector3){ cosf(angulo) * raio, 1000.0f, sinf(angulo) * raio };
}

static void ativarAliado(Jogo *g, int indice)
{
    Aliado *a = &g->aliados[indice];
    /* 4 primeiros slots são guarda; os demais sempre interceptam. */
    PapelAliado papel = indice < ALIADOS_GUARDA_MAX ? ALIADO_DEFENDER : ALIADO_INTERCEPTAR;

    a->ativo = 1;
    a->casco = CASCO_ALIADO;
    a->cooldownTiro = 0.0f;
    a->tempoRespawn = 0.0f;
    a->alvoId = -1;
    a->alvoYwing = -1;
    a->fugindo = 0;
    a->papel = papel;
    a->pos = pontoNascimentoAliado(indice);
    a->vel = (Vector3){ 0.0f, 0.0f, VEL_ALIADO };
}

void iniciarAliados(Jogo *g)
{
    memset(g->aliados, 0, sizeof g->aliados);
    for (int i = 0; i < MAX_ALIADOS; i++) {
        g->aliados[i].alvoId = -1;
        g->aliados[i].alvoYwing = -1;
    }
    for (int i = 0; i < ALIADOS_INICIAIS; i++)
        ativarAliado(g, i);
    g->aliadosIniciados = 1;
}

int tamanhoRosterAliados(const Jogo *g)
{
    (void)g;
    return ALIADOS_MAX;
}

int contarAliadosAtivos(const Jogo *g)
{
    int n = 0;

    for (int i = 0; i < MAX_ALIADOS; i++)
        n += g->aliados[i].ativo;
    return n;
}

static const Inimigo *buscarInimigo(const Jogo *g, int id)
{
    if (id < 0)
        return NULL;
    for (const NoInimigo *no = g->inimigos.inicio; no; no = no->prox)
        if (no->dado.id == id)
            return &no->dado;
    return NULL;
}

/* Defensores priorizam bombardeiros; interceptadores preferem alvos fora do perímetro de defesa.
 * Um custo por parceiro no mesmo alvo distribui os caças. O bônus da trava evita trocas contínuas. */
static const Inimigo *escolherAlvo(const Jogo *g, const Aliado *a)
{
    const Inimigo *melhor = NULL;
    float custoMelhor = 1e30f;

    for (const NoInimigo *no = g->inimigos.inicio; no; no = no->prox) {
        const Inimigo *in = &no->dado;
        float distCentro = Vector3Length(in->pos);
        float custo = Vector3Distance(a->pos, in->pos);

        if (a->papel == ALIADO_DEFENDER) {
            if (distCentro > RAIO_DEFESA)
                continue;
            /* Defensores só reagem ao bombardeiro quando ele já está na corrida ou colado no casco. */
            if (in->tipo == YWING) {
                if (in->faseBombardeio != BOMBA_APROXIMAR && distCentro >= 2000.0f)
                    continue;
                custo -= 12000.0f;
            } else if (in->estadoIA == IA_ATACAR_DESTROIER) {
                custo -= 6000.0f;
            }
        } else {
            /* Interceptadores caçam X/A no espaço aberto; Y-Wing só quando atribuído na run. */
            if (in->tipo == YWING)
                continue;
            if (distCentro > RAIO_DEFESA)
                custo -= 12000.0f;
        }
        /* No máximo dois aliados por alvo: sem isso, 11 TIEs derretem um Y-Wing em um passo. */
        {
            int parceiros = 0;

            for (int k = 0; k < MAX_ALIADOS; k++)
                if (&g->aliados[k] != a && g->aliados[k].ativo && g->aliados[k].alvoId == in->id)
                    parceiros++;
            custo += parceiros * 800.0f;
            if (parceiros >= 2)
                custo += 50000.0f;
        }
        if (a->alvoId == in->id)
            custo -= 500.0f;
        if (custo < custoMelhor) {
            custoMelhor = custo;
            melhor = in;
        }
    }
    return melhor;
}

static Vector3 separar(Vector3 pos, Vector3 outro, int indice)
{
    Vector3 longe = Vector3Subtract(pos, outro);
    float dist = Vector3Length(longe);

    if (dist >= DIST_SEPARACAO)
        return (Vector3){ 0 };
    if (dist < 1e-4f)
        longe = (Vector3){ indice % 2 ? 1.0f : -1.0f, 0.0f, 0.0f };
    else
        longe = Vector3Scale(longe, 1.0f / dist);
    return Vector3Scale(longe, PESO_SEPARACAO * (1.0f - dist / DIST_SEPARACAO));
}

/* Antecipa o casco do Destróier e soma a normal de saída ao rumo desejado. */
static Vector3 evitarCasco(const Aliado *a, Vector3 rumo)
{
    Vector3 frente = Vector3LengthSqr(rumo) > 1e-6f ? Vector3Normalize(rumo)
                                                    : (Vector3){ 0.0f, 0.0f, 1.0f };
    float olhar = 220.0f + Vector3Length(a->vel) * 0.45f;

    for (int i = 1; i <= 2; i++) {
        Vector3 sonda = Vector3Add(a->pos, Vector3Scale(frente, olhar * (float)i * 0.5f));
        Vector3 fora = sonda;

        if (afastarDoDestroier(&fora, RAIO_ALIADO)) {
            Vector3 normal = Vector3Normalize(Vector3Subtract(fora, sonda));

            return Vector3Add(rumo, Vector3Scale(normal, 1.6f));
        }
    }
    return rumo;
}

/* Atribui os primeiros ceil(Y/2) aliados ativos aos Y-Wings que iniciaram a run. */
static void atribuirInterceptacao(Jogo *g)
{
    int idsYwing[64];
    int nY = 0, necessario, atribuidos = 0;

    /* Razão pedida: 1 TIE para 1–2 Y-Wings, 2 para 3–4, 3 para 5–6, etc. */
    for (const NoInimigo *no = g->inimigos.inicio; no && nY < 64; no = no->prox)
        if (no->dado.tipo == YWING)
            idsYwing[nY++] = no->dado.id;

    necessario = (nY + 1) / 2;
    for (int i = 0; i < MAX_ALIADOS; i++) {
        Aliado *a = &g->aliados[i];

        a->alvoYwing = -1;
        if (!a->ativo || atribuidos >= necessario)
            continue;
        a->alvoYwing = idsYwing[atribuidos % nY];
        atribuidos++;
    }
}

void atualizarAliados(Jogo *g, float dt)
{
    int roster;

    if (dt <= 0.0f)
        return;

    /* Reforços e respawn: o roster cresce até ALIADOS_MAX e nenhum aliado fica sumido para sempre.
     * Sem iniciarAliados (testes que montam aliados à mão) só processa quem já está ativo. */
    if (g->aliadosIniciados) {
        roster = tamanhoRosterAliados(g);
        for (int i = 0; i < roster; i++) {
            Aliado *a = &g->aliados[i];

            if (a->ativo)
                continue;
            if (a->tempoRespawn > 0.0f) {
                a->tempoRespawn -= dt;
                if (a->tempoRespawn > 0.0f)
                    continue;
            }
            ativarAliado(g, i);
        }
        if (contarAliadosAtivos(g) == 0)
            ativarAliado(g, 0);
    }

    atribuirInterceptacao(g);

    for (int i = 0; i < MAX_ALIADOS; i++) {
        Aliado *a = &g->aliados[i];
        const Inimigo *alvo = NULL;
        Vector3 rumo, destino, antes;

        if (!a->ativo)
            continue;
        a->cooldownTiro = fmaxf(0.0f, a->cooldownTiro - dt);

        if (a->alvoYwing >= 0)
            alvo = buscarInimigo(g, a->alvoYwing);
        if (!alvo)
            alvo = escolherAlvo(g, a);

        /* Um defensor que saiu do perímetro volta à patrulha antes de perseguir novamente. */
        if (!alvo && a->papel == ALIADO_DEFENDER && Vector3Length(a->pos) > RAIO_DEFESA + 200.0f)
            destino = pontoPatrulha(i, a->papel, g->tempoDecorrido + 3.0f);
        else if (!alvo)
            destino = pontoPatrulha(i, a->papel, g->tempoDecorrido + 3.0f);
        else
            destino = a->pos;

        if (!alvo || a->alvoId != alvo->id)
            a->fugindo = 0;
        a->alvoId = alvo ? alvo->id : -1;

        if (alvo) {
            float dist = Vector3Distance(a->pos, alvo->pos);

            if (dist < 350.0f) a->fugindo = 1;
            if (dist > 1000.0f) a->fugindo = 0;
            if (a->fugindo) {
                Vector3 longe = Vector3Normalize(Vector3Subtract(a->pos, alvo->pos));
                Vector3 lado = Vector3CrossProduct(longe, (Vector3){ 0.0f, 1.0f, 0.0f });

                if (Vector3LengthSqr(longe) < 1e-6f)
                    longe = (Vector3){ 1.0f, 0.0f, 0.0f };
                destino = Vector3Add(a->pos, Vector3Scale(Vector3Add(longe, lado), 1000.0f));
            } else {
                destino = pontoDeInterceptacao(a->pos, VEL_LASER, alvo->pos, alvo->vel);
            }
        }

        rumo = Vector3Normalize(Vector3Subtract(destino, a->pos));
        for (int k = 0; k < MAX_ALIADOS; k++)
            if (k != i && g->aliados[k].ativo)
                rumo = Vector3Add(rumo, separar(a->pos, g->aliados[k].pos, i));
        for (const NoInimigo *no = g->inimigos.inicio; no; no = no->prox)
            rumo = Vector3Add(rumo, separar(a->pos, no->dado.pos, i));
        rumo = Vector3Add(rumo, separar(a->pos, g->jogador.pos, i));
        rumo = evitarCasco(a, rumo);
        if (Vector3Length(rumo) > 1.0f)
            rumo = Vector3Normalize(rumo);
        a->vel = Vector3MoveTowards(a->vel, Vector3Scale(rumo, VEL_ALIADO), ACELERACAO_ALIADO * dt);
        a->pos = Vector3Add(a->pos, Vector3Scale(a->vel, dt));
        antes = a->pos;
        if (afastarDoDestroier(&a->pos, RAIO_ALIADO)) {
            Vector3 normal = Vector3Normalize(Vector3Subtract(a->pos, antes));
            float entrada = Vector3DotProduct(a->vel, normal);

            if (entrada < 0.0f)
                a->vel = Vector3Subtract(a->vel, Vector3Scale(normal, entrada));
        }
        if (Vector3Length(a->pos) > RAIO_ARENA) {
            Vector3 radial = Vector3Normalize(a->pos);
            float saida = Vector3DotProduct(a->vel, radial);

            a->pos = Vector3Scale(radial, RAIO_ARENA);
            if (saida > 0.0f)
                a->vel = Vector3Subtract(a->vel, Vector3Scale(radial, saida));
        }
        if (alvo && !a->fugindo && a->cooldownTiro <= TOLERANCIA_COOLDOWN
            && Vector3Distance(a->pos, alvo->pos) <= ALCANCE_TIRO_ALIADO) {
            Vector3 mira = pontoDeInterceptacao(a->pos, VEL_LASER, alvo->pos, alvo->vel);
            Vector3 direcao = Vector3Normalize(Vector3Subtract(mira, a->pos));

            if (Vector3DotProduct(Vector3Normalize(a->vel), direcao) >= COSSENO_CONE_TIRO
                && dispararLaserAliado(g, a, alvo))
                a->cooldownTiro = COOLDOWN_TIRO_ALIADO;
        }
    }
}

void danificarAliado(Jogo *g, int indice, float dano)
{
    Aliado *a;

    if (indice < 0 || indice >= MAX_ALIADOS || dano <= 0.0f)
        return;
    a = &g->aliados[indice];
    if (!a->ativo)
        return;
    a->casco = fmaxf(0.0f, a->casco - dano);
    if (a->casco == 0.0f) {
        a->ativo = 0;
        a->alvoId = -1;
        a->alvoYwing = -1;
        a->tempoRespawn = ALIADOS_TEMPO_RESPAWN;
        criarExplosao(g, a->pos, RAIO_EXPLOSAO);
        audioBoomAliado(a->pos);
    }
}
