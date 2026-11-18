#include "audio.h"

#include <math.h>
#include <stddef.h>

#include "destroier.h"
#include "jogo.h"
#include "nave.h"
#include "raymath.h"

#define PASTA_SONS    "assets/sounds/"
#define PASTA_MENU    "assets/sounds/menu/"
#define PASTA_MUSICAS "assets/sounds/musics/"

#define TAU_VOLUME    0.08f   /* s; suavização dos loops (crossfade dos setores de motor) */
#define TAU_MUSICA    0.25f   /* s; crossfade entre faixas de fundo */
#define VOZES_TECLA   4

/* Volumes base dos sons de interface (antes dos volumes geral/efeitos). */
#define VOLUME_UI_MOVER       0.55f
#define VOLUME_UI_SELECIONAR  0.75f
#define VOLUME_UI_VOLTAR      0.65f
#define VOLUME_UI_TECLA       0.45f

/* Configuração viva; a tela de opções altera os campos e chama audioAplicarVolumes(). */
static ConfigAudio configAudio = {
    AUDIO_VOLUME_GERAL_PADRAO,
    AUDIO_VOLUME_EFEITOS_PADRAO,
    AUDIO_VOLUME_MUSICA_PADRAO
};

/*
 * Alcances (u) das fontes posicionais. Os sons de voo, SPIN e BOOM dos caças ganharam
 * 500 u para o combate próximo do Destróier não ficar mudo logo atrás do jogador.
 */
#define ALCANCE_ENGINE_INIMIGO     2350.0f
#define ALCANCE_SPIN_INIMIGO       2350.0f
#define ALCANCE_BOOM_INIMIGO       2400.0f
#define ALCANCE_TIRO_INIMIGO       4000.0f
#define ALCANCE_ISD                9000.0f
#define ALCANCE_AFTERBURNER        1500.0f
#define ALCANCE_ALARME             12000.0f

/* Doppler: "som" bem acima das naves (até 550 u/s) para o fator não explodir. */
#define VELOCIDADE_SOM_JOGO 2500.0f
#define PITCH_DOPPLER_MIN   0.72f
#define PITCH_DOPPLER_MAX   1.35f
#define TAU_PITCH           0.06f
/*
 * Com o alcance maior as fontes distantes passam a ser ouvidas; o deslocamento de pitch
 * delas perde força para não competir com as naves próximas.
 */
#define FORCA_DOPPLER_MINIMA 0.35f

#define VOZES_POR_TIPO 3
#define SPIN_VOZES 12
#define MAX_VARIANTES  20
#define MAX_FONTES_MOTOR 64
#define NUM_MOTORES_JOGADOR 5
#define NUM_CABINE_JOGADOR  2

#define FRACAO_CASCO_CRITICO  0.25f
#define INTERVALO_AVISO_CASCO 0.75f

/*
 * Barramento dos sons do jogador (motor, cabine, tiros e avisos). O TIE está sempre no centro
 * da mixagem; sem esse corte o motor e os tiros abafam os inimigos posicionais.
 */
#define VOLUME_JOGADOR        0.68f

/* Volumes dos eventos (antes dos volumes geral/efeitos). */
#define VOLUME_TIRO_JOGADOR   (0.80f * VOLUME_JOGADOR)
#define VOLUME_TIRO_INIMIGO   0.85f
#define VOLUME_ACELERACAO     (0.45f * VOLUME_JOGADOR)
#define VOLUME_CABINE         (0.55f * VOLUME_JOGADOR)
#define VOLUME_MOTOR_JOGADOR  (0.70f * VOLUME_JOGADOR)
#define VOLUME_MOTOR_INIMIGO  1.00f
#define VOLUME_SPIN_INIMIGO   1.00f
#define VOLUME_BOOM_INIMIGO   1.00f

#define VOLUME_DANO_JOGADOR   (0.75f * VOLUME_JOGADOR)
#define VOLUME_COLISAO_JOGADOR (0.80f * VOLUME_JOGADOR)
#define VOLUME_ALVO_JOGADOR   (0.60f * VOLUME_JOGADOR)
#define VOLUME_AVISO_CASCO    (0.70f * VOLUME_JOGADOR)
#define VOLUME_RUIDO_CASCO    (0.60f * VOLUME_JOGADOR)
#define VOLUME_SPIN_JOGADOR   (0.90f * VOLUME_JOGADOR)
#define VOLUME_BOOM_JOGADOR   (0.95f * VOLUME_JOGADOR)

/* Alarme de casco baixo do Destróier: precisa atravessar o motor do TIE e a música. */
#define VOLUME_ALARME_ISD     1.00f

/*
 * BOOM com curva mais suave que os demais pontuais: a explosão carrega mais longe mesmo
 * com o volume base já no teto.
 */
#define CURVA_BOOM_INIMIGO    1.50f

typedef struct {
    Sound s[MAX_VARIANTES];
    int n;
} Banco;

/* Loop com volume e pitch suavizados: a troca de setor e o Doppler não estalam. */
typedef struct {
    Music m;
    int carregado;
    int tocando;
    float volume;
    float pitch;
} Loop;

/* Faixa de fundo em loop, com crossfade entre a faixa atual e a anterior. */
typedef struct {
    Music m;
    int carregado;
    int tocando;
    float volume;
} MusicaFundo;

typedef struct {
    int grupo;
    int id;
} ChaveFonte;

typedef struct {
    ChaveFonte chave;   /* grupo/id; id 0 dentro do grupo = voz livre */
    Loop principal;     /* idle do X-Wing ou motor único do Y/A */
    Loop thrusters;     /* só X-Wing */
} VozMotor;

/* Fonte móvel registrada no frame (inimigos hoje, aliados no futuro). */
typedef struct {
    ChaveFonte chave;
    TipoNave tipo;
    Vector3 pos, vel;
} FonteMotor;

