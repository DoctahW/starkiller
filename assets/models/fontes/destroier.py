# Gera assets/models/destroier.glb (Destróier Estelar, VIS-03) a partir do modelo do Sketchfab
# "Star Wars: Imperial II Star Destroyer", de Daniel (CC BY 4.0, créditos no README):
# https://sketchfab.com/3d-models/star-wars-imperial-ii-star-destroyer-b8bd2d35f7604670ab85242c06c6d280
#
#   blender -b --factory-startup -P assets/models/fontes/destroier.py -- <star_wars_imperial_ii_star_destroyer.glb>
#
# Rodar da raiz do repositório. Seguindo a convenção do README (seção "Modelos 3D"):
#   - escala para o comprimento do jogo (Z_POPA..Z_PROA de include/destroier.h), proa em +Z;
#   - menos triângulos (Decimate) até TRIANGULOS_ALVO;
#   - sem mapas de normal (a raylib não os usa), só o primeiro mapa UV e texturas em PNG
#     (a raylib padrão não lê JPEG);
#   - materiais emissivos (motores e luzes) ganham cor base = cor do emissivo e uma textura
#     emissiva 1x1: a raylib só lê o emissivo de glTF com textura, e o render.c desenha
#     sem luz os materiais que a têm.
# Depois de rodar, gerar de novo o casco de colisão: python3 assets/models/fontes/casco_destroier.py

import os
import sys
import tempfile

import bpy
from mathutils import Matrix, Vector

SAIDA = os.path.join(os.getcwd(), "assets", "models", "destroier.glb")

Z_PROA, Z_POPA = 823.0, -777.0       # include/destroier.h: 1600 u, o comprimento canônico em metros
TRIANGULOS_ALVO = 60000
MINIMO_DECIMATE = 2000                # peças com menos triângulos que isto não passam pelo Decimate


def entrada():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if len(argv) != 1:
        sys.exit("uso: blender -b --factory-startup -P %s -- <arquivo.glb do Sketchfab>" % __file__)
    return argv[0]


def malhas():
    return [o for o in bpy.context.scene.objects if o.type == "MESH"]


def triangulos(objs):
    return sum(len(p.vertices) - 2 for o in objs for p in o.data.polygons)


def achatar_hierarquia():
    """Tira os pais (mantendo a posição no mundo) e apaga os vazios do Sketchfab."""
    for o in malhas():
        mundo = o.matrix_world.copy()
        o.parent = None
        o.matrix_world = mundo
    for o in [o for o in bpy.context.scene.objects if o.type != "MESH"]:
        bpy.data.objects.remove(o)


def caixa_mundo(objs):
    """Caixa exata pelos vértices: bound_box de objeto girado (Greebles1 está a 45°) sai maior."""
    pontos = [o.matrix_world @ v.co for o in objs for v in o.data.vertices]
    lo = Vector((min(p[i] for p in pontos) for i in range(3)))
    hi = Vector((max(p[i] for p in pontos) for i in range(3)))
    return lo, hi


def encaixar_no_jogo(objs):
    """No Blender a proa do jogo (+Z) é -Y. Escala para o comprimento do jogo e põe popa e proa no lugar."""
    lo, hi = caixa_mundo(objs)
    escala = (Z_PROA - Z_POPA) / (hi.y - lo.y)
    # Popa (+Y no Blender) vai para -Y = Z_POPA do jogo; x centrado; altura (Z) mantém a origem do autor,
    # que fica no plano da parede lateral do casco.
    deslocamento = Vector((-(lo.x + hi.x) / 2 * escala, -Z_POPA - hi.y * escala, 0.0))
    m = Matrix.Translation(deslocamento) @ Matrix.Scale(escala, 4)
    # O original já tem a proa em +Z do glTF (-Y no Blender): não precisa girar.
    for o in objs:
        o.matrix_world = m @ o.matrix_world
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return escala


def texturizado(o):
    return any(n.type == "TEX_IMAGE" for m in o.data.materials if m and m.node_tree for n in m.node_tree.nodes)


