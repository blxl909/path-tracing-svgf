#version 330 core

in vec3 pix;//[-1,1]

out vec4 fragColor;

uniform sampler2D lastColor;//cpu端还未设置
uniform sampler2D curColor;//cpu端还未设置
uniform sampler2D curWorldPos;
uniform sampler2D curDepth;
uniform mat4 pre_viewproj;
uniform mat4 cur_view;
uniform mat4 cur_proj;
uniform uint frameCounter;
uniform int screen_width;//resolution width
uniform int screen_height;//resolution wheight

vec2 Halton_2_3[8] =vec2[]
(
    vec2(0.0f, -1.0f / 3.0f),
    vec2(-1.0f / 2.0f, 1.0f / 3.0f),
    vec2(1.0f / 2.0f, -7.0f / 9.0f),
    vec2(-3.0f / 4.0f, -1.0f / 9.0f),
    vec2(1.0f / 4.0f, 5.0f / 9.0f),
    vec2(-1.0f / 4.0f, -5.0f / 9.0f),
    vec2(3.0f / 4.0f, 1.0f / 9.0f),
    vec2(-7.0f / 8.0f, 7.0f / 9.0f)
);

void main(){

    float inv_screen_width = 1.0f/screen_width;
    float inv_screen_height = 1.0f/screen_height;

    vec2 uv=pix.xy*0.5+0.5;
    vec4 cur_world_pos=texture2D(curWorldPos,uv).rgba;

    mat4 jittered_proj=cur_proj;
    uint index=frameCounter%8u;
    vec2 jitter=vec2(Halton_2_3[index].x*inv_screen_width,
                     Halton_2_3[index].y*inv_screen_height);

    jittered_proj[2][0]+=jitter.x;
    jittered_proj[2][1]+=jitter.y;

    vec4 jittered_cur_clip_space=jittered_proj*cur_view*cur_world_pos;
    vec2 jitter_cur_uv=vec2(jittered_cur_clip_space.xy)*0.5+0.5;
    vec4 pre_clip_pos=pre_viewproj*cur_world_pos;
    vec2 pre_uv=vec2(pre_clip_pos.xy)*0.5+0.5;

    vec2 velocity=pre_uv-jitter_cur_uv;
    
    vec3 nowColor=texture2D(curColor,uv).rgb;
    if(frameCounter==0u){
        fragColor=vec4(nowColor,1.0);//check later
        return;
    }

}