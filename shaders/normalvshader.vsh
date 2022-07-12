#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;


out vec3 FragPos;
out vec3 Normal;
out vec4 preScreenPosition;
out vec4 nowScreenPosition;


uniform mat4 view;
uniform mat4 projection;

uniform mat4 pre_viewproj;
uniform int screen_width;//resolution width
uniform int screen_height;//resolution wheight
uniform uint frameCounter;

const vec2 Halton_2_3[8] = vec2[](
    vec2(0.0f, -1.0f / 3.0f),
    vec2(-1.0f / 2.0f, 1.0f / 3.0f),
    vec2(1.0f / 2.0f, -7.0f / 9.0f),
    vec2(-3.0f / 4.0f, -1.0f / 9.0f),
    vec2(1.0f / 4.0f, 5.0f / 9.0f),
    vec2(-1.0f / 4.0f, -5.0f / 9.0f),
    vec2(3.0f / 4.0f, 1.0f / 9.0f),
    vec2(-7.0f / 8.0f, 7.0f / 9.0f)
    );

void main()
{


    uint offsetIdx = frameCounter % 8u;
    float deltaWidth = 1.0 / screen_width, deltaHeight = 1.0 / screen_height;
    vec2 jitter = vec2(
        Halton_2_3[offsetIdx].x * deltaWidth,
        Halton_2_3[offsetIdx].y * deltaHeight
    );

    vec4 worldPos = vec4(aPos, 1.0);
    mat4 jitterMat = projection;
    jitterMat[2][0] += jitter.x;
    jitterMat[2][1] += jitter.y;

    gl_Position = projection * view * worldPos;
    //gl_Position = jitterMat * view * worldPos;

    preScreenPosition = pre_viewproj * worldPos;
    nowScreenPosition = projection * view * worldPos;

    FragPos = worldPos.xyz;
    Normal = aNormal;


}