typedef struct {
    float volume;
    float pan;
} VozEspacial;

static int audioPronto;

/* Ouvinte: atualizado uma vez por frame por audioAtualizar. */
static Vector3 ouvintePos;
static Vector3 ouvinteVel;
static Vector3 ouvinteDireita = { -1.0f, 0.0f, 0.0f };

/* Bancos de one-shots. */
static Banco bancoTiroJogador;
static Banco bancoTiroX;
static Banco bancoTiroY;
static Sound tiroAwing;
static Banco bancoColisao;
static Banco bancoDano;
static Banco bancoAlvo;
static Banco bancoLowHp;
static Banco bancoAceleracao;
static Banco bancoFrenagem;
static Sound avisoLowHp;
static Banco bancoBoomJogador;
static Sound spinJogador;
static Banco bancoBoomInimigo[3];
static Sound spinInimigoBase[3];
static Sound spinInimigoVoz[3][SPIN_VOZES];
static Sound isdDestroyed;
static Sound isdShieldEnable;
static Sound isdShieldDisable;

/* Sons de interface. uiTecla usa um pequeno pool para não cortar digitação rápida. */
static Sound uiMover;
static Sound uiSelecionar;
static Sound uiVoltar;
static Sound uiTeclaBase;
static Sound uiTeclaVoz[VOZES_TECLA];
static int proximaTecla;

/* Músicas de fundo, indexadas por FaixaMusica - 1. */
static MusicaFundo musicas[3];
static FaixaMusica faixaAtual = AUDIO_MUSICA_NENHUMA;
static float fatorMusica = 1.0f;

/* Loops. */
static Loop motoresJogador[NUM_MOTORES_JOGADOR];
static Loop cabineJogador[NUM_CABINE_JOGADOR];
static Loop motorIsd;
static Loop posQueimadorIsd;
static Loop alarmeIsd;
static VozMotor vozes[3][VOZES_POR_TIPO];

/* Fontes móveis registradas neste frame e temporizadores. */
static FonteMotor fontes[MAX_FONTES_MOTOR];
static int nFontes;
static float lowHpRuidoRestante;
static float avisoRestante;
static float aceleracaoAntes;
static int destroierEstavaVivo = 1;
static int proximoTiroJogador;

static Sound carregarSom(const char *caminho)
{
    Sound vazio = { 0 };
    Sound s;

    if (!audioPronto)
        return vazio;
    if (!FileExists(caminho)) {
        TraceLog(LOG_WARNING, "AUDIO: %s ausente", caminho);
        return vazio;
    }
    s = LoadSound(caminho);
    if (!IsSoundValid(s)) {
        TraceLog(LOG_WARNING, "AUDIO: %s não carregou", caminho);
        return vazio;
    }
    return s;
}

static void carregarBanco(Banco *b, const char *padrao, int quantidade)
{
    int i;

    b->n = 0;
    for (i = 0; i < quantidade && b->n < MAX_VARIANTES; i++) {
        Sound s = carregarSom(TextFormat(padrao, i + 1));

        if (IsSoundValid(s))
            b->s[b->n++] = s;
    }
}

static void liberarSom(Sound *s)
{
    if (IsSoundValid(*s))
        UnloadSound(*s);
    *s = (Sound){ 0 };
}

static void liberarBanco(Banco *b)
{
    int i;

    for (i = 0; i < b->n; i++)
        liberarSom(&b->s[i]);
    b->n = 0;
}

/* Cada caça abatido precisa de uma voz própria de SPIN para poder pará-la no BOOM. */
static void carregarSpins(TipoNave tipo, const char *caminho)
{
    int i;

    spinInimigoBase[tipo] = carregarSom(caminho);
    if (!IsSoundValid(spinInimigoBase[tipo]))
        return;
    for (i = 0; i < SPIN_VOZES; i++)
        spinInimigoVoz[tipo][i] = LoadSoundAlias(spinInimigoBase[tipo]);
}

static void liberarSpins(TipoNave tipo)
{
    int i;

    for (i = 0; i < SPIN_VOZES; i++) {
        if (IsSoundValid(spinInimigoVoz[tipo][i]))
            UnloadSoundAlias(spinInimigoVoz[tipo][i]);
        spinInimigoVoz[tipo][i] = (Sound){ 0 };
    }
    liberarSom(&spinInimigoBase[tipo]);
}

static Sound somAleatorio(const Banco *b)
{
    if (b->n <= 0)
        return (Sound){ 0 };
    return b->s[GetRandomValue(0, b->n - 1)];
}

static float aleatorioEntre(float minimo, float maximo)
{
    return minimo + (maximo - minimo) * ((float)GetRandomValue(0, 10000) / 10000.0f);
}

static void carregarLoop(Loop *l, const char *caminho)
{
    if (!audioPronto)
        return;
    if (!FileExists(caminho)) {
        TraceLog(LOG_WARNING, "AUDIO: %s ausente (loop)", caminho);
        return;
    }
    l->m = LoadMusicStream(caminho);
    if (!IsMusicValid(l->m)) {
        TraceLog(LOG_WARNING, "AUDIO: %s não carregou (loop)", caminho);
        l->m = (Music){ 0 };
        return;
    }
    l->m.looping = 1;
    l->carregado = 1;
    l->tocando = 0;
    l->volume = 0.0f;
    l->pitch = 1.0f;
}

static void liberarLoop(Loop *l)
{
    if (!l->carregado)
        return;
    StopMusicStream(l->m);
    UnloadMusicStream(l->m);
    l->m = (Music){ 0 };
    l->carregado = 0;
    l->tocando = 0;
    l->volume = 0.0f;
    l->pitch = 1.0f;
}

