#version 450 core

in vec3 pix;//[-1,1]



#define INF             114514.0

layout (location = 0) out vec4 fragColor;//reprojected color
layout (location = 1) out vec4 moment_History;//x 方差 、 y history、 z 一阶矩 w 二阶矩


uniform sampler2D texPass0;//color
uniform sampler2D texPass1;//normal and depth
uniform sampler2D texPass2;//world pos


//以下cpu端均未设置
uniform sampler2D lastNormalDepth;//cpu端还未设置
uniform sampler2D lastColor;//cpu端还未设置
uniform sampler2D lastMomentHistory;//x 方差 、 y history、 z 一阶矩 w 二阶矩
//uniform sampler2D velocity;

uniform int screen_width;//resolution width
uniform int screen_height;//resolution wheight
uniform mat4 pre_viewproj;
//uniform mat4 viewproj;



bool is_tap_consistent(int x, int y,vec3 curnormal,float curdepth){
    if (x < 0 || x > screen_width)  return false;
 	if (y < 0 || y > screen_height) return false;
	vec2 tap_uv=vec2(x/float(screen_width),y/float(screen_height));
    vec4 prev_normal_and_depth=texture2D(lastNormalDepth,tap_uv).rgba;
    vec3 prev_normal=prev_normal_and_depth.xyz;
    float prev_depth=prev_normal_and_depth.w;

    const float THRESHOLD_NORMAL = 0.8f;//0.95
	const float THRESHOLD_DEPTH  = 0.2f;//2

    bool consistent_normal = dot(curnormal, prev_normal)  > THRESHOLD_NORMAL;
	bool consistent_depth  = abs(curdepth - prev_depth) < THRESHOLD_DEPTH;

    return consistent_normal && consistent_depth;
}



float luminance(float r,float g,float b){
    return 0.2125*r+0.7154*g+0.0721*b;
}




