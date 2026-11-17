# Gera src/casco_destroier.inc, o casco de colisão do Destróier, a partir de assets/models/destroier.glb:
#
#   python3 assets/models/fontes/casco_destroier.py
#
# Rodar da raiz do repositório depois de mudar o modelo (precisa do numpy). O casco vira um mapa
# de alturas: o plano x-z é dividido em células de LADO_CELULA e cada uma guarda o intervalo de y
# que a superfície do modelo ocupa nela (de baixo até em cima, cheio por dentro). O Destróier é
# uma cunha sem nada por baixo de nada, então o mapa fica rente ao casco; a exceção é o vão sob
# as asas da ponte de comando, que conta como casco.

import json
import math
import struct

import numpy as np

ENTRADA = "assets/models/destroier.glb"
SAIDA = "src/casco_destroier.inc"
LADO_CELULA = 8.0           # u; as paredes laterais ficam até isto para fora do modelo
PASSO_AMOSTRA = LADO_CELULA / 3


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
    """Pontos em grade baricêntrica sobre cada triângulo, a no máximo PASSO_AMOSTRA uns dos outros."""
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


def main():
    p = amostrar(ler_glb(ENTRADA))
    x0 = math.floor(p[:, 0].min() / LADO_CELULA) * LADO_CELULA
    z0 = math.floor(p[:, 2].min() / LADO_CELULA) * LADO_CELULA
    ix = ((p[:, 0] - x0) // LADO_CELULA).astype(int)
    iz = ((p[:, 2] - z0) // LADO_CELULA).astype(int)
    nx, nz = ix.max() + 1, iz.max() + 1
    baixo = np.full((nz, nx), np.inf)
    cima = np.full((nz, nx), -np.inf)
    np.minimum.at(baixo, (iz, ix), p[:, 1])
    np.maximum.at(cima, (iz, ix), p[:, 1])

    linhas, colunas = [], []
    for z in range(nz):
        cheias = np.nonzero(np.isfinite(baixo[z]))[0]
        if len(cheias) == 0:
            linhas.append((0, 0, len(colunas)))
            continue
        primeira, ultima = cheias[0], cheias[-1]
        linhas.append((primeira, ultima - primeira + 1, len(colunas)))
        for x in range(primeira, ultima + 1):
            if np.isfinite(baixo[z, x]):
                colunas.append((math.floor(baixo[z, x]), math.ceil(cima[z, x])))
            else:
                colunas.append((1, 0))   # buraco: baixo > cima

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
        f.write("};\n\n/* Intervalo de y ocupado em cada coluna; baixo > cima é coluna vazia. */\n")
        f.write("static const ColunaCasco COLUNAS_CASCO[%d] = {\n" % len(colunas))
        for i in range(0, len(colunas), 10):
            f.write("    " + " ".join("{ %d, %d }," % c for c in colunas[i:i + 10]) + "\n")
        f.write("};\n")
    print("casco: %d pontos, grade %d x %d, %d colunas (%.0f%% cheias) em %s"
          % (len(p), nx, nz, len(colunas), 100.0 * sum(b <= c for b, c in colunas) / len(colunas), SAIDA))


main()