def reduzir(objs, alvo):
    """
    Decimate só nos detalhes pesados sem textura: casco e placas (texturizados) e peças pequenas
    ficam intactos, porque o Decimate neles deixa lascas e estraga o UV.
    """
    total = triangulos(objs)
    candidatos = [o for o in objs if triangulos([o]) > MINIMO_DECIMATE and not texturizado(o)]
    fixos = total - triangulos(candidatos)
    proporcao = min(1.0, max(alvo - fixos, 0) / triangulos(candidatos))
    for o in candidatos:
        mod = o.modifiers.new("decimate", "DECIMATE")
        mod.ratio = proporcao
        bpy.context.view_layer.objects.active = o
        bpy.ops.object.modifier_apply(modifier=mod.name)
    return total


def ajustar_materiais():
    branco = bpy.data.images.new("emissivo", 1, 1, alpha=False)
    branco.pixels.foreach_set([1.0, 1.0, 1.0, 1.0])
    branco.file_format = "PNG"
    branco.pack()

    for m in bpy.data.materials:
        if not m.node_tree:
            continue
        nos, ligacoes = m.node_tree.nodes, m.node_tree.links
        bsdf = next((n for n in nos if n.type == "BSDF_PRINCIPLED"), None)
        if bsdf is None:
            continue
        for n in [n for n in nos if n.type == "NORMAL_MAP"]:
            for t in [l.from_node for l in n.inputs["Color"].links]:
                nos.remove(t)
            nos.remove(n)

        emissao = bsdf.inputs["Emission Color"]
        forca = bsdf.inputs["Emission Strength"].default_value
        cor = list(emissao.default_value)
        if emissao.is_linked or forca <= 0.0 or max(cor[:3]) <= 0.0:
            continue
        bsdf.inputs["Base Color"].default_value = cor
        tex = nos.new("ShaderNodeTexImage")
        tex.image = branco
        ligacoes.new(tex.outputs["Color"], emissao)


def so_primeiro_uv(objs):
    """As texturas do original usam TEXCOORD_0; a raylib lê no máximo dois e avisa dos outros."""
    for o in objs:
        while len(o.data.uv_layers) > 1:
            o.data.uv_layers.remove(o.data.uv_layers[-1])


def texturas_em_png():
    """O exportador mantém o formato de origem; salva cada imagem não PNG como PNG e troca as referências."""
    pasta = tempfile.mkdtemp()
    for n, img in enumerate([i for i in bpy.data.images if i.file_format != "PNG"]):
        caminho = os.path.join(pasta, "%d.png" % n)
        img.save(filepath=caminho)
        nova = bpy.data.images.load(caminho)
        nova.pack()
        img.user_remap(nova)


def main():
    origem = entrada()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=origem)
    achatar_hierarquia()
    objs = malhas()
    lo, hi = caixa_mundo(objs)
    print("destroier: original %d triângulos, caixa %s .. %s" % (triangulos(objs), tuple(lo), tuple(hi)))

    escala = encaixar_no_jogo(objs)
    total = reduzir(objs, TRIANGULOS_ALVO)
    ajustar_materiais()
    so_primeiro_uv(objs)
    texturas_em_png()
    lo, hi = caixa_mundo(objs)
    print("destroier: escala %.4f, %d -> %d triângulos" % (escala, total, triangulos(objs)))
    # Caixa nas coordenadas do jogo (x, y = Z do Blender, z = -Y do Blender).
    print("destroier: caixa no jogo x %.1f..%.1f y %.1f..%.1f z %.1f..%.1f"
          % (lo.x, hi.x, lo.z, hi.z, -hi.y, -lo.y))

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.export_scene.gltf(filepath=SAIDA, export_format="GLB", use_selection=True,
                              export_yup=True, export_apply=True, export_materials="EXPORT",
                              export_image_format="AUTO", export_animations=False)
    print("destroier: exportado em", SAIDA)


main()