static void loopAtualizarComPitch(Loop *l, float alvo, float pan, float pitchAlvo, float dt)
{
    float k;

    if (!l->carregado)
        return;
    if (alvo < 0.0f)
        alvo = 0.0f;
    alvo *= configAudio.geral * configAudio.efeitos;
    if (pan < -1.0f)
        pan = -1.0f;
    if (pan > 1.0f)
        pan = 1.0f;
    if (pitchAlvo < PITCH_DOPPLER_MIN)
        pitchAlvo = PITCH_DOPPLER_MIN;
    if (pitchAlvo > PITCH_DOPPLER_MAX)
        pitchAlvo = PITCH_DOPPLER_MAX;

    if (alvo <= 0.0001f && l->volume <= 0.002f) {
        l->volume = 0.0f;
        if (l->tocando) {
            StopMusicStream(l->m);
            l->tocando = 0;
        }
        return;
    }

    if (!l->tocando) {
        l->volume = 0.0f;
        l->pitch = pitchAlvo;   /* começa já na afinação certa */
        PlayMusicStream(l->m);
        l->tocando = 1;
    }

    k = 1.0f - expf(-dt / TAU_VOLUME);
    l->volume += (alvo - l->volume) * k;
    if (l->volume < 0.0f)
        l->volume = 0.0f;

    {
        float kp = 1.0f - expf(-dt / TAU_PITCH);

        l->pitch += (pitchAlvo - l->pitch) * kp;
    }

    SetMusicVolume(l->m, l->volume);
    SetMusicPan(l->m, pan);
    SetMusicPitch(l->m, l->pitch);
    UpdateMusicStream(l->m);
}

static void loopAtualizar(Loop *l, float alvo, float pan, float dt)
{
    loopAtualizarComPitch(l, alvo, pan, 1.0f, dt);
}

static void carregarMusica(MusicaFundo *mf, const char *caminho)
{
    if (!audioPronto)
        return;
    if (!FileExists(caminho)) {
        TraceLog(LOG_WARNING, "AUDIO: %s ausente (música)", caminho);
        return;
    }

    mf->m = LoadMusicStream(caminho);
    if (!IsMusicValid(mf->m)) {
        TraceLog(LOG_WARNING, "AUDIO: %s não carregou (música)", caminho);
        mf->m = (Music){ 0 };
        return;
    }

    mf->m.looping = 1;
    mf->carregado = 1;
    mf->tocando = 0;
    mf->volume = 0.0f;
}

static void liberarMusica(MusicaFundo *mf)
{
    if (!mf->carregado)
        return;
    StopMusicStream(mf->m);
    UnloadMusicStream(mf->m);
    mf->m = (Music){ 0 };
    mf->carregado = 0;
    mf->tocando = 0;
    mf->volume = 0.0f;
}

/*
 * A faixa atual sobe até o alvo; todas as outras descem e param. Assim a troca de tela
 * é um crossfade curto e o duck da pausa só muda o alvo, sem reiniciar a música.
 */
static void atualizarMusicas(float dt)
{
    float alvoAtual = 0.0f;
    float k;
    int i;

    if (!audioPronto)
        return;

    if (faixaAtual != AUDIO_MUSICA_NENHUMA) {
        alvoAtual = configAudio.geral * configAudio.musica * fatorMusica;
        if (alvoAtual < 0.0f)
            alvoAtual = 0.0f;
    }

    k = 1.0f - expf(-dt / TAU_MUSICA);
    for (i = 0; i < 3; i++) {
        MusicaFundo *mf = &musicas[i];
        int eAtual = (int)faixaAtual == i + 1;
        float alvo = eAtual ? alvoAtual : 0.0f;

        if (!mf->carregado)
            continue;

        if (eAtual && !mf->tocando) {
            mf->volume = 0.0f;
            PlayMusicStream(mf->m);
            mf->tocando = 1;
        }

        mf->volume += (alvo - mf->volume) * k;
        if (mf->volume < 0.0f)
            mf->volume = 0.0f;
        if (mf->volume > 1.0f)
            mf->volume = 1.0f;

        SetMusicVolume(mf->m, mf->volume);
        if (mf->tocando) {
            UpdateMusicStream(mf->m);
            if (!eAtual && mf->volume <= 0.002f) {
                StopMusicStream(mf->m);
                mf->tocando = 0;
                mf->volume = 0.0f;
            }
        }
    }
}

static void tocar(Sound s, float volume, float pan)
{
    if (!audioPronto || !IsSoundValid(s))
        return;
    if (volume < 0.0f)
        volume = 0.0f;
    if (volume > 1.0f)
        volume = 1.0f;
    if (pan < -1.0f)
        pan = -1.0f;
    if (pan > 1.0f)
        pan = 1.0f;

    SetSoundVolume(s, volume * configAudio.geral * configAudio.efeitos);
    SetSoundPan(s, pan);
    PlaySound(s);
}

/* Volume e pan de uma fonte em `pos`, com atenuação até `alcance` e curva configurável. */
static VozEspacial espacialComCurva(Vector3 pos, float alcance, float volumeBase, float curva)
{
    VozEspacial v = { 0.0f, 0.0f };
    Vector3 rel = Vector3Subtract(pos, ouvintePos);
    float dist = Vector3Length(rel);
    float atenuacao;

    if (dist >= alcance)
        return v;
    atenuacao = 1.0f - dist / alcance;
    if (curva != 1.0f)
        atenuacao = powf(atenuacao, curva);
    v.volume = volumeBase * atenuacao;

    if (dist > 1e-3f) {
        float lado = Vector3DotProduct(Vector3Scale(rel, 1.0f / dist), ouvinteDireita);

        if (lado < -1.0f)
            lado = -1.0f;
        if (lado > 1.0f)
            lado = 1.0f;
        v.pan = lado * 0.75f;   /* nunca vai 100% para um lado */
    }
    return v;
}

