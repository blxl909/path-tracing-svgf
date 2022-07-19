#version 450 core

in vec3 pix;

layout (location = 0) out vec4 OutIllumination;//reprojected illumination xyz, variance w
layout (location = 1) out vec4 OutMoments_HistoryLength;//first moment x,second moment y,history length z

uniform sampler2D gMotion;
uniform sampler2D gColor;
uniform sampler2D gAlbedo;
//uniform sampler2D gEmission;//not used currently
uniform sampler2D gPrevIllum;
uniform sampler2D gPrevMoments_HistoryLength;//first moment x,second moment y,history length z
uniform sampler2D gNormalAndLinearZ;//normal xyz, linearz w
uniform sampler2D gPrevNormalAndLinearZ;
uniform sampler2D gNormalDepthFwidth;//Normal fwidth x,depth fwidth y,linearz z

uniform float inv_screen_width;
uniform float inv_screen_height;



vec3 demodulate(vec3 c, vec3 albedo)
{
    return c / max(albedo, vec3(0.001, 0.001, 0.001));
}

bool isReprjValid(vec2 coord, float Z, float Zprev, float fwidthZ, vec3 normal, vec3 normalPrev, float fwidthNormal)
{
    // check whether reprojected pixel is inside of the screen
	if(coord.x<0.0||coord.x>1.0||coord.y<0.0||coord.y>1.0) return false;

    // check if deviation of depths is acceptable
    if (abs(Zprev - Z) / (fwidthZ + 1e-2f) > 10.f) return false;

    // check normals for compatibility
    if (distance(normal, normalPrev) / (fwidthNormal + 1e-2) > 16.0) return false;

    return true;
}

bool loadPrevData(vec2 ipos, out vec4 prevIllum, out vec2 prevMoments, out float historyLength)
{
	vec2 motion = texture2D(gMotion,ipos).xy;
	vec2 Fwidth = texture2D(gNormalDepthFwidth,ipos).xy;
	float normal_fwidth = Fwidth.x;
	float depth_fwidth = Fwidth.y;

	vec2 iposPrev = ipos - motion;

	vec4 cur_normal_depth = texture2D(gNormalAndLinearZ,ipos).xyzw;
	vec3 cur_normal = cur_normal_depth.xyz;
	float cur_depth = cur_normal_depth.w;

    prevIllum   = vec4(0,0,0,0);
    prevMoments = vec2(0,0);

    bool v[4];
	
    vec2 offset[4] = vec2 [] ( vec2(0, 0), vec2(inv_screen_width, 0), vec2(0, inv_screen_height), vec2(inv_screen_width, inv_screen_height) );

    // check for all 4 taps of the bilinear filter for validity
    bool valid = false;
    for (int sampleIdx = 0; sampleIdx < 4; sampleIdx++)
    {
        vec2 loc = iposPrev + offset[sampleIdx];

		vec4 prev_normal_depth = texture2D(gPrevNormalAndLinearZ,loc).xyzw;
		float prev_depth = prev_normal_depth.w;
		vec3 prev_normal = prev_normal_depth.xyz;

		v[sampleIdx] = isReprjValid(loc, cur_depth, prev_depth, depth_fwidth, cur_normal, prev_normal, normal_fwidth);

		valid = valid || v[sampleIdx];
    }

    if (valid)
    {
        float sumw = 0;

        float x = iposPrev.x - int(iposPrev.x / inv_screen_width) * inv_screen_width;
        float y = iposPrev.y - int(iposPrev.y / inv_screen_height) * inv_screen_height;

        // bilinear weights
        float w[4] = float[]( (1 - x) * (1 - y),
                                   x  * (1 - y),
                              (1 - x) *      y,
                                   x  *      y );

        // perform the actual bilinear interpolation
        for (int sampleIdx = 0; sampleIdx < 4; sampleIdx++)
        {
            vec2 loc = iposPrev + offset[sampleIdx];
            if (v[sampleIdx])
            {
                prevIllum   += w[sampleIdx] * texture2D(gPrevIllum,loc).xyzw;
                prevMoments += w[sampleIdx] * texture2D(gPrevMoments_HistoryLength,loc).xy;
                sumw        += w[sampleIdx];
             }
        }

        // redistribute weights in case not all taps were used
        valid = (sumw >= 0.01);
        prevIllum   = valid ? prevIllum / sumw   : vec4(0, 0, 0, 0);
        prevMoments = valid ? prevMoments / sumw : vec2(0, 0);
    }

    if (!valid) // perform cross-bilateral filter in the hope to find some suitable samples somewhere
    {
        float nValid = 0.0;

        // this code performs a binary descision for each tap of the cross-bilateral filter
        const int radius = 1;
        for (int yy = -radius; yy <= radius; yy++)
        {
            for (int xx = -radius; xx <= radius; xx++)
            {
                vec2 loc = iposPrev + vec2(xx * inv_screen_width, yy * inv_screen_height);

				vec4 prev_normal_depth = texture2D(gPrevNormalAndLinearZ,loc).xyzw;
				float prev_depth = prev_normal_depth.w;
				vec3 prev_normal = prev_normal_depth.xyz;

                if (isReprjValid(loc, cur_depth, prev_depth, depth_fwidth, cur_normal, prev_normal, normal_fwidth))
                {
                    prevIllum += texture2D(gPrevIllum,loc).xyzw;
                    prevMoments += texture2D(gPrevMoments_HistoryLength,loc).xy;
                    nValid += 1.0;
                }
            }
        }
        if (nValid > 0)
        {
            valid = true;
            prevIllum   /= nValid;
            prevMoments /= nValid;
        }
    }

    if (valid)
    {
        // crude, fixme
        historyLength = texture2D(gPrevMoments_HistoryLength,iposPrev).z;
    }
    else
    {
        prevIllum   = vec4(0,0,0,0);
        prevMoments = vec2(0,0);
        historyLength = 0;
    }

    return valid;
}

