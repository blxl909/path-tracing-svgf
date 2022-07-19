#version 450 core

in vec3 pix;

layout (location = 0) out vec4 OutIllumination;//reprojected illumination xyz, variance w
layout (location = 1) out vec4 OutMoments_HistoryLength;//first moment x,second moment y,history length z

uniform sampler2D gMotion;
uniform sampler2D gColor;
uniform sampler2D gAlbedo;
uniform sampler2D gEmission;
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

	//vec3 illumination = texture2D(gColor,uv).xyz;
	vec3 illumination = demodulate(texture2D(gColor,uv).xyz - texture2D(gEmission,uv).xyz,texture2D(gAlbedo,uv).xyz); 

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