/* Curva quadrática: sons pontuais e fontes grandes (Destróier) somem mais cedo. */
static VozEspacial espacial(Vector3 pos, float alcance, float volumeBase)
{
    return espacialComCurva(pos, alcance, volumeBase, 2.0f);
}

static void tocarEspacialComCurva(Sound s, Vector3 pos, float base, float alcance, float curva)
{
    VozEspacial v = espacialComCurva(pos, alcance, base, curva);

    if (v.volume > 0.005f)
        tocar(s, v.volume, v.pan);
}

static void tocarEspacial(Sound s, Vector3 pos, float base, float alcance)
{
    tocarEspacialComCurva(s, pos, base, alcance, 2.0f);
}

float audioFatorDoppler(Vector3 pos, Vector3 vel)
{
    Vector3 rel = Vector3Subtract(pos, ouvintePos);
    float dist = Vector3Length(rel);
    float radial, fator, forcaDoppler;

    if (dist < 1.0f)   /* em cima do ouvinte a direção é instável */
        return 1.0f;

    rel = Vector3Scale(rel, 1.0f / dist);
    radial = Vector3DotProduct(Vector3Subtract(vel, ouvinteVel), rel);
    if (radial > 0.8f * VELOCIDADE_SOM_JOGO)
        radial = 0.8f * VELOCIDADE_SOM_JOGO;
    if (radial < -0.8f * VELOCIDADE_SOM_JOGO)
        radial = -0.8f * VELOCIDADE_SOM_JOGO;

    fator = VELOCIDADE_SOM_JOGO / (VELOCIDADE_SOM_JOGO + radial);

    /* Fonte distante (o alcance novo a deixa audível) desloca menos o pitch. */
    forcaDoppler = 1.0f - dist / ALCANCE_ENGINE_INIMIGO;
    if (forcaDoppler < FORCA_DOPPLER_MINIMA)
        forcaDoppler = FORCA_DOPPLER_MINIMA;
    fator = 1.0f + (fator - 1.0f) * forcaDoppler;

    if (fator < PITCH_DOPPLER_MIN)
        fator = PITCH_DOPPLER_MIN;
    if (fator > PITCH_DOPPLER_MAX)
        fator = PITCH_DOPPLER_MAX;
    return fator;
}

void audioMotorRegistrar(GrupoMotor grupo, int id, TipoNave tipo, Vector3 pos, Vector3 vel)
{
    int i;

    if (!audioPronto || id == 0)
        return;

    for (i = 0; i < nFontes; i++)
        if (fontes[i].chave.grupo == (int)grupo && fontes[i].chave.id == id) {
            fontes[i].tipo = tipo;
            fontes[i].pos = pos;
            fontes[i].vel = vel;
            return;
        }

    if (nFontes >= MAX_FONTES_MOTOR)
        return;
    fontes[nFontes].chave.grupo = (int)grupo;
    fontes[nFontes].chave.id = id;
    fontes[nFontes].tipo = tipo;
    fontes[nFontes].pos = pos;
    fontes[nFontes].vel = vel;
    nFontes++;
}

static int vozLivre(int tipo)
{
    int k;

    for (k = 0; k < VOZES_POR_TIPO; k++)
        if (vozes[tipo][k].chave.id == 0)
            return k;
    return -1;
}

static int mesmaChave(ChaveFonte a, ChaveFonte b)
{
    return a.grupo == b.grupo && a.id == b.id;
}

static int temVoz(int tipo, ChaveFonte chave)
{
    int k;

    for (k = 0; k < VOZES_POR_TIPO; k++)
        if (mesmaChave(vozes[tipo][k].chave, chave))
            return 1;
    return 0;
}

static int contemChave(const ChaveFonte *chaves, int n, ChaveFonte chave)
{
    int i;

    for (i = 0; i < n; i++)
        if (mesmaChave(chaves[i], chave))
            return 1;
    return 0;
}

static FonteMotor *buscarFonte(ChaveFonte chave)
{
    int i;

    for (i = 0; i < nFontes; i++)
        if (mesmaChave(fontes[i].chave, chave))
            return &fontes[i];
    return NULL;
}

/*
 * Vozes de motor de todas as fontes móveis registradas no frame. Vale para inimigos e, no
 * futuro, aliados: as mais próximas de cada tipo ocupam as vozes; as demais desvanecem.
 */
