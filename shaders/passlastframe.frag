#version 450 core

in vec3 pix;
//输出到三个渲染目标
layout (location = 0) out vec4 lastColor;//上一帧颜色（即 fragcolor）
layout (location = 1) out vec4 lastNormalDepth;//上一帧
layout (location = 2) out vec4 lastMomentHistory;//上一帧（即 moment_History）
layout (location = 3) out vec4 lastTAA;//上一帧（即 moment_History）
layout (location = 4) out vec4 lastAccColor;//(展示累积的颜色)

//分别对上几个个pass的输出进行采样
uniform sampler2D texPass0;//上一个pass输出的颜色（来自reprojectionpass  pass2）
uniform sampler2D texPass1;//fshaderpass 输出的normaldepth
uniform sampler2D texPass2;//reprojectionpass  pass2 输出的MomenandtHistory
uniform sampler2D curTAAoutput;
uniform sampler2D curAccColor;

void  main(){
    vec4 next_color=texture2D(texPass0,pix.xy*0.5+0.5).rgba;
    vec4 next_normaldepth=texture2D(texPass1,pix.xy*0.5+0.5).rgba;
    vec4 next_momenthistory=texture2D(texPass2,pix.xy*0.5+0.5).rgba;
    vec4 next_taa_input=texture2D(curTAAoutput,pix.xy*0.5+0.5).rgba;
    vec4 next_acc_input=texture2D(curAccColor,pix.xy*0.5+0.5).rgba;

    lastColor=next_color;
    lastNormalDepth=next_normaldepth;
    lastMomentHistory=next_momenthistory;
    lastTAA=next_taa_input;
    lastAccColor=next_acc_input;
}
