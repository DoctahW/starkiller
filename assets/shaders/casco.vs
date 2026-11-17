#version 330

// Normais no mundo para a luz; atributos e uniforms com os nomes padrão da raylib.
// O Destróier é desenhado na origem, sem rotação nem escala, então a normal do objeto
// já é a normal do mundo: não usar matNormal, que a raylib monta em espaço de câmera.
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;

uniform mat4 mvp;

out vec2 fragTexCoord;
out vec3 fragNormal;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragNormal = vertexNormal;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