static void atualizarVozesMotores(float dt)
{
    int t, k;

    for (t = 0; t < 3; t++) {
        ChaveFonte desejados[VOZES_POR_TIPO];
        int nDesejados = 0;

        /* As fontes mais próximas do tipo, dentro do alcance. */
        while (nDesejados < VOZES_POR_TIPO) {
            FonteMotor *melhor = NULL;
            float melhorDist = ALCANCE_ENGINE_INIMIGO;
            int i;

            for (i = 0; i < nFontes; i++) {
                FonteMotor *f = &fontes[i];
                float dist;

                if ((int)f->tipo != t || contemChave(desejados, nDesejados, f->chave))
                    continue;
                dist = Vector3Distance(f->pos, ouvintePos);
                if (dist < melhorDist) {
                    melhorDist = dist;
                    melhor = f;
                }
            }
            if (melhor == NULL)
                break;
            desejados[nDesejados++] = melhor->chave;
        }

        /* Solta a voz de quem saiu das mais próximas (ou da lista/alcance). */
        for (k = 0; k < VOZES_POR_TIPO; k++)
            if (vozes[t][k].chave.id != 0
                && !contemChave(desejados, nDesejados, vozes[t][k].chave)) {
                vozes[t][k].chave.grupo = 0;
                vozes[t][k].chave.id = 0;
            }

        /* Ocupa as vozes livres com os desejados que ainda não têm voz. */
        for (k = 0; k < nDesejados; k++) {
            int slot;

            if (temVoz(t, desejados[k]))
                continue;
            slot = vozLivre(t);
            if (slot < 0)
                break;
            vozes[t][slot].chave = desejados[k];
        }
    }

    /* Volume/pan/Doppler de cada voz; as livres desvanecem. */
    for (t = 0; t < 3; t++)
        for (k = 0; k < VOZES_POR_TIPO; k++) {
            VozMotor *v = &vozes[t][k];
            FonteMotor *f = v->chave.id ? buscarFonte(v->chave) : NULL;

            if (f == NULL) {
                loopAtualizar(&v->principal, 0.0f, 0.0f, dt);
                loopAtualizar(&v->thrusters, 0.0f, 0.0f, dt);
                continue;
            }

            {
                VozEspacial e = espacialComCurva(f->pos, ALCANCE_ENGINE_INIMIGO,
                                                 VOLUME_MOTOR_INIMIGO, 1.0f);
                float pitch = audioFatorDoppler(f->pos, f->vel);

                if (t == XWING) {
                    /* X-Wing: idle + thrusters; o thrusters um pouco mais baixo. */
                    loopAtualizarComPitch(&v->principal, e.volume * 0.80f, e.pan, pitch, dt);
                    loopAtualizarComPitch(&v->thrusters, e.volume * 0.60f, e.pan, pitch, dt);
                } else {
                    loopAtualizarComPitch(&v->principal, e.volume, e.pan, pitch, dt);
                    loopAtualizarComPitch(&v->thrusters, 0.0f, 0.0f, pitch, dt);
                }
            }
        }

    nFontes = 0;
}

static void silenciarVozesMotores(float dt)
{
    int t, k;

    for (t = 0; t < 3; t++)
        for (k = 0; k < VOZES_POR_TIPO; k++) {
            vozes[t][k].chave.grupo = 0;
            vozes[t][k].chave.id = 0;
            loopAtualizar(&vozes[t][k].principal, 0.0f, 0.0f, dt);
            loopAtualizar(&vozes[t][k].thrusters, 0.0f, 0.0f, dt);
        }
    nFontes = 0;
}

/* Setores de velocidade 0..4 em crossfade linear; cabine em 2 setores. */
static void atualizarMotoresJogador(const Jogo *g, float dt)
{
    float t = (g->jogador.velocidade - VEL_MIN) / (VEL_MAX - VEL_MIN);
    float posicao;
    float fracao;
    int setor, k;

    if (t < 0.0f)
        t = 0.0f;
    if (t > 1.0f)
        t = 1.0f;

    posicao = t * (float)(NUM_MOTORES_JOGADOR - 1);
    setor = (int)posicao;
    if (setor >= NUM_MOTORES_JOGADOR - 1)
        setor = NUM_MOTORES_JOGADOR - 2;
    fracao = posicao - (float)setor;

    for (k = 0; k < NUM_MOTORES_JOGADOR; k++) {
        float alvo = 0.0f;

        if (k == setor)
            alvo = (1.0f - fracao) * VOLUME_MOTOR_JOGADOR;
        else if (k == setor + 1)
            alvo = fracao * VOLUME_MOTOR_JOGADOR;
        loopAtualizar(&motoresJogador[k], alvo, 0.0f, dt);
    }

    for (k = 0; k < NUM_CABINE_JOGADOR; k++) {
        float alvo = (k == 0 ? 1.0f - t : t) * VOLUME_CABINE;

        loopAtualizar(&cabineJogador[k], alvo, 0.0f, dt);
    }
}

static void silenciarMotoresJogador(float dt)
{
    int k;

    for (k = 0; k < NUM_MOTORES_JOGADOR; k++)
        loopAtualizar(&motoresJogador[k], 0.0f, 0.0f, dt);
    for (k = 0; k < NUM_CABINE_JOGADOR; k++)
        loopAtualizar(&cabineJogador[k], 0.0f, 0.0f, dt);
}

