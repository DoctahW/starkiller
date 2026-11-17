#version 330

// Ruído de valor 3D em fBm, com o domínio entortado por outro fBm para dar fiapos. A densidade
// se concentra numa faixa inclinada que passa pela Estrela da Morte. Escura de propósito: as
// estrelas continuam por cima. O ruído de 1/255 no fim evita faixas no degradê de 8 bits.

in vec3 fragDir;

out vec4 finalColor;

const vec3 NORMAL_FAIXA = vec3(0.22, 0.95, -0.15);
const vec3 AZUL  = vec3(0.10, 0.18, 0.40);
const vec3 CIANO = vec3(0.08, 0.30, 0.42);
const vec3 ROXO  = vec3(0.30, 0.12, 0.40);
const float INTENSIDADE = 0.65;

float hash(vec3 p)
{
    p = fract(p * 0.3183099 + 0.1) * 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float ruido(vec3 x)
{
    vec3 i = floor(x), f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash(i), hash(i + vec3(1, 0, 0)), f.x),
                   mix(hash(i + vec3(0, 1, 0)), hash(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(hash(i + vec3(0, 0, 1)), hash(i + vec3(1, 0, 1)), f.x),
                   mix(hash(i + vec3(0, 1, 1)), hash(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

float fbm(vec3 p)
{
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) {
        s += a * ruido(p);
        p = p * 2.03 + vec3(1.7, 9.2, 3.1);
        a *= 0.5;
    }
    return s;
}

void main()
{
    vec3 d = normalize(fragDir);
    vec3 q = d * 2.2;
    vec3 torto = vec3(fbm(q + vec3(5.2)), fbm(q + vec3(1.3, 7.1, 2.9)), fbm(q + vec3(8.4, 0.6, 4.4)));
    float w = fbm(q + 2.0 * torto);
    float faixa = exp(-pow(dot(d, normalize(NORMAL_FAIXA)) / 0.4, 2.0));
    float dens = smoothstep(0.3, 0.75, w) * (0.2 + 0.8 * faixa);
    vec3 cor = mix(AZUL, CIANO, smoothstep(0.4, 0.7, fbm(q * 1.7 + 3.0)));
    cor = mix(cor, ROXO, smoothstep(0.55, 0.75, fbm(q * 0.8 - 4.0)));
    vec3 c = cor * dens * INTENSIDADE + (hash(vec3(gl_FragCoord.xy, 0.0)) - 0.5) / 255.0;
    finalColor = vec4(max(c, 0.0), 1.0);
}
