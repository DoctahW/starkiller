# Gera src/casco_destroier.inc, o casco de colisão do Destróier, a partir de assets/models/destroier.glb:
#
#   python3 assets/models/fontes/casco_destroier.py
#
# Rodar da raiz do repositório depois de mudar o modelo (precisa do numpy). O casco vira um mapa de
# colunas: o plano x-z é dividido em células de LADO_CELULA e cada uma guarda até MAX_FAIXAS
# intervalos sólidos de y.
#
# A malha do modelo é uma casca fina, então o preenchimento por baixo/em cima sozinho deixaria o
# Destróier oco. O gerador voxeliza a superfície, marca como ar o que é alcançável de fora (fora da
# grade) e considera sólido todo o resto: o interior fechado do casco fica cheio e os vãos abertos,
# como o espaço sob as asas da ponte de comando, ficam vazios.

import json
import math
import struct

import numpy as np

ENTRADA = "assets/models/destroier.glb"
SAIDA = "src/casco_destroier.inc"
LADO_CELULA = 8.0           # u; as paredes laterais ficam até isto para fora do modelo
PASSO_AMOSTRA = LADO_CELULA / 4
MAX_FAIXAS = 4              # intervalos sólidos por coluna
GAP_MESCLA = 1              # voxels; faixas separadas por até isto são o mesmo sólido


def ler_glb(caminho):
    dados = open(caminho, "rb").read()
    n = struct.unpack("<I", dados[12:16])[0]
    gltf = json.loads(dados[20:20 + n])
    binario = 20 + n + 8

    def acessor(i, comps):
        a = gltf["accessors"][i]
        v = gltf["bufferViews"][a["bufferView"]]
        tipo = {5126: np.float32, 5123: np.uint16, 5125: np.uint32}[a["componentType"]]
        inicio = binario + v.get("byteOffset", 0) + a.get("byteOffset", 0)
        return np.frombuffer(dados, tipo, a["count"] * comps, inicio).reshape(-1, comps)

    # O conversor (destroier.py) aplica as transformações: os nós não têm matriz.
    assert all("matrix" not in nd and "translation" not in nd and "rotation" not in nd and "scale" not in nd
               for nd in gltf["nodes"]), "glb com transformação nos nós"
    triangulos = []
    for malha in gltf["meshes"]:
        for p in malha["primitives"]:
            pos = acessor(p["attributes"]["POSITION"], 3).astype(np.float64)
            idx = acessor(p["indices"], 1).reshape(-1, 3)
            triangulos.append(pos[idx])
    return np.concatenate(triangulos)


def amostrar(tri):
    """Pontos em grade baricêntrica sobre cada triângulo, para a superfície não furar os voxels."""
    a, b, c = tri[:, 0], tri[:, 1], tri[:, 2]
    aresta = np.max([np.linalg.norm(b - a, axis=1), np.linalg.norm(c - b, axis=1),
                     np.linalg.norm(a - c, axis=1)], axis=0)
    divisoes = np.maximum(1, np.ceil(aresta / PASSO_AMOSTRA)).astype(int)
    pontos = [tri.reshape(-1, 3)]
    for n in np.unique(divisoes):
        sel = divisoes == n
        i, j = np.meshgrid(np.arange(n + 1), np.arange(n + 1), indexing="ij")
        ok = i + j <= n
        u, v = (i[ok] / n)[None, :, None], (j[ok] / n)[None, :, None]
        pa, pb, pc = a[sel][:, None], b[sel][:, None], c[sel][:, None]
        pontos.append((pa + u * (pb - pa) + v * (pc - pa)).reshape(-1, 3))
    return np.concatenate(pontos)


def exterior(solido):
    """Máscara do ar alcançável da borda da grade, andando em 6 direções sem cruzar sólido."""
    fora = np.zeros_like(solido)
    fora[0, :, :] = fora[-1, :, :] = True
    fora[:, 0, :] = fora[:, -1, :] = True
    fora[:, :, 0] = fora[:, :, -1] = True
    fora &= ~solido

    while True:
        novo = fora.copy()
        novo[1:, :, :] |= fora[:-1, :, :]
        novo[:-1, :, :] |= fora[1:, :, :]
        novo[:, 1:, :] |= fora[:, :-1, :]
        novo[:, :-1, :] |= fora[:, 1:, :]
        novo[:, :, 1:] |= fora[:, :, :-1]
        novo[:, :, :-1] |= fora[:, :, 1:]
        novo &= ~solido
        if np.array_equal(novo, fora):
            return fora
        fora = novo


def faixas_da_coluna(coluna):
    """Intervalos [baixo, cima) em voxels de uma coluna sólida, de baixo para cima."""
    bordas = np.flatnonzero(np.diff(np.concatenate(([0], coluna.view(np.int8), [0]))))
    faixas = [(int(b), int(c)) for b, c in zip(bordas[::2], bordas[1::2])]
    if not faixas:
        return []

    # Junta o que está separado por até GAP_MESCLA voxels (borda de voxel, não vão de verdade).
    juntas = [faixas[0]]
    for baixo, cima in faixas[1:]:
        if baixo - juntas[-1][1] <= GAP_MESCLA:
            juntas[-1] = (juntas[-1][0], cima)
        else:
            juntas.append((baixo, cima))

    # Se houver mais que MAX_FAIXAS, fecha os menores vãos (mais sólido, nunca menos).
    while len(juntas) > MAX_FAIXAS:
        lacunas = [juntas[i + 1][0] - juntas[i][1] for i in range(len(juntas) - 1)]
        k = int(np.argmin(lacunas))
        juntas[k] = (juntas[k][0], juntas[k + 1][1])
        del juntas[k + 1]
    return juntas