float luminance(vec3 color){
    return 0.2125*color.r+0.7154*color.g+0.0721*color.b;
}

void main(){

	vec2 uv = pix.xy * 0.5 + 0.5;

	float cur_depth = texture2D(gNormalAndLinearZ,uv).w;
	if(cur_depth == 1.0f){
		OutIllumination = texture2D(gColor,uv).xyzw;
		OutMoments_HistoryLength = texture2D(gPrevMoments_HistoryLength,uv).xyzw;
		return;
	}

	vec3 illumination = texture2D(gColor,uv).xyz;
	//vec3 illumination = demodulate(texture2D(gColor,uv).xyz,texture2D(gAlbedo,uv).xyz); 


	if(isnan(illumination.x)||isnan(illumination.y)||isnan(illumination.z)){
		illumination = vec3(0);
	}

	float historyLength;
    vec4 prevIllumination;
    vec2 prevMoments;

	bool success = loadPrevData(uv, prevIllumination, prevMoments, historyLength);
	historyLength = min(32.0f, success ? historyLength + 1.0f : 1.0f);

	float alpha        = success ? max(0.2, 1.0 / historyLength) : 1.0;
    float alphaMoments = success ? max(0.2, 1.0 / historyLength) : 1.0;

	vec2 moments;
    moments.r = luminance(illumination);
    moments.g = moments.r * moments.r;

	moments = (1.0 - alphaMoments) * prevMoments + alphaMoments * moments;

	float variance = max(0.f, moments.g - moments.r * moments.r);

	OutIllumination.xyz = (1.0-alpha)*prevIllumination.xyz + alpha * illumination;
	OutIllumination.w = variance;

	OutMoments_HistoryLength.xy = moments;
	OutMoments_HistoryLength.z = historyLength;

}

// #version 450 core

// in vec3 pix;//[-1,1]



// #define INF             114514.0

