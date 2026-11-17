#version 330

// Naves da arena: difusa do sol mais um ambiente, que deixa o lado na sombra legível.
// Luz e ambiente vêm do render.c, que usa os mesmos valores nas primitivas.

in vec2 fragTexCoord;
in vec3 fragNormal;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 dirLuz;     // unitário, da superfície para o sol, no mundo
uniform vec3 corLuz;
uniform vec3 ambiente;

out vec4 finalColor;

void main()
{
    vec3 base = texture(texture0, fragTexCoord).rgb * colDiffuse.rgb;
    float luz = max(dot(normalize(fragNormal), dirLuz), 0.0);
    finalColor = vec4(base * (ambiente + corLuz * luz), 1.0);
}
