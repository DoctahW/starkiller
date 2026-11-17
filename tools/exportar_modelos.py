"""Exporta a sonda de validação de eixos, escala e UV com Blender em background."""
import json
import math
from pathlib import Path
import bpy

RAIZ = Path(__file__).resolve().parents[1]
PASTA = RAIZ / "assets/models"
PASTA.mkdir(parents=True, exist_ok=True)

def coord(p):
    x, y, z = p
    return x, -z, y


def material(nome, cor):
    m = bpy.data.materials.new(nome)
    m.diffuse_color = (*cor, 1)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*cor, 1)
    bsdf.inputs["Roughness"].default_value = 0.7
    return m


def preparar(obj, mat):
    obj.data.materials.append(mat)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return obj


def caixa(nome, p, tamanho, mat, giro=0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=coord(p))
    o = bpy.context.object
    o.name = nome
    o.dimensions = tamanho[0], tamanho[2], tamanho[1]
    preparar(o, mat)
    o.rotation_euler[1] = math.radians(-giro)
    bevel = o.modifiers.new("Chanfros", "BEVEL")
    bevel.width = min(tamanho) * 0.12
    bevel.segments = 2
    bpy.ops.object.modifier_apply(modifier=bevel.name)
    return o


def limpar():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)


def exportar(nome, raio=None):
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    # Uma malha por material, reduzindo draw calls e mantendo cores.
    bpy.context.view_layer.objects.active = next(o for o in bpy.context.scene.objects if o.type == "MESH")
    bpy.ops.object.join()
    o = bpy.context.object
    tri = o.modifiers.new("Triangulos", "TRIANGULATE")
    bpy.ops.object.modifier_apply(modifier=tri.name)
    triangulos = len(o.data.polygons)
    assert triangulos <= 5000, (nome, triangulos)
    if raio is not None:
        assert max(v.co.length for v in o.data.vertices) <= raio, nome
    caminho = PASTA / f"{nome}.glb"
    bpy.ops.export_scene.gltf(filepath=str(caminho), export_format="GLB",
                              use_selection=True, export_yup=True,
                              export_texcoords=True, export_normals=True,
                              export_materials="EXPORT")
    return {"arquivo": caminho.name, "triangulos": triangulos,
            "raio_maximo": round(max(v.co.length for v in o.data.vertices), 3)}


vermelho = material("Frente +Z", (0.72, 0.055, 0.035))
amarelo = material("Cima +Y", (0.93, 0.56, 0.07))
relatorio = []

# Sonda assimétrica com textura embutida: permite verificar eixos, UV e escala.
limpar()
teste = material("Grade UV", (1,1,1))
img = bpy.data.images.new("Grade UV", width=32, height=32)
pixels = []
for y in range(32):
    for x in range(32):
        pixels.extend((1, 0.18, 0.05, 1) if (x//8+y//8)%2 else (0.1,0.6,1,1))
img.pixels = pixels
img.pack()
tex = teste.node_tree.nodes.new("ShaderNodeTexImage")
tex.image = img
teste.node_tree.links.new(tex.outputs["Color"], teste.node_tree.nodes.get("Principled BSDF").inputs["Base Color"])
caixa("Corpo 2x2x4", (0,0,0), (2,2,4), teste)
caixa("Frente +Z", (0,0,3), (0.5,0.5,2), vermelho)
caixa("Cima +Y", (0,2,0), (0.5,2,0.5), amarelo)
relatorio.append(exportar("teste_eixos"))
(PASTA / "manifesto.json").write_text(json.dumps(relatorio, indent=2) + "\n")
print(json.dumps(relatorio, indent=2))
