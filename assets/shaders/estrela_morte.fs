#version 330

// Um sol de lado: metade da esfera fica na sombra, quase do preto do espaço, e só se vê
// o recorte dela tapando as estrelas. A névoa azul tira contraste do lado iluminado.

in vec2 fragTexCoord;
in vec3 fragNormal;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 dirLuz;   // unitário, da superfície para o sol, no mundo

out vec4 finalColor;

const vec3 COR_LUZ  = vec3(0.60, 0.65, 0.75);
const vec3 AMBIENTE = vec3(0.02, 0.03, 0.06);
const vec3 NEVOA    = vec3(0.04, 0.06, 0.12);
const float FORCA_NEVOA = 0.35;

void main()
{
    vec3 base = texture(texture0, fragTexCoord).rgb * colDiffuse.rgb;
    float luz = smoothstep(-0.1, 0.5, dot(normalize(fragNormal), dirLuz));   // terminador suave
    vec3 cor = base * (AMBIENTE + COR_LUZ * luz);
    finalColor = vec4(mix(cor, NEVOA, FORCA_NEVOA), 1.0);
}
