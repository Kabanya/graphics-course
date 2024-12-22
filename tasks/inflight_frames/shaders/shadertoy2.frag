#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D iChannel0;
layout(binding = 1) uniform sampler2D iChannel1;
layout(binding = 2, set = 0) uniform params {
  uvec2 iResolution;
  uvec2 iMouse;
  float iTime;
};

const int   maxSteps = 70;
const float eps      = 0.01;
const float maxDist = 15.0;
const vec3  wLightDir = normalize(vec3(-1, -1, -1));

mat4 rotateX(float theta) {
    float c = cos(theta);
    float s = sin(theta);
    return mat4(
        vec4(1, 0, 0, 0),
        vec4(0, c, -s, 0),
        vec4(0, s, c, 0),
        vec4(0, 0, 0, 1)
    );
}

mat4 rotateY(float theta) {
    float c = cos(theta);
    float s = sin(theta);
    return mat4(
        vec4(c, 0, s, 0),
        vec4(0, 1, 0, 0),
        vec4(-s, 0, c, 0),
        vec4(0, 0, 0, 1)
    );
}

mat4 translate(vec3 offs){
    return mat4(
        vec4(1, 0, 0, 0),
        vec4(0, 1, 0, 0),
        vec4(0, 0, 1, 0),
        vec4(offs.x, offs.y, offs.z, 1)
        );
}

vec3 transformPoint(in mat4 m, in vec3 v) {
    return (m * vec4(v, 1)).xyz;
}

vec3 transformVector(in mat4 m, in vec3 v) {
    return (m * vec4(v, 0)).xyz;
}

float smin(float a, float b, float k) {
    float h = max(k - abs(a - b), 0.0) / k;
    return min(a, b) - h * h * h * k * (1.0 / 6.0);
}

float sdRoundedCylinder(vec3 p, float ra, float rb, float h) {
    vec2 d = vec2(length(p.xz) - 2.0 * ra + rb, abs(p.y) - h);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - rb;
}

float dTorus(vec3 p, vec2 t) {
    vec2 q = vec2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}

float dSphere(vec3 p, float r) {
    return length(p) - r;
}

float sdfPlane(vec3 p, vec4 n) {
    return dot(p, n.xyz) + n.w;
}

vec2 sdf(in vec3 p) {

    const float ORBIT_RADIUS = 2.0;
    const float SMOOTH_FACTOR = 1.0;
    const vec4 PLANE_NORMAL = vec4(0.0, 0.34, 0.0, 1.0);

    float moveSpeed = sin(0.0 * 1.5) * 2.4;
    float orbitAngle = 0.0;
    float ballHeight = sin(0.0) * 1.5;

    // Positions
    vec3 spherePos = vec3(0.0, moveSpeed, 0.0);
    vec3 torusPos = ORBIT_RADIUS * vec3(cos(orbitAngle), 0.0, sin(orbitAngle));
    vec3 ballPos = torusPos + vec3(0.0, ballHeight, 0.0);

    float cylinderSdf = sdRoundedCylinder(p, 0.5, 0.2, 0.8);
    float sphereSdf = dSphere(p - spherePos, 0.6);
    float torusSdf = dTorus(p - torusPos, vec2(0.6, 0.15));
    float ballSdf = dSphere(p - ballPos, 0.2);
    float planeSdf = sdfPlane(p, PLANE_NORMAL);

    float blendedSdf = smin(cylinderSdf, sphereSdf, SMOOTH_FACTOR);

    float minSdf = min(blendedSdf, min(planeSdf, min(torusSdf, ballSdf)));
    float id;

    if (minSdf == blendedSdf) {
        id = (cylinderSdf < sphereSdf) ? 1.0 : 2.0;
    } else if (minSdf == planeSdf) {
        id = 3.0;
    } else if (minSdf == torusSdf) {
        id = 4.0;
    } else {
        id = 5.0;
    }

    return vec2(minSdf, id);
}

vec3 trace(in vec3 from, in vec3 dir, out bool hit, out float id) {
    vec3 p = from;
    float totalDist = 0.0;
    id = -1.0;
    hit = false;

    for (int steps = 0; steps < maxSteps; steps++) {
        vec2 distAndID = sdf(p);
        float dist = abs(distAndID.x);

        if (dist < eps) {
            hit = true;
            id = distAndID.y;
            break;
        }

        totalDist += dist;
        if (totalDist > maxDist) {
            break;
        }
        p += dist * dir;
    }
    return p;
}