// layout (location = 0) out vec4 fragColor;//reprojected color
// layout (location = 1) out vec4 moment_History;//x 方差 、 y history、 z 一阶矩 w 二阶矩


// uniform sampler2D texPass0;//color
// uniform sampler2D texPass1;//normal and depth
// uniform sampler2D texPass2;//world pos


// //以下cpu端均未设置
// uniform sampler2D lastNormalDepth;//cpu端还未设置
// uniform sampler2D lastColor;//cpu端还未设置
// uniform sampler2D lastMomentHistory;//x 方差 、 y history、 z 一阶矩 w 二阶矩
// //uniform sampler2D velocity;

// uniform int screen_width;//resolution width
// uniform int screen_height;//resolution wheight
// uniform mat4 pre_viewproj;
// uniform float normal_threshold;
// uniform float depth_threshold;

// //uniform mat4 viewproj;



// bool is_tap_consistent(int x, int y,vec3 curnormal,float curdepth){
//     if (x < 0 || x > screen_width)  return false;
//  	if (y < 0 || y > screen_height) return false;
// 	vec2 tap_uv=vec2(x/float(screen_width),y/float(screen_height));
//     vec4 prev_normal_and_depth=texture2D(lastNormalDepth,tap_uv).rgba;
//     vec3 prev_normal=prev_normal_and_depth.xyz;
//     float prev_depth=prev_normal_and_depth.w;


//     bool consistent_normal = dot(curnormal, prev_normal)  > normal_threshold;
// 	bool consistent_depth  = abs(curdepth - prev_depth) < depth_threshold;

//     return consistent_normal && consistent_depth;
// }



// float luminance(float r,float g,float b){
//     return 0.2125*r+0.7154*g+0.0721*b;
// }




// void main(){
    
//     //写的时候注意pix是[-1,1] 纹理采样时需要转换范围
//     vec2 uv=pix.xy*0.5+0.5;
//     vec4 direct=texture2D(texPass0,uv).rgba;
//     //存储一阶矩和二阶矩
//     vec2 moment;
//     moment.x=luminance(direct.x,direct.y,direct.z);
//     moment.y=moment.x*moment.x;

//     vec4 normal_and_depth=texture2D(texPass1,uv).rgba;
//     float depth=normal_and_depth.a;
//     vec3 normal=normal_and_depth.rgb;


// 	vec4 worldPos=texture2D(texPass2,uv).rgba;
//     vec4 pre_worldPos=pre_viewproj*worldPos;

// 	if(pre_worldPos.w==0.0f){
// 		fragColor=direct;
// 		vec4 history = texture2D(lastMomentHistory,uv).rgba;
// 		float variance_direct   = max(0.0f, moment.y - moment.x * moment.x);
//         moment_History=vec4(variance_direct,1,history.zw);
// 		return;
// 	}

//     pre_worldPos/=pre_worldPos.w;
//     vec2 pre_uv=pre_worldPos.xy*0.5+0.5;

//     if( depth==0.0f||pre_uv.x<0||pre_uv.x>1.0||pre_uv.y<0||pre_uv.y>1.0){
//         fragColor=direct;

//         vec4 history = texture2D(lastMomentHistory,uv).rgba;
// 		float variance_direct   = max(0.0f, moment.y - moment.x * moment.x);
//         moment_History=vec4(variance_direct,1,history.zw);
//         return;
//     }
    
    
	
// 	vec4 prev_normal_and_depth=texture2D(lastNormalDepth,pre_uv).rgba;
	
// 	float prev_depth=prev_normal_and_depth.a;
// 	vec3 prev_normal=prev_normal_and_depth.rgb;
    

//     float s_prev=pre_uv.x*float(screen_width);
//     float t_prev = pre_uv.y * float(screen_height);

//     int x_prev=int(s_prev);//beter is not subtract 0.5,actually it seems more reasonal theoreticallly
//     int y_prev=int(t_prev);