void iniciarAudio(void)
{
    int k, i;

    if (audioPronto)
        return;

    audioConfigPadroes(&configAudio);
    audioConfigCarregar(&configAudio, CAMINHO_CONFIG_AUDIO);

    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        TraceLog(LOG_WARNING, "AUDIO: dispositivo indisponível; o jogo segue sem som");
        CloseAudioDevice();
        return;
    }
    audioPronto = 1;

    carregarBanco(&bancoTiroJogador, PASTA_SONS "tie/weapon_%d.ogg", 2);
    carregarBanco(&bancoTiroX, PASTA_SONS "rebel/weapon_%d.ogg", 2);
    carregarBanco(&bancoTiroY, PASTA_SONS "rebel/missile_%d.ogg", 2);
    tiroAwing = carregarSom(PASTA_SONS "rebel/weapon_awing.ogg");

    carregarBanco(&bancoColisao, PASTA_SONS "tie/collision_%d.ogg", 6);
    carregarBanco(&bancoDano, PASTA_SONS "tie/shot_%02d.ogg", 20);
    carregarBanco(&bancoAlvo, PASTA_SONS "tie/target_%d.ogg", 6);
    /* TELEMETRY_RANDOM fica reservado para uso futuro: os arquivos continuam em
     * assets/sounds/tie/telemetry_*.ogg, mas não são carregados nem tocados agora. */
    carregarBanco(&bancoLowHp, PASTA_SONS "tie/low_hp_%d.ogg", 7);
    avisoLowHp = carregarSom(PASTA_SONS "tie/low_hp_warning.ogg");
    carregarBanco(&bancoAceleracao, PASTA_SONS "tie/accel_%d.ogg", 4);
    carregarBanco(&bancoFrenagem, PASTA_SONS "tie/decel_%d.ogg", 4);

    spinJogador = carregarSom(PASTA_SONS "tie/spin.ogg");
    carregarBanco(&bancoBoomJogador, PASTA_SONS "tie/boom_%d.ogg", 4);
    carregarSpins(XWING, PASTA_SONS "rebel/spin_xwing.ogg");
    carregarSpins(YWING, PASTA_SONS "rebel/spin_ywing.ogg");
    carregarSpins(AWING, PASTA_SONS "rebel/spin_awing.ogg");
    carregarBanco(&bancoBoomInimigo[XWING], PASTA_SONS "rebel/boom_xwing_%d.ogg", 2);
    carregarBanco(&bancoBoomInimigo[YWING], PASTA_SONS "rebel/boom_ywing_%d.ogg", 2);
    carregarBanco(&bancoBoomInimigo[AWING], PASTA_SONS "rebel/boom_awing_%d.ogg", 2);

    isdDestroyed = carregarSom(PASTA_SONS "isd/destroyed.ogg");
    isdShieldEnable = carregarSom(PASTA_SONS "isd/shield_enable.ogg");
    isdShieldDisable = carregarSom(PASTA_SONS "isd/shield_disable.ogg");

    for (k = 0; k < NUM_MOTORES_JOGADOR; k++)
        carregarLoop(&motoresJogador[k], TextFormat(PASTA_SONS "tie/engine_%d.ogg", k + 1));
    carregarLoop(&cabineJogador[0], PASTA_SONS "tie/cabin_low.ogg");
    carregarLoop(&cabineJogador[1], PASTA_SONS "tie/cabin_high.ogg");

    carregarLoop(&motorIsd, TextFormat(PASTA_SONS "isd/engine_%d.ogg", GetRandomValue(1, 5)));
    carregarLoop(&posQueimadorIsd, PASTA_SONS "isd/afterburner.ogg");
    carregarLoop(&alarmeIsd, PASTA_SONS "isd/alarm.ogg");

    for (k = 0; k < VOZES_POR_TIPO; k++) {
        carregarLoop(&vozes[XWING][k].principal, PASTA_SONS "rebel/engine_xwing_idle.ogg");
        carregarLoop(&vozes[XWING][k].thrusters, PASTA_SONS "rebel/engine_xwing_thrusters.ogg");
        carregarLoop(&vozes[YWING][k].principal, PASTA_SONS "rebel/engine_ywing.ogg");
        carregarLoop(&vozes[AWING][k].principal, PASTA_SONS "rebel/engine_awing.ogg");
    }

    uiMover = carregarSom(PASTA_MENU "ui_menuMove.wav");
    uiSelecionar = carregarSom(PASTA_MENU "ui_menuSelect.wav");
    uiVoltar = carregarSom(PASTA_MENU "ui_menuBack.wav");
    uiTeclaBase = carregarSom(PASTA_MENU "ui_keyboardtype.wav");
    proximaTecla = 0;
    if (IsSoundValid(uiTeclaBase)) {
        for (i = 0; i < VOZES_TECLA; i++)
            uiTeclaVoz[i] = LoadSoundAlias(uiTeclaBase);
    }

    carregarMusica(&musicas[AUDIO_MUSICA_MENU - 1], PASTA_MUSICAS "menuMusic2.mp3");
    carregarMusica(&musicas[AUDIO_MUSICA_COMBATE - 1], PASTA_MUSICAS "in_gameMusic.mp3");
    carregarMusica(&musicas[AUDIO_MUSICA_RANKING - 1], PASTA_MUSICAS "ranking_menuMusic.mp3");

    ouvintePos = (Vector3){ 0.0f, 0.0f, 0.0f };
    ouvinteVel = (Vector3){ 0.0f, 0.0f, 0.0f };
    ouvinteDireita = (Vector3){ -1.0f, 0.0f, 0.0f };
    nFontes = 0;
    lowHpRuidoRestante = aleatorioEntre(1.2f, 3.2f);
    avisoRestante = 0.0f;
    aceleracaoAntes = 0.0f;
    destroierEstavaVivo = 1;
    proximoTiroJogador = 0;
}

void encerrarAudio(void)
{
    int k, t, i;

    if (!audioPronto)
        return;

    for (k = 0; k < NUM_MOTORES_JOGADOR; k++)
        liberarLoop(&motoresJogador[k]);
    for (k = 0; k < NUM_CABINE_JOGADOR; k++)
        liberarLoop(&cabineJogador[k]);
    liberarLoop(&motorIsd);
    liberarLoop(&posQueimadorIsd);
    liberarLoop(&alarmeIsd);
    for (t = 0; t < 3; t++)
        for (k = 0; k < VOZES_POR_TIPO; k++) {
            liberarLoop(&vozes[t][k].principal);
            liberarLoop(&vozes[t][k].thrusters);
            vozes[t][k].chave.grupo = 0;
            vozes[t][k].chave.id = 0;
        }

    liberarBanco(&bancoTiroJogador);
    liberarBanco(&bancoTiroX);
    liberarBanco(&bancoTiroY);
    liberarSom(&tiroAwing);
    liberarBanco(&bancoColisao);
    liberarBanco(&bancoDano);
    liberarBanco(&bancoAlvo);
    liberarBanco(&bancoLowHp);
    liberarSom(&avisoLowHp);
    liberarBanco(&bancoAceleracao);
    liberarBanco(&bancoFrenagem);
    liberarSom(&spinJogador);
    liberarBanco(&bancoBoomJogador);
    for (t = 0; t < 3; t++) {
        liberarSpins(t);
        liberarBanco(&bancoBoomInimigo[t]);
    }
    liberarSom(&isdDestroyed);
    liberarSom(&isdShieldEnable);
    liberarSom(&isdShieldDisable);

    liberarSom(&uiMover);
    liberarSom(&uiSelecionar);
    liberarSom(&uiVoltar);
    for (i = 0; i < VOZES_TECLA; i++) {
        if (IsSoundValid(uiTeclaVoz[i]))
            UnloadSoundAlias(uiTeclaVoz[i]);
        uiTeclaVoz[i] = (Sound){ 0 };
    }
    liberarSom(&uiTeclaBase);

    for (i = 0; i < 3; i++)
        liberarMusica(&musicas[i]);
    faixaAtual = AUDIO_MUSICA_NENHUMA;
    fatorMusica = 1.0f;

    audioPronto = 0;
    CloseAudioDevice();
}