vec3 generateNormal(vec3 z) {
    float e = max(0.01, eps);
    float dx1 = sdf(z + vec3(e, 0, 0)).x;
    float dx2 = sdf(z - vec3(e, 0, 0)).x;
    float dy1 = sdf(z + vec3(0, e, 0)).x;
    float dy2 = sdf(z - vec3(0, e, 0)).x;
    float dz1 = sdf(z + vec3(0, 0, e)).x;
    float dz2 = sdf(z - vec3(0, 0, e)).x;

    return normalize(vec3(dx1 - dx2, dy1 - dy2, dz1 - dz2));
}

void mainImage(in vec2 fragCoord, vec2 iResolution) {
    vec3 iMouse = vec3(0.0, 0.0, 0.0);
    fragCoord.y = -fragCoord.y;
    iResolution.y = -iResolution.y;

    bool hit;
    float id;
    vec3 cEye = vec3(0, 0, 10);
    vec3 mouse = vec3(iMouse.xy / iResolution.xy - 0.5, iMouse.z - 0.5);
    mat4 camToWorld =
        translate(vec3(0, 0, 0))
        * rotateY(6.0 * mouse.x)
        * rotateX(6.0 * -mouse.y)
        * translate(vec3(0, 0, -1));
    mat4 worldToCam       = inverse(camToWorld);
    mat4 normalCamToWorld = transpose(worldToCam);
    mat4 normalWorldToCam = transpose(normalCamToWorld);

    vec2 scale = 9.0 * iResolution.xy / max(iResolution.x, iResolution.y);
    vec3 cPixel = vec3(scale * (fragCoord / iResolution.xy - vec2(0.5)), 1);
    vec3 wDir = normalize(transformVector(normalCamToWorld, normalize(cPixel - cEye)));
    vec3 wEye = transformPoint(camToWorld, cEye);
    vec3 wSurf = trace(wEye, wDir, hit, id);

    vec3 color = texture(iChannel1, wDir.xy).rgb;

    if (hit) {
        vec3 wNorm = generateNormal(wSurf);
        vec3 wViewDir = normalize(wEye - wSurf);
        vec3 wLightDir = normalize(vec3(1.0, 1.0, -1.0));

        vec3 objColor;

        if (id == 1.0) {
            vec3 w = abs(wNorm);
            w /= (w.x + w.y + w.z);
            objColor = w.x * texture(iChannel0, wSurf.yz).rgb
                     + w.y * texture(iChannel0, wSurf.xz).rgb
                     + w.z * texture(iChannel0, wSurf.xy).rgb;
        } else if (id == 2.0) {
            vec3 w = abs(wNorm);
            w /= (w.x + w.y + w.z);
            objColor = w.x * texture(iChannel0, wSurf.yz).rgb
                     + w.y * texture(iChannel0, wSurf.xz).rgb
                     + w.z * texture(iChannel0, wSurf.xy).rgb;

            objColor = mix(vec3(1.0, 0.0, 0.0), vec3(1.0, 1.0, 0.0),
                           wSurf.y * 0.5 + 0.5) * (1.0 - (wSurf.y * 0.5 + 0.5) * (wSurf.y * 0.5 + 0.5));
        } else if (id == 4.0) {
            objColor = vec3(0.0, 0.5, 1.0);
        } else if (id == 5.0) {
            objColor = vec3(0.8, 0.4, 0.0);
        } else {
            vec2 uv = wSurf.xz * 0.5 + 0.5;
            objColor = texture(iChannel0, uv).rgb;
        }

        float ambientStrength = 0.1;
        vec3 ambient = ambientStrength * objColor;

        float diffuseStrength = 0.8;
        vec3 diffuse = diffuseStrength * objColor * max(dot(wNorm, wLightDir), 0.0);

        float specularStrength = 0.5;
        float specularShine = 32.0;

        vec3 reflectDir = reflect(-wLightDir, wNorm);
        float spec = pow(max(dot(wViewDir, reflectDir), 0.0), specularShine);
        vec3 specular = specularStrength * spec * vec3(1.0, 1.0, 1.0);

        bool shadowHit;
        float shadowId;
        vec3 wShadSurf = trace(wSurf + wNorm * 0.01, wLightDir, shadowHit, shadowId);

        float shadowFactor = shadowHit ? 0.5 : 1.0;
        color = ambient + shadowFactor * (diffuse + specular);
    }

    fragColor = vec4(color, 1.0);
}

void main()
{
  vec2 fragCoord = gl_FragCoord.xy;

  vec2 iResolution = vec2(1280.0, 720.0);
  mainImage(fragCoord, iResolution);
}