//     float fractional_s = s_prev - floor(s_prev);
//     float fractional_t = t_prev - floor(t_prev);

// 	float one_minus_fractional_s = 1.0f - fractional_s;
// 	float one_minus_fractional_t = 1.0f - fractional_t;


// 	float w0 = one_minus_fractional_s * one_minus_fractional_t;
// 	float w1 =           fractional_s * one_minus_fractional_t;
// 	float w2 = one_minus_fractional_s *           fractional_t;
// 	float w3 = 1.0f - w0 - w1 - w2;

// 	float weights[4] = float[]( w0, w1, w2, w3 );
// 	float consistent_weights_sum = 0.0f;


//     for (int j = 0; j < 2; j++) {
// 		for (int i = 0; i < 2; i++) {
// 			int tap = i + j * 2;		
// 			if (is_tap_consistent(x_prev + i, y_prev + j, normal, depth)) {

// 				consistent_weights_sum += weights[tap];
// 			} else {
// 				weights[tap] = 0.0f;
// 			}
// 		}
// 	}

//     vec3 prev_direct   = vec3(0.0f);
// 	vec2 prev_moment   = vec2(0.0f);

//     // If we already found at least 1 consistent tap
// 	if (consistent_weights_sum >0.01f) {
// 		// Add consistent taps using their bilinear weight
// 		for (int j = 0; j < 2; j++) {
// 			for (int i = 0; i < 2; i++) {
// 				int tap = i + j * 2;

// 				if (weights[tap] > 0.01f) {
// 					int tap_x = x_prev + i;
// 					int tap_y = y_prev + j;
//                     vec2 tap_uv=vec2(tap_x/float(screen_width),tap_y/float(screen_height));
// 					vec3 tap_direct   = texture2D(lastColor,tap_uv).rgb;
// 					vec2 tap_moment   = texture2D(lastMomentHistory,tap_uv).zw;

// 					prev_direct   += weights[tap] * tap_direct;
// 					prev_moment   += weights[tap] * tap_moment;
// 				}
// 			}
// 		}
// 	} else {
// 		// If we haven't yet found a consistent tap in a 2x2 region, try a 3x3 region
// 		for (int j = -1; j <= 1; j++) {
// 			for (int i = -1; i <= 1; i++) {

//                 int tap_x = x_prev + i;
// 				int tap_y = y_prev + j;
//                 vec2 tap_uv=vec2(tap_x/float(screen_width),tap_y/float(screen_height));

// 				if (is_tap_consistent(tap_x,tap_y, normal, depth)) {

// 					prev_direct   += texture2D(lastColor,tap_uv).rgb;
// 					prev_moment   += texture2D(lastMomentHistory,tap_uv).zw;

// 					consistent_weights_sum += 1.0f;
// 				}
// 			}
// 		}
// 	}
	
//     if (consistent_weights_sum > 0.01f) {
// 		// Normalize
// 		prev_direct   /= consistent_weights_sum;
// 		prev_moment   /= consistent_weights_sum;

// 		float history = texture2D(lastMomentHistory,uv).g+1.0f; // Increase History Length by 1 step
// 		history=clamp(history,1.0f,64.0f);
// 		float inv_history = 1.0f / history;
//         //暂时固定
// 		float alpha_colour = max(0.2f, inv_history);
// 		float alpha_moment = max(0.2f, inv_history);

// 		// Integrate using exponential moving average

// 		direct.rgb   = (1.0-alpha_colour)*prev_direct+alpha_colour*direct.rgb;
// 		moment   = (1.0-alpha_moment)*prev_moment+alpha_moment*moment;

// 		float variance_direct   = max(0.0f, moment.y - moment.x * moment.x);
			
//         moment_History=vec4(variance_direct,history,moment);
			

// 	} else {
//         moment_History=vec4(0.0,1.0,moment);
// 	}
//     fragColor=vec4(direct);

    


// }

