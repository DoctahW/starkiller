#version 330

// Esfera centrada na câmera do céu: a posição do vértice já é a direção de vista.
in vec3 vertexPosition;

uniform mat4 mvp;

out vec3 fragDir;

void main()
{
    fragDir = vertexPosition;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
