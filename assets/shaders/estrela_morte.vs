#version 330

// Normais no mundo para a luz; atributos e uniforms com os nomes padrão da raylib.
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;

uniform mat4 mvp;
uniform mat4 matNormal;

out vec2 fragTexCoord;
out vec3 fragNormal;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragNormal = vec3(matNormal * vec4(vertexNormal, 0.0));
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