void main(){
    
    //写的时候注意pix是[-1,1] 纹理采样时需要转换范围
    vec2 uv=pix.xy*0.5+0.5;
    vec4 direct=texture2D(texPass0,uv).rgba;
    //存储一阶矩和二阶矩
    vec2 moment;
    moment.x=luminance(direct.x,direct.y,direct.z);
    moment.y=moment.x*moment.x;

    vec4 normal_and_depth=texture2D(texPass1,uv).rgba;
    float depth=normal_and_depth.a;
    vec3 normal=normal_and_depth.rgb;


	vec4 worldPos=texture2D(texPass2,uv).rgba;
    vec4 pre_worldPos=pre_viewproj*worldPos;
//---------------
	//vec2 velocity = texture(velocity, getClosestOffset()).rg;
    //vec2 offsetUV = pix.xy*0.5+0.5 - velocity;
	//if(offsetUV.x<0||offsetUV.x>1||offsetUV.y<0||offsetUV.y>1){
		//gl_FragData[0]=direct;
		//vec4 history = texture2D(lastMomentHistory,uv).rgba;
		//float variance_direct   = max(0.0f, moment.y - moment.x * moment.x);
        //gl_FragData[1]=vec4(variance_direct,1,history.zw);
		//return;
	//}
    //vec3 preColor = texture(lastColor, offsetUV).rgb;
	//pre_worldPos=
//---------------
	if(pre_worldPos.w==0.0f){
		fragColor=direct;
		vec4 history = texture2D(lastMomentHistory,uv).rgba;
		float variance_direct   = max(0.0f, moment.y - moment.x * moment.x);
        moment_History=vec4(variance_direct,1,history.zw);
		return;
	}

    pre_worldPos/=pre_worldPos.w;
    vec2 pre_uv=pre_worldPos.xy*0.5+0.5;

    if( depth==0.0f||pre_uv.x<0||pre_uv.x>1.0||pre_uv.y<0||pre_uv.y>1.0){
        fragColor=direct;

        vec4 history = texture2D(lastMomentHistory,uv).rgba;
		float variance_direct   = max(0.0f, moment.y - moment.x * moment.x);
        moment_History=vec4(variance_direct,1,history.zw);
        return;
    }
    
    
	
	vec4 prev_normal_and_depth=texture2D(lastNormalDepth,pre_uv).rgba;
	
	float prev_depth=prev_normal_and_depth.a;
	vec3 prev_normal=prev_normal_and_depth.rgb;
    

    float s_prev=pre_uv.x*float(screen_width);
    float t_prev = pre_uv.y * float(screen_height);

    int x_prev=int(s_prev);//beter is not subtract 0.5,actually it seems more reasonal theoreticallly
    int y_prev=int(t_prev);


    float fractional_s = s_prev - floor(s_prev);
    float fractional_t = t_prev - floor(t_prev);

	float one_minus_fractional_s = 1.0f - fractional_s;
	float one_minus_fractional_t = 1.0f - fractional_t;


	float w0 = one_minus_fractional_s * one_minus_fractional_t;
	float w1 =           fractional_s * one_minus_fractional_t;
	float w2 = one_minus_fractional_s *           fractional_t;
	float w3 = 1.0f - w0 - w1 - w2;

	float weights[4] = float[]( w0, w1, w2, w3 );
	float consistent_weights_sum = 0.0f;


    for (int j = 0; j < 2; j++) {
		for (int i = 0; i < 2; i++) {
			int tap = i + j * 2;		
			if (is_tap_consistent(x_prev + i, y_prev + j, normal, depth)) {

				consistent_weights_sum += weights[tap];
			} else {
				weights[tap] = 0.0f;
			}
		}
	}

    vec3 prev_direct   = vec3(0.0f);
	vec2 prev_moment   = vec2(0.0f);

    // If we already found at least 1 consistent tap
	if (consistent_weights_sum >0.01f) {
		// Add consistent taps using their bilinear weight
		for (int j = 0; j < 2; j++) {
			for (int i = 0; i < 2; i++) {
				int tap = i + j * 2;

				if (weights[tap] > 0.01f) {
					int tap_x = x_prev + i;
					int tap_y = y_prev + j;
                    vec2 tap_uv=vec2(tap_x/float(screen_width),tap_y/float(screen_height));
					vec3 tap_direct   = texture2D(lastColor,tap_uv).rgb;
					vec2 tap_moment   = texture2D(lastMomentHistory,tap_uv).zw;

					prev_direct   += weights[tap] * tap_direct;
					prev_moment   += weights[tap] * tap_moment;
				}
			}
		}
	} else {
		// If we haven't yet found a consistent tap in a 2x2 region, try a 3x3 region
		for (int j = -1; j <= 1; j++) {
			for (int i = -1; i <= 1; i++) {

                int tap_x = x_prev + i;
				int tap_y = y_prev + j;
                vec2 tap_uv=vec2(tap_x/float(screen_width),tap_y/float(screen_height));

				if (is_tap_consistent(tap_x,tap_y, normal, depth)) {

					prev_direct   += texture2D(lastColor,tap_uv).rgb;
					prev_moment   += texture2D(lastMomentHistory,tap_uv).zw;

					consistent_weights_sum += 1.0f;
				}
			}
		}
	}
	
    if (consistent_weights_sum > 0.01f) {
		// Normalize
		prev_direct   /= consistent_weights_sum;
		prev_moment   /= consistent_weights_sum;

		float history = texture2D(lastMomentHistory,uv).g+1.0f; // Increase History Length by 1 step
		history=clamp(history,1.0f,64.0f);
		float inv_history = 1.0f / history;
        //暂时固定
		float alpha_colour = max(0.2f, inv_history);
		float alpha_moment = max(0.2f, inv_history);

		// Integrate using exponential moving average

		direct.rgb   = (1.0-alpha_colour)*prev_direct+alpha_colour*direct.rgb;
		moment   = (1.0-alpha_moment)*prev_moment+alpha_moment*moment;

		float variance_direct   = max(0.0f, moment.y - moment.x * moment.x);
			
        moment_History=vec4(variance_direct,history,moment);
			

	} else {
        moment_History=vec4(0.0,1.0,moment);
	}
    fragColor=vec4(direct);

    


}