def main():
    tri = ler_glb(ENTRADA)
    p = amostrar(tri)
    x0 = math.floor(p[:, 0].min() / LADO_CELULA) * LADO_CELULA
    y0 = math.floor(p[:, 1].min() / LADO_CELULA) * LADO_CELULA
    z0 = math.floor(p[:, 2].min() / LADO_CELULA) * LADO_CELULA
    ix = ((p[:, 0] - x0) // LADO_CELULA).astype(int)
    iy = ((p[:, 1] - y0) // LADO_CELULA).astype(int)
    iz = ((p[:, 2] - z0) // LADO_CELULA).astype(int)
    nx, ny, nz = ix.max() + 1, iy.max() + 1, iz.max() + 1

    superficie = np.zeros((nz, ny, nx), dtype=bool)
    superficie[iz, iy, ix] = True
    fora = exterior(superficie)
    cheio = ~fora
    print("grade %d x %d x %d; voxels de superficie %d, solidos %d"
          % (nx, ny, nz, int(superficie.sum()), int(cheio.sum())))

    # Amostras de superfície por coluna, ordenadas em y: os limites de cada faixa saem do menor e do
    # maior y amostrado nela (o voxel de 8 u sozinho deixava a colisão até 8 u para fora do casco).
    celula = iz * nx + ix
    ordem = np.lexsort((p[:, 1], celula))
    celula_ord = celula[ordem]
    y_ord = p[ordem, 1]
    unicos, inicios_amostra = np.unique(celula_ord, return_index=True)
    fins_amostra = np.append(inicios_amostra[1:], len(celula_ord))
    amostras = {int(c): (int(i), int(f))
                for c, i, f in zip(unicos, inicios_amostra, fins_amostra)}

    linhas, colunas = [], []
    vazias = 0
    max_faixas = 0
    for z in range(nz):
        cheias = np.flatnonzero(cheio[z].any(axis=0))
        if cheias.size == 0:
            linhas.append((0, 0, len(colunas)))
            continue
        primeira, ultima = int(cheias[0]), int(cheias[-1])
        linhas.append((primeira, ultima - primeira + 1, len(colunas)))
        for x in range(primeira, ultima + 1):
            faixas = faixas_da_coluna(cheio[z, :, x])
            if not faixas:
                colunas.append(((), (), 0))
                vazias += 1
                continue
            ini, fim = amostras.get(z * nx + x, (0, 0))
            ys = y_ord[ini:fim]
            baixos = [0] * MAX_FAIXAS
            cimas = [0] * MAX_FAIXAS
            for i, (baixo, cima) in enumerate(faixas):
                y_lo = y0 + baixo * LADO_CELULA
                y_hi = y0 + cima * LADO_CELULA
                sel = ys[(ys >= y_lo) & (ys <= y_hi)] if fim > ini else ys
                if sel.size and float(sel.max() - sel.min()) >= 1.0:
                    baixos[i] = math.floor(float(sel.min()))
                    cimas[i] = math.ceil(float(sel.max()))
                else:
                    # Sem amostras suficientes na faixa: mantém o intervalo dos voxels.
                    baixos[i] = math.floor(y_lo)
                    cimas[i] = math.ceil(y_hi)
            colunas.append((tuple(baixos), tuple(cimas), len(faixas)))
            max_faixas = max(max_faixas, len(faixas))

    with open(SAIDA, "w") as f:
        f.write("/* Gerado por assets/models/fontes/casco_destroier.py a partir de %s; não editar. */\n\n" % ENTRADA)
        f.write("#define LADO_CELULA_CASCO %.1ff\n" % LADO_CELULA)
        f.write("#define X0_CASCO          %.1ff\n" % x0)
        f.write("#define Z0_CASCO          %.1ff\n" % z0)
        f.write("#define NUM_LINHAS_CASCO  %d\n\n" % nz)
        f.write("/* Linha z: colunas x de `primeira` a `primeira + quantidade - 1`, a partir de COLUNAS_CASCO[inicio]. */\n")
        f.write("static const LinhaCasco LINHAS_CASCO[NUM_LINHAS_CASCO] = {\n")
        for i in range(0, len(linhas), 6):
            f.write("    " + " ".join("{ %d, %d, %d }," % l for l in linhas[i:i + 6]) + "\n")
        f.write("};\n\n/* Intervalos sólidos de y em cada coluna; n = 0 é coluna vazia. */\n")
        f.write("static const ColunaCasco COLUNAS_CASCO[%d] = {\n" % len(colunas))
        for i in range(0, len(colunas), 4):
            itens = []
            for baixos, cimas, n in colunas[i:i + 4]:
                b = ", ".join(str(v) for v in (baixos or (0,) * MAX_FAIXAS))
                c = ", ".join(str(v) for v in (cimas or (0,) * MAX_FAIXAS))
                itens.append("{ { %s }, { %s }, %d }" % (b, c, n))
            f.write("    " + ", ".join(itens) + ",\n")
        f.write("};\n")
    print("casco: %d pontos, grade %d x %d, %d colunas (%d vazias, até %d faixas) em %s"
          % (len(p), nx, nz, len(colunas), vazias, max_faixas, SAIDA))


main()