int audioDisponivel(void)
{
    return audioPronto;
}

void audioAtualizar(const Jogo *g, const Comandos *cmd, int emCombate, float dt)
{
    float fracaoCasco;

    if (g == NULL)
        return;

    /* O ouvinte também é atualizado sem dispositivo: os testes de Doppler usam isso. */
    ouvintePos = g->jogador.pos;
    ouvinteVel = Vector3Scale(g->jogador.frente, g->jogador.velocidade);
    ouvinteDireita = g->jogador.direita;

    if (!audioPronto)
        return;
    if (dt < 0.0f)
        dt = 0.0f;
    if (dt > 0.25f)
        dt = 0.25f;

    atualizarMusicas(dt);

    /* Destruição do Destróier: dispara uma vez, na transição de vivo para destruído. */
    if (g->cascoDestroier > 0.0f) {
        destroierEstavaVivo = 1;
    } else if (emCombate && destroierEstavaVivo) {
        audioDestroierDestruido();
        destroierEstavaVivo = 0;
    }

    if (emCombate && g->jogador.casco > 0.0f)
        atualizarMotoresJogador(g, dt);
    else
        silenciarMotoresJogador(dt);

    /* Aceleração/frenagem só na borda do comando: segurar Shift não repete o som. */
    if (emCombate && cmd != NULL && g->jogador.casco > 0.0f) {
        if (cmd->aceleracao > 0.1f && aceleracaoAntes <= 0.1f)
            tocar(somAleatorio(&bancoAceleracao), VOLUME_ACELERACAO, 0.0f);
        if (cmd->aceleracao < -0.1f && aceleracaoAntes >= -0.1f)
            tocar(somAleatorio(&bancoFrenagem), VOLUME_ACELERACAO, 0.0f);
        aceleracaoAntes = cmd->aceleracao;
    } else {
        aceleracaoAntes = 0.0f;
    }

    /* Destróier: motor, pós-combustor e alarme de casco baixo (< 25%). */
    if (emCombate && g->cascoDestroier > 0.0f) {
        Vector3 popa = { 0.0f, 0.0f, Z_POPA_DESTROIER * ESCALA_DESTROIER };
        VozEspacial motor = espacial(popa, ALCANCE_ISD, 0.8f);
        VozEspacial queimador = espacial(popa, ALCANCE_AFTERBURNER, 0.95f);

        loopAtualizar(&motorIsd, motor.volume, motor.pan, dt);
        loopAtualizar(&posQueimadorIsd, queimador.volume, queimador.pan, dt);

        if (g->cascoDestroier <= CASCO_MAX_DESTROIER * FRACAO_CASCO_CRITICO) {
            VozEspacial alarme = espacial((Vector3){ 0.0f, 0.0f, 0.0f }, ALCANCE_ALARME,
                                          VOLUME_ALARME_ISD);

            loopAtualizar(&alarmeIsd, alarme.volume, alarme.pan, dt);
        } else {
            loopAtualizar(&alarmeIsd, 0.0f, 0.0f, dt);
        }
    } else {
        loopAtualizar(&motorIsd, 0.0f, 0.0f, dt);
        loopAtualizar(&posQueimadorIsd, 0.0f, 0.0f, dt);
        loopAtualizar(&alarmeIsd, 0.0f, 0.0f, dt);
    }

    /* Cabine: avisos de casco baixo (o TELEMETRY_RANDOM está desligado por enquanto). */
    fracaoCasco = g->jogador.casco / CASCO_JOGADOR;
    if (emCombate && g->jogador.casco > 0.0f) {
        if (fracaoCasco < FRACAO_CASCO_CRITICO) {
            avisoRestante -= dt;
            if (avisoRestante <= 0.0f) {
                tocar(avisoLowHp, VOLUME_AVISO_CASCO, 0.0f);
                avisoRestante = INTERVALO_AVISO_CASCO;
            }
            lowHpRuidoRestante -= dt;
            if (lowHpRuidoRestante <= 0.0f) {
                tocar(somAleatorio(&bancoLowHp), VOLUME_RUIDO_CASCO, 0.0f);
                lowHpRuidoRestante = aleatorioEntre(1.2f, 3.2f);
            }
        } else {
            avisoRestante = 0.0f;
            lowHpRuidoRestante = aleatorioEntre(1.2f, 3.2f);
        }
    } else {
        avisoRestante = 0.0f;
        lowHpRuidoRestante = aleatorioEntre(1.2f, 3.2f);
    }

    /* Registra as fontes móveis do frame; inimigos hoje, aliados no futuro. */
    if (emCombate) {
        NoInimigo *no;

        for (no = g->inimigos.inicio; no != NULL; no = no->prox)
            audioMotorRegistrar(AUDIO_MOTOR_INIMIGO, no->dado.id, no->dado.tipo,
                                no->dado.pos, no->dado.vel);
        atualizarVozesMotores(dt);
    } else {
        silenciarVozesMotores(dt);
    }
}

