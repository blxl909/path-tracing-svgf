#version 330 core

in vec3 pix;
out vec4 fragColor;

uniform sampler2D texPass0;
uniform sampler2D texPass1;
uniform sampler2D texPass2;


vec3 tonrMapping(in vec3 c, float limit){
    float luminance=0.3*c.x+0.6*c.y+0.1*c.z;
    return c*1.0/(1.0+luminance/limit);
}

float near = 0.1; 
float far  = 1000.0; 

float LinearizeDepth(float depth) 
{
    float z = depth * 2.0 - 1.0; // back to NDC 
    return (2.0 * near * far) / (far + near - z * (far - near));    
}

void  main(){
    vec3 color=texture2D(texPass0,pix.xy*0.5+0.5).rgb;
    //color=tonrMapping(color,1.5);
    //color=pow(color,vec3(1.0/2.2));
    //vec4 tmpcolor=texture2D(texPass1,pix.xy*0.5+0.5).rgba;

    //color.a=LinearizeDepth(color.a);
    fragColor=vec4(color.rgb,1.0);


    

}