void audioTiroJogador(void)
{
    if (bancoTiroJogador.n <= 0)
        return;
    tocar(bancoTiroJogador.s[proximoTiroJogador], VOLUME_TIRO_JOGADOR, 0.0f);
    proximoTiroJogador = (proximoTiroJogador + 1) % bancoTiroJogador.n;
}

void audioTiroInimigo(TipoNave tipo, Vector3 pos)
{
    Sound s = { 0 };

    if (tipo == XWING)
        s = somAleatorio(&bancoTiroX);
    else if (tipo == YWING)
        s = somAleatorio(&bancoTiroY);
    else if (tipo == AWING)
        s = tiroAwing;

    tocarEspacial(s, pos, VOLUME_TIRO_INIMIGO, ALCANCE_TIRO_INIMIGO);
}

void audioDanoJogador(void)
{
    tocar(somAleatorio(&bancoDano), VOLUME_DANO_JOGADOR, 0.0f);
}

void audioColisaoJogador(void)
{
    tocar(somAleatorio(&bancoColisao), VOLUME_COLISAO_JOGADOR, 0.0f);
}

void audioTrocaAlvo(void)
{
    tocar(somAleatorio(&bancoAlvo), VOLUME_ALVO_JOGADOR, 0.0f);
}

int audioSpinInimigo(TipoNave tipo, Vector3 pos)
{
    VozEspacial v;
    int i;

    if (!audioPronto || (int)tipo < 0 || (int)tipo > 2)
        return -1;

    v = espacial(pos, ALCANCE_SPIN_INIMIGO, VOLUME_SPIN_INIMIGO);
    if (v.volume <= 0.005f)
        return -1;

    for (i = 0; i < SPIN_VOZES; i++) {
        Sound s = spinInimigoVoz[tipo][i];

        if (IsSoundValid(s) && !IsSoundPlaying(s)) {
            tocar(s, v.volume, v.pan);
            return i;
        }
    }
    return -1;
}

void audioBoomInimigo(TipoNave tipo, Vector3 pos, int vozSpin)
{
    if (!audioPronto || (int)tipo < 0 || (int)tipo > 2)
        return;

    if (vozSpin >= 0 && vozSpin < SPIN_VOZES
        && IsSoundValid(spinInimigoVoz[tipo][vozSpin]))
        StopSound(spinInimigoVoz[tipo][vozSpin]);

    tocarEspacialComCurva(somAleatorio(&bancoBoomInimigo[tipo]), pos, VOLUME_BOOM_INIMIGO,
                          ALCANCE_BOOM_INIMIGO, CURVA_BOOM_INIMIGO);
}

void audioSpinJogador(void)
{
    tocar(spinJogador, VOLUME_SPIN_JOGADOR, 0.0f);
}

void audioBoomJogador(Vector3 pos)
{
    (void)pos;   /* a explosão é do próprio ouvinte: centro, sem atenuação */
    if (IsSoundValid(spinJogador))
        StopSound(spinJogador);
    tocar(somAleatorio(&bancoBoomJogador), VOLUME_BOOM_JOGADOR, 0.0f);
}

void audioPararSpins(void)
{
    int t, i;

    if (!audioPronto)
        return;
    if (IsSoundValid(spinJogador))
        StopSound(spinJogador);
    for (t = 0; t < 3; t++)
        for (i = 0; i < SPIN_VOZES; i++)
            if (IsSoundValid(spinInimigoVoz[t][i]))
                StopSound(spinInimigoVoz[t][i]);
}

void audioDestroierDestruido(void)
{
    tocar(isdDestroyed, 1.0f, 0.0f);
}

void audioEscudoDestroier(int ligado)
{
    tocarEspacial(ligado ? isdShieldEnable : isdShieldDisable,
                  (Vector3){ 0.0f, 0.0f, 0.0f }, 0.9f, ALCANCE_ISD);
}

static float limitarVolume(float v)
{
    if (!(v >= AUDIO_VOLUME_MIN))   /* também pega NaN */
        return AUDIO_VOLUME_MIN;
    if (v > AUDIO_VOLUME_MAX)
        return AUDIO_VOLUME_MAX;
    return v;
}

ConfigAudio *audioConfig(void)
{
    return &configAudio;
}

void audioAplicarVolumes(void)
{
    configAudio.geral = limitarVolume(configAudio.geral);
    configAudio.efeitos = limitarVolume(configAudio.efeitos);
    configAudio.musica = limitarVolume(configAudio.musica);
}

int audioSalvarConfiguracao(void)
{
    return audioConfigSalvar(&configAudio, CAMINHO_CONFIG_AUDIO);
}

void audioMusicaTocar(FaixaMusica faixa, float fator)
{
    if (faixa < AUDIO_MUSICA_NENHUMA || faixa > AUDIO_MUSICA_RANKING)
        faixa = AUDIO_MUSICA_NENHUMA;
    if (!(fator >= 0.0f))           /* também pega NaN */
        fator = 0.0f;
    if (fator > 1.0f)
        fator = 1.0f;

    faixaAtual = faixa;
    fatorMusica = fator;
}

void audioUIMover(void)
{
    tocar(uiMover, VOLUME_UI_MOVER, 0.0f);
}

void audioUISelecionar(void)
{
    tocar(uiSelecionar, VOLUME_UI_SELECIONAR, 0.0f);
}

void audioUIVoltar(void)
{
    tocar(uiVoltar, VOLUME_UI_VOLTAR, 0.0f);
}

void audioUITecla(void)
{
    if (!audioPronto || !IsSoundValid(uiTeclaBase))
        return;

    tocar(uiTeclaVoz[proximaTecla], VOLUME_UI_TECLA, 0.0f);
    proximaTecla = (proximaTecla + 1) % VOZES_TECLA;
}
