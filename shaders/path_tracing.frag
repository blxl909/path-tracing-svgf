#version 450 core
#extension GL_EXT_gpu_shader4: enable

in vec3 pix;
//out vec4 fragColor;
layout (location = 0) out vec4 fragColor;
layout (location = 1) out vec4 Emission;
layout (location = 2) out vec4 Albedo;

// ----------------------------------------------------------------------------- //

uniform uint frameCounter;
uniform int nTriangles;
uniform int nNodes;
uniform int width;
uniform int height;
uniform int hdrResolution;

uniform samplerBuffer triangles;
uniform samplerBuffer nodes;
uniform samplerBuffer pointLights;//to be set in host side

uniform sampler2DArray material_array;

uniform sampler2D lastFrame;
uniform sampler2D hdrMap;
uniform sampler2D hdrCache;

uniform vec3 eye;
uniform mat4 cameraRotate;
uniform bool use_normal_map;
uniform bool accumulate;
uniform int pointLightSize;
uniform float clamp_threshold;
uniform int max_tracing_depth;
 

const vec2 Halton_2_3[8] = vec2[](
    vec2(1.0f / 16.0f, 8.0f / 9.0f),
    vec2(7.0f / 8.0f, 5.0f / 9.0f),
    vec2(3.0f / 8.0f, 2.0f / 9.0f),
    vec2(5.0f / 8.0f, 7.0f / 9.0f),
    vec2(1.0f / 8.0f, 4.0f / 9.0f),
    vec2(3.0f / 4.0f, 1.0f / 9.0f),
    vec2(1.0f / 4.0f, 2.0f / 3.0f),
    vec2(1.0f/2.0f, 1.0f / 3.0f)
    );
    

// ----------------------------------------------------------------------------- //

#define PI              3.1415926
#define INF             114514.0
#define epsilon         1e-6f
#define SIZE_TRIANGLE   15  //12 before modify
#define SIZE_BVHNODE    4
#define SIZE_POINTLIGHT 2   

// ----------------------------------------------------------------------------- //

struct PointLight{
    vec3 position;
    vec3 radiance;
};

// Triangle 数据格式
struct Triangle {
    vec3 p1, p2, p3;    // 顶点坐标
    vec3 n1, n2, n3;    // 顶点法线
    vec2 uv1, uv2, uv3;
    int objIndex;
};

// BVH 树节点
struct BVHNode {
    int left;           // 左子树
    int right;          // 右子树
    int n;              // 包含三角形数目
    int index;          // 三角形索引
    vec3 AA, BB;        // 碰撞盒
};

// 物体表面材质定义
struct Material {
    vec3 emissive;          // 作为光源时的发光颜色
    vec3 baseColor;
    float subsurface;
    float metallic;
    float specular;
    float specularTint;
    float roughness;
    float anisotropic;
    float sheen;
    float sheenTint;
    float clearcoat;
    float clearcoatGloss;
    float IOR;
    float transmission;
};

// 光线
struct Ray {
    vec3 startPoint;
    vec3 direction;
};

// 光线求交结果
struct HitResult {
    bool isHit;             // 是否命中
    bool isInside;          // 是否从内部命中
    float distance;         // 与交点的距离
    vec3 hitPoint;          // 光线命中点
    vec3 normal;            // 命中点法线
    vec3 viewDir;           // 击中该点的光线的方向
    Material material;      // 命中点的表面材质
    int objIndex;
};

// 重要性采样的返回结果
struct SampleResult {
    vec3 direction;
    float pdf;
};

// ----------------------------------------------------------------------------- //

PointLight getPointLight(int i){
    int offset = i * SIZE_POINTLIGHT;
    PointLight light;

    light.position = texelFetch(pointLights, offset + 0).xyz;
    light.radiance = texelFetch(pointLights, offset + 1).xyz;

    return light;
}


// 获取第 i 下标的三角形
Triangle getTriangle(int i) {
    int offset = i * SIZE_TRIANGLE;
    Triangle t;

    // 顶点坐标
    t.p1 = texelFetch(triangles, offset + 0).xyz;
    t.p2 = texelFetch(triangles, offset + 1).xyz;
    t.p3 = texelFetch(triangles, offset + 2).xyz;
    // 法线
    t.n1 = texelFetch(triangles, offset + 3).xyz;
    t.n2 = texelFetch(triangles, offset + 4).xyz;
    t.n3 = texelFetch(triangles, offset + 5).xyz;

    vec3 uvPacked1 = texelFetch(triangles, offset + 12).xyz;
    vec3 uvPacked2 = texelFetch(triangles, offset + 13).xyz;
    //uv
    t.uv1 = uvPacked1.xy;
    t.uv2 = vec2(uvPacked1.z,uvPacked2.x);
    t.uv3 = uvPacked2.yz;
    //obj Index
    t.objIndex = int(texelFetch(triangles, offset + 14).x);

    return t;
}

// 获取第 i 下标的三角形的材质
Material getMaterial(int i) {
    Material m;

    int offset = i * SIZE_TRIANGLE;
    vec3 param1 = texelFetch(triangles, offset + 8).xyz;
    vec3 param2 = texelFetch(triangles, offset + 9).xyz;
    vec3 param3 = texelFetch(triangles, offset + 10).xyz;
    vec3 param4 = texelFetch(triangles, offset + 11).xyz;
    
    m.emissive = texelFetch(triangles, offset + 6).xyz;
    m.baseColor = texelFetch(triangles, offset + 7).xyz;
    m.subsurface = param1.x;
    m.metallic = param1.y;
    m.specular = param1.z;
    m.specularTint = param2.x;
    m.roughness = param2.y;
    m.anisotropic = param2.z;
    m.sheen = param3.x;
    m.sheenTint = param3.y;
    m.clearcoat = param3.z;
    m.clearcoatGloss = param4.x;
    m.IOR = param4.y;
    m.transmission = param4.z;

    return m;
}

// 获取第 i 下标的 BVHNode 对象
BVHNode getBVHNode(int i) {
    BVHNode node;

    // 左右子树
    int offset = i * SIZE_BVHNODE;
    ivec3 childs = ivec3(texelFetch(nodes, offset + 0).xyz);
    ivec3 leafInfo = ivec3(texelFetch(nodes, offset + 1).xyz);
    node.left = int(childs.x);
    node.right = int(childs.y);
    node.n = int(leafInfo.x);
    node.index = int(leafInfo.y);

    // 包围盒
    node.AA = texelFetch(nodes, offset + 2).xyz;
    node.BB = texelFetch(nodes, offset + 3).xyz;

    return node;
}

// ----------------------------------------------------------------------------- //

// 光线和三角形求交 
HitResult hitTriangle(Triangle triangle, Ray ray) {
    HitResult res;
    res.distance = INF;
    res.isHit = false;
    res.isInside = false;

    vec3 p1 = triangle.p1;
    vec3 p2 = triangle.p2;
    vec3 p3 = triangle.p3;

    vec3 S = ray.startPoint;    // 射线起点
    vec3 d = ray.direction;     // 射线方向
    vec3 N = normalize(cross(p2-p1, p3-p1));    // 法向量

    // 从三角形背后（模型内部）击中
    if (dot(N, d) > 0.0f) {
        N = -N;   
        res.isInside = true;
    }

    // 如果视线和三角形平行
    if (abs(dot(N, d)) < 0.00001f) return res;

    // 距离
    float t = (dot(N, p1) - dot(S, N)) / dot(d, N);
    if (t < 0.0005f) return res;    // 如果三角形在光线背面

    // 交点计算
    vec3 P = S + d * t;

    // 判断交点是否在三角形中
    vec3 c1 = cross(p2 - p1, P - p1);
    vec3 c2 = cross(p3 - p2, P - p2);
    vec3 c3 = cross(p1 - p3, P - p3);
    bool r1 = (dot(c1, N) > 0 && dot(c2, N) > 0 && dot(c3, N) > 0);
    bool r2 = (dot(c1, N) < 0 && dot(c2, N) < 0 && dot(c3, N) < 0);

    // 命中，封装返回结果
    if (r1 || r2) {
        res.isHit = true;
        res.hitPoint = P;
        res.distance = t;
        res.normal = N;
        res.viewDir = d;
        
        // 根据交点位置插值顶点法线
        float alpha = (-(P.x-p2.x)*(p3.y-p2.y) + (P.y-p2.y)*(p3.x-p2.x)) / (-(p1.x-p2.x)*(p3.y-p2.y) + (p1.y-p2.y)*(p3.x-p2.x)+1e-7);
        float beta  = (-(P.x-p3.x)*(p1.y-p3.y) + (P.y-p3.y)*(p1.x-p3.x)) / (-(p2.x-p3.x)*(p1.y-p3.y) + (p2.y-p3.y)*(p1.x-p3.x)+1e-7);
        float gama  = 1.0 - alpha - beta;
        vec3 Nsmooth = alpha * triangle.n1 + beta * triangle.n2 + gama * triangle.n3;
        Nsmooth = normalize(Nsmooth);
        /*
        vec3 Nsmooth = N;*/
        res.normal = (res.isInside) ? (-Nsmooth) : (Nsmooth);
    }

    return res;
}

// 和 aabb 盒子求交，没有交点则返回 -1
float hitAABB(Ray r, vec3 AA, vec3 BB) {
    vec3 invdir = 1.0 / r.direction;

    vec3 f = (BB - r.startPoint) * invdir;
    vec3 n = (AA - r.startPoint) * invdir;

    vec3 tmax = max(f, n);
    vec3 tmin = min(f, n);

    float t1 = min(tmax.x, min(tmax.y, tmax.z));
    float t0 = max(tmin.x, max(tmin.y, tmin.z));

    return (t1 >= t0) ? ((t0 > 0.0) ? (t0) : (t1)) : (-1);
}

// ----------------------------------------------------------------------------- //

bool under_zero(vec3 color){
    return color.x<0.0||color.y<0.0||color.z<0.0;
}


// 暴力遍历数组下标范围 [l, r] 求最近交点
HitResult hitArray(Ray ray, int l, int r) {
    HitResult res;
    res.isHit = false;
    res.distance = INF;

    int nearest_tri_index=-1;

    for(int i=l; i<=r; i++) {
        Triangle triangle = getTriangle(i);
        HitResult r = hitTriangle(triangle, ray);
        if(r.isHit && r.distance<res.distance) {
            res = r;
            res.material = getMaterial(i);
            nearest_tri_index=i;
        }
    }
//---------------------------
    if(res.isHit){

        Triangle hit_tri=getTriangle(nearest_tri_index);

        vec3 p1 = hit_tri.p1;
        vec3 p2 = hit_tri.p2;
        vec3 p3 = hit_tri.p3;
        vec3 P = res.hitPoint;

        //不需要透视矫正插值 在world空间中仍然保持线性性质
        float alpha = (-(P.x-p2.x)*(p3.y-p2.y) + (P.y-p2.y)*(p3.x-p2.x)) / (-(p1.x-p2.x)*(p3.y-p2.y) + (p1.y-p2.y)*(p3.x-p2.x)+1e-7);
        float beta  = (-(P.x-p3.x)*(p1.y-p3.y) + (P.y-p3.y)*(p1.x-p3.x)) / (-(p2.x-p3.x)*(p1.y-p3.y) + (p2.y-p3.y)*(p1.x-p3.x)+1e-7);
        float gama  = 1.0 - alpha - beta;
        
        vec2 smooth_uv=alpha * hit_tri.uv1 + beta * hit_tri.uv2 + gama * hit_tri.uv3;

        int mat_id = hit_tri.objIndex * 4;//albedo  at the first place others +1 +2 to be implement
        if(under_zero(res.material.baseColor)){
            res.material.baseColor =  texture2DArray(material_array,vec3(smooth_uv,float(mat_id))).xyz;
        }
        if(res.material.metallic<0.0f){
            res.material.metallic = texture2DArray(material_array,vec3(smooth_uv,float(mat_id+1.0f))).x;//srgb or linear?
        }
        if(use_normal_map){
            //calculate TBN matrix
            vec3 edge1 = p2-p1;
            vec3 edge2 = p3-p1;
            vec2 deltaUV1 = hit_tri.uv2 - hit_tri.uv1;
            vec2 deltaUV2 = hit_tri.uv3 - hit_tri.uv1;

            float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
            vec3 tangent;
            tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
            tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
            tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
            tangent = normalize(tangent);

            vec3 bitangent = cross(tangent,res.normal);

            mat3 TBN = mat3(tangent,bitangent,res.normal);
            
            //sample texture normal
            vec3 tex_normal = texture2DArray(material_array,vec3(smooth_uv,float(mat_id+2.0f))).xyz;
            tex_normal = normalize(tex_normal * 2.0f - 1.0f);
            //get actual normal define in world space
            res.normal = normalize(TBN * tex_normal);
        }
        if(res.material.roughness<0.0f){
            res.material.roughness = texture2DArray(material_array,vec3(smooth_uv,float(mat_id+3.0f))).x;
        }

    }
//--------------------------------------
    return res;
}

// 遍历 BVH 求交
HitResult hitBVH(Ray ray) {
    HitResult res;
    res.isHit = false;
    res.distance = INF;

    // 栈
    int stack[256];
    int sp = 0;

    stack[sp++] = 1;
    while(sp>0) {
        int top = stack[--sp];
        BVHNode node = getBVHNode(top);
        
        // 是叶子节点，遍历三角形，求最近交点
        if(node.n>0) {
            int L = node.index;
            int R = node.index + node.n - 1;
            HitResult r = hitArray(ray, L, R);
            if(r.isHit && r.distance<res.distance) res = r;
            continue;
        }
        
        // 和左右盒子 AABB 求交
        float d1 = INF; // 左盒子距离
        float d2 = INF; // 右盒子距离
        if(node.left>0) {
            BVHNode leftNode = getBVHNode(node.left);
            d1 = hitAABB(ray, leftNode.AA, leftNode.BB);
        }
        if(node.right>0) {
            BVHNode rightNode = getBVHNode(node.right);
            d2 = hitAABB(ray, rightNode.AA, rightNode.BB);
        }

        // 在最近的盒子中搜索
        if(d1>0 && d2>0) {
            if(d1<d2) { // d1<d2, 左边先
                stack[sp++] = node.right;
                stack[sp++] = node.left;
            } else {    // d2<d1, 右边先
                stack[sp++] = node.left;
                stack[sp++] = node.right;
            }
        } else if(d1>0) {   // 仅命中左边
            stack[sp++] = node.left;
        } else if(d2>0) {   // 仅命中右边
            stack[sp++] = node.right;
        }
    }

    return res;
}

// ----------------------------------------------------------------------------- //

/*
 * 生成随机向量，依赖于 frameCounter 帧计数器
 * 代码来源：https://blog.demofox.org/2020/05/25/casual-shadertoy-path-tracing-1-basic-camera-diffuse-emissive/
*/

uint seed = uint(
    uint((pix.x * 0.5 + 0.5) * width)  * uint(1973) + 
    uint((pix.y * 0.5 + 0.5) * height) * uint(9277) + 
    uint(frameCounter) * uint(26699)) | uint(1);

uint wang_hash(inout uint seed) {
    seed = uint(seed ^ uint(61)) ^ uint(seed >> uint(16));
    seed *= uint(9);
    seed = seed ^ (seed >> 4);
    seed *= uint(0x27d4eb2d);
    seed = seed ^ (seed >> 15);
    return seed;
}
 
float rand() {
    return float(wang_hash(seed)) / 4294967296.0;
}

uint seed_sync = uint(
    uint((pix.x * 0.0 + 0.5) * width)  * uint(1973) + 
    uint((pix.y * 0.0 + 0.5) * height) * uint(9277) + 
    uint(114514) * uint(26699)) | uint(1);

float rand_sync() {
    return float(wang_hash(seed_sync)) / 4294967296.0;
}

// ----------------------------------------------------------------------------- //

// 1 ~ 8 维的 sobol 生成矩阵
const uint V[8*32] = const uint[](
    2147483648u, 1073741824u, 536870912u, 268435456u, 134217728u, 67108864u, 33554432u, 16777216u, 8388608u, 4194304u, 2097152u, 1048576u, 524288u, 262144u, 131072u, 65536u, 32768u, 16384u, 8192u, 4096u, 2048u, 1024u, 512u, 256u, 128u, 64u, 32u, 16u, 8u, 4u, 2u, 1u,
    2147483648u, 3221225472u, 2684354560u, 4026531840u, 2281701376u, 3422552064u, 2852126720u, 4278190080u, 2155872256u, 3233808384u, 2694840320u, 4042260480u, 2290614272u, 3435921408u, 2863267840u, 4294901760u, 2147516416u, 3221274624u, 2684395520u, 4026593280u, 2281736192u, 3422604288u, 2852170240u, 4278255360u, 2155905152u, 3233857728u, 2694881440u, 4042322160u, 2290649224u, 3435973836u, 2863311530u, 4294967295u,
    2147483648u, 3221225472u, 1610612736u, 2415919104u, 3892314112u, 1543503872u, 2382364672u, 3305111552u, 1753219072u, 2629828608u, 3999268864u, 1435500544u, 2154299392u, 3231449088u, 1626210304u, 2421489664u, 3900735488u, 1556135936u, 2388680704u, 3314585600u, 1751705600u, 2627492864u, 4008611328u, 1431684352u, 2147543168u, 3221249216u, 1610649184u, 2415969680u, 3892340840u, 1543543964u, 2382425838u, 3305133397u,
    2147483648u, 3221225472u, 536870912u, 1342177280u, 4160749568u, 1946157056u, 2717908992u, 2466250752u, 3632267264u, 624951296u, 1507852288u, 3872391168u, 2013790208u, 3020685312u, 2181169152u, 3271884800u, 546275328u, 1363623936u, 4226424832u, 1977167872u, 2693105664u, 2437829632u, 3689389568u, 635137280u, 1484783744u, 3846176960u, 2044723232u, 3067084880u, 2148008184u, 3222012020u, 537002146u, 1342505107u,
    2147483648u, 1073741824u, 536870912u, 2952790016u, 4160749568u, 3690987520u, 2046820352u, 2634022912u, 1518338048u, 801112064u, 2707423232u, 4038066176u, 3666345984u, 1875116032u, 2170683392u, 1085997056u, 579305472u, 3016343552u, 4217741312u, 3719483392u, 2013407232u, 2617981952u, 1510979072u, 755882752u, 2726789248u, 4090085440u, 3680870432u, 1840435376u, 2147625208u, 1074478300u, 537900666u, 2953698205u,
    2147483648u, 1073741824u, 1610612736u, 805306368u, 2818572288u, 335544320u, 2113929216u, 3472883712u, 2290089984u, 3829399552u, 3059744768u, 1127219200u, 3089629184u, 4199809024u, 3567124480u, 1891565568u, 394297344u, 3988799488u, 920674304u, 4193267712u, 2950604800u, 3977188352u, 3250028032u, 129093376u, 2231568512u, 2963678272u, 4281226848u, 432124720u, 803643432u, 1633613396u, 2672665246u, 3170194367u,
    2147483648u, 3221225472u, 2684354560u, 3489660928u, 1476395008u, 2483027968u, 1040187392u, 3808428032u, 3196059648u, 599785472u, 505413632u, 4077912064u, 1182269440u, 1736704000u, 2017853440u, 2221342720u, 3329785856u, 2810494976u, 3628507136u, 1416089600u, 2658719744u, 864310272u, 3863387648u, 3076993792u, 553150080u, 272922560u, 4167467040u, 1148698640u, 1719673080u, 2009075780u, 2149644390u, 3222291575u,
    2147483648u, 1073741824u, 2684354560u, 1342177280u, 2281701376u, 1946157056u, 436207616u, 2566914048u, 2625634304u, 3208642560u, 2720006144u, 2098200576u, 111673344u, 2354315264u, 3464626176u, 4027383808u, 2886631424u, 3770826752u, 1691164672u, 3357462528u, 1993345024u, 3752330240u, 873073152u, 2870150400u, 1700563072u, 87021376u, 1097028000u, 1222351248u, 1560027592u, 2977959924u, 23268898u, 437609937u
);

// 格林码 
uint grayCode(uint i) {
	return i ^ (i>>1);
}

// 生成第 d 维度的第 i 个 sobol 数
float sobol(uint d, uint i) {
    uint result = 0u;
    uint offset = d * 32u;
    for(uint j = 0u; i>0u; i >>= 1u, j++) 
        if((i & 1u)==1u)
            result ^= V[j+offset];

    return float(result) * (1.0f/float(0xFFFFFFFFU));
}

// 生成第 i 帧的第 b 次反弹需要的二维随机向量
vec2 sobolVec2(uint i, uint b) {
    float u = sobol(b*2u, grayCode(i));
    float v = sobol(b*2u+1u, grayCode(i));
    return vec2(u, v);
}

vec2 CranleyPattersonRotation(vec2 p) {
    uint pseed = uint(
        uint((pix.x * 0.5 + 0.5) * width)  * uint(1973) + 
        uint((pix.y * 0.5 + 0.5) * height) * uint(9277) + 
        uint(114514/1919) * uint(26699)) | uint(1);
    
    float u = float(wang_hash(pseed)) / 4294967296.0;
    float v = float(wang_hash(pseed)) / 4294967296.0;

    p.x += u;
    if(p.x>1) p.x -= 1;
    if(p.x<0) p.x += 1;

    p.y += v;
    if(p.y>1) p.y -= 1;
    if(p.y<0) p.y += 1;

    return p;
}

// ----------------------------------------------------------------------------- //

float sqr(float x) { 
    return x*x; 
}

float SchlickFresnel(float u) {
    float m = clamp(1-u, 0, 1);
    float m2 = m*m;
    return m2*m2*m; // pow(m,5)
}

float GTR1(float NdotH, float a) {
    if (a >= 1) return 1/PI;
    float a2 = a*a;
    float t = 1 + (a2-1)*NdotH*NdotH;
    return (a2-1) / (PI*log(a2)*t);
}

float GTR2(float NdotH, float a) {
    float a2 = a*a;
    float t = 1 + (a2-1)*NdotH*NdotH;
    return a2 / (PI * t*t);
}

float GTR2_aniso(float NdotH, float HdotX, float HdotY, float ax, float ay) {
    return 1 / (PI * ax*ay * sqr( sqr(HdotX/ax) + sqr(HdotY/ay) + NdotH*NdotH ));
}

float smithG_GGX(float NdotV, float alphaG) {
    float a = alphaG*alphaG;
    float b = NdotV*NdotV;
    return 1 / (NdotV + sqrt(a + b - a*b));
}

float smithG_GGX_aniso(float NdotV, float VdotX, float VdotY, float ax, float ay) {
    return 1 / (NdotV + sqrt( sqr(VdotX*ax) + sqr(VdotY*ay) + sqr(NdotV) ));
}

vec3 BRDF_Evaluate_aniso(vec3 V, vec3 N, vec3 L, vec3 X, vec3 Y, in Material material) {
    float NdotL = dot(N, L);
    float NdotV = dot(N, V);
    if(NdotL < 0 || NdotV < 0) return vec3(0);

    vec3 H = normalize(L + V);
    float NdotH = dot(N, H);
    float LdotH = dot(L, H);

    // 各种颜色
    vec3 Cdlin = material.baseColor;
    float Cdlum = 0.3 * Cdlin.r + 0.6 * Cdlin.g  + 0.1 * Cdlin.b;
    vec3 Ctint = (Cdlum > 0) ? (Cdlin/Cdlum) : (vec3(1));   
    vec3 Cspec = material.specular * mix(vec3(1), Ctint, material.specularTint);
    vec3 Cspec0 = mix(0.08*Cspec, Cdlin, material.metallic); // 0° 镜面反射颜色
    vec3 Csheen = mix(vec3(1), Ctint, material.sheenTint);   // 织物颜色

    // 漫反射
    float Fd90 = 0.5 + 2.0 * LdotH * LdotH * material.roughness;
    float FL = SchlickFresnel(NdotL);
    float FV = SchlickFresnel(NdotV);
    float Fd = mix(1.0, Fd90, FL) * mix(1.0, Fd90, FV);

    // 次表面散射
    float Fss90 = LdotH * LdotH * material.roughness;
    float Fss = mix(1.0, Fss90, FL) * mix(1.0, Fss90, FV);
    float ss = 1.25 * (Fss * (1.0 / (NdotL + NdotV) - 0.5) + 0.5);
     
    // 镜面反射 -- 各向同性
    float alpha = max(0.001, sqr(material.roughness));
    float Ds = GTR2(NdotH, alpha);
    float FH = SchlickFresnel(LdotH);
    vec3 Fs = mix(Cspec0, vec3(1), FH);
    float Gs = smithG_GGX(NdotL, material.roughness);
    Gs *= smithG_GGX(NdotV, material.roughness);
    /*
    // 镜面反射 -- 各向异性
    float aspect = sqrt(1.0 - material.anisotropic * 0.9);
    float ax = max(0.001, sqr(material.roughness)/aspect);
    float ay = max(0.001, sqr(material.roughness)*aspect);
    float Ds = GTR2_aniso(NdotH, dot(H, X), dot(H, Y), ax, ay);
    float FH = SchlickFresnel(LdotH);
    vec3 Fs = mix(Cspec0, vec3(1), FH);
    float Gs;
    Gs  = smithG_GGX_aniso(NdotL, dot(L, X), dot(L, Y), ax, ay);
    Gs *= smithG_GGX_aniso(NdotV, dot(V, X), dot(V, Y), ax, ay);
    */

    // 清漆
    float Dr = GTR1(NdotH, mix(0.1, 0.001, material.clearcoatGloss));
    float Fr = mix(0.04, 1.0, FH);
    float Gr = smithG_GGX(NdotL, 0.25) * smithG_GGX(NdotV, 0.25);

    // sheen
    vec3 Fsheen = FH * material.sheen * Csheen;
    
    vec3 diffuse = (1.0/PI) * mix(Fd, ss, material.subsurface) * Cdlin + Fsheen;
    vec3 specular = Gs * Fs * Ds;
    vec3 clearcoat = vec3(0.25 * Gr * Fr * Dr * material.clearcoat);

    return diffuse * (1.0 - material.metallic) + specular + clearcoat;
}

vec3 BRDF_Evaluate(vec3 V, vec3 N, vec3 L, in Material material) {
    float NdotL = dot(N, L);
    float NdotV = dot(N, V);
    if(NdotL < 0 || NdotV < 0) return vec3(0);

    vec3 H = normalize(L + V);
    float NdotH = dot(N, H);
    float LdotH = dot(L, H);

    // 各种颜色
    vec3 Cdlin = material.baseColor;
    float Cdlum = 0.3 * Cdlin.r + 0.6 * Cdlin.g  + 0.1 * Cdlin.b;
    vec3 Ctint = (Cdlum > 0) ? (Cdlin/Cdlum) : (vec3(1));   
    vec3 Cspec = material.specular * mix(vec3(1), Ctint, material.specularTint);
    vec3 Cspec0 = mix(0.08*Cspec, Cdlin, material.metallic); // 0° 镜面反射颜色
    vec3 Csheen = mix(vec3(1), Ctint, material.sheenTint);   // 织物颜色

    // 漫反射
    float Fd90 = 0.5 + 2.0 * LdotH * LdotH * material.roughness;
    float FL = SchlickFresnel(NdotL);
    float FV = SchlickFresnel(NdotV);
    float Fd = mix(1.0, Fd90, FL) * mix(1.0, Fd90, FV);

    // 次表面散射
    float Fss90 = LdotH * LdotH * material.roughness;
    float Fss = mix(1.0, Fss90, FL) * mix(1.0, Fss90, FV);
    float ss = 1.25 * (Fss * (1.0 / (NdotL + NdotV) - 0.5) + 0.5);
     
    // 镜面反射 -- 各向同性
    float alpha = max(0.001, sqr(material.roughness));
    float Ds = GTR2(NdotH, alpha);
    float FH = SchlickFresnel(LdotH);
    vec3 Fs = mix(Cspec0, vec3(1), FH);
    float Gs = smithG_GGX(NdotL, material.roughness);
    Gs *= smithG_GGX(NdotV, material.roughness);

    // 清漆
    float Dr = GTR1(NdotH, mix(0.1, 0.001, material.clearcoatGloss));
    float Fr = mix(0.04, 1.0, FH);
    float Gr = smithG_GGX(NdotL, 0.25) * smithG_GGX(NdotV, 0.25);

    // sheen
    vec3 Fsheen = FH * material.sheen * Csheen;
    
    vec3 diffuse = (1.0/PI) * mix(Fd, ss, material.subsurface) * Cdlin + Fsheen;
    vec3 specular = Gs * Fs * Ds;
    vec3 clearcoat = vec3(0.25 * Gr * Fr * Dr * material.clearcoat);

    return diffuse * (1.0 - material.metallic) + specular + clearcoat;
}

// ----------------------------------------------------------------------------- //

void getTangent(vec3 N, inout vec3 tangent, inout vec3 bitangent) {
    vec3 helper = vec3(1, 0, 0);
    if(abs(N.x)>0.999) helper = vec3(0, 0, 1);
    bitangent = normalize(cross(N, helper));
    tangent = normalize(cross(N, bitangent));
}

// 将向量 v 投影到 N 的法向半球
vec3 toNormalHemisphere(vec3 v, vec3 N) {
    vec3 helper = vec3(1, 0, 0);
    if(abs(N.x)>0.999) helper = vec3(0, 0, 1);
    vec3 tangent = normalize(cross(N, helper));
    vec3 bitangent = normalize(cross(N, tangent));
    return v.x * tangent + v.y * bitangent + v.z * N;
}

// 半球均匀采样
vec3 SampleHemisphere(float xi_1, float xi_2) {
    //xi_1 = rand(), xi_2 = rand();
    float z = xi_1;
    float r = max(0, sqrt(1.0 - z*z));
    float phi = 2.0 * PI * xi_2;
    return vec3(r * cos(phi), r * sin(phi), z);
}

// 余弦加权的法向半球采样
vec3 SampleCosineHemisphere(float xi_1, float xi_2, vec3 N) {
    // 均匀采样 xy 圆盘然后投影到 z 半球
    float r = sqrt(xi_1);
    float theta = xi_2 * 2.0 * PI;
    float x = r * cos(theta);
    float y = r * sin(theta);
    float z = sqrt(1.0 - x*x - y*y);

    // 从 z 半球投影到法向半球
    vec3 L = toNormalHemisphere(vec3(x, y, z), N);
    return L;
}

// GTR2 重要性采样
vec3 SampleGTR2(float xi_1, float xi_2, vec3 V, vec3 N, float alpha) {
    
    float phi_h = 2.0 * PI * xi_1;
    float sin_phi_h = sin(phi_h);
    float cos_phi_h = cos(phi_h);

    float cos_theta_h = sqrt((1.0-xi_2)/(1.0+(alpha*alpha-1.0)*xi_2));
    float sin_theta_h = sqrt(max(0.0, 1.0 - cos_theta_h * cos_theta_h));

    // 采样 "微平面" 的法向量 作为镜面反射的半角向量 h 
    vec3 H = vec3(sin_theta_h*cos_phi_h, sin_theta_h*sin_phi_h, cos_theta_h);
    H = toNormalHemisphere(H, N);   // 投影到真正的法向半球

    // 根据 "微法线" 计算反射光方向
    vec3 L = reflect(-V, H);

    return L;
}

// GTR1 重要性采样
vec3 SampleGTR1(float xi_1, float xi_2, vec3 V, vec3 N, float alpha) {
    
    float phi_h = 2.0 * PI * xi_1;
    float sin_phi_h = sin(phi_h);
    float cos_phi_h = cos(phi_h);

    float cos_theta_h = sqrt((1.0-pow(alpha*alpha, 1.0-xi_2))/(1.0-alpha*alpha));
    float sin_theta_h = sqrt(max(0.0, 1.0 - cos_theta_h * cos_theta_h));

    // 采样 "微平面" 的法向量 作为镜面反射的半角向量 h 
    vec3 H = vec3(sin_theta_h*cos_phi_h, sin_theta_h*sin_phi_h, cos_theta_h);
    H = toNormalHemisphere(H, N);   // 投影到真正的法向半球

    // 根据 "微法线" 计算反射光方向
    vec3 L = reflect(-V, H);

    return L;
}

// 按照辐射度分布分别采样三种 BRDF
vec3 SampleBRDF(float xi_1, float xi_2, float xi_3, vec3 V, vec3 N, in Material material) {
    float alpha_GTR1 = mix(0.1, 0.001, material.clearcoatGloss);
    float alpha_GTR2 = max(0.001, sqr(material.roughness));
    
    // 辐射度统计
    float r_diffuse = (1.0 - material.metallic);
    float r_specular = 1.0;
    float r_clearcoat = 0.25 * material.clearcoat;
    float r_sum = r_diffuse + r_specular + r_clearcoat;

    // 根据辐射度计算概率
    float p_diffuse = r_diffuse / r_sum;
    float p_specular = r_specular / r_sum;
    float p_clearcoat = r_clearcoat / r_sum;

    // 按照概率采样
    float rd = xi_3;

    // 漫反射
    if(rd <= p_diffuse) {
        return SampleCosineHemisphere(xi_1, xi_2, N);
    } 
    // 镜面反射
    else if(p_diffuse < rd && rd <= p_diffuse + p_specular) {    
        return SampleGTR2(xi_1, xi_2, V, N, alpha_GTR2);
    } 
    // 清漆
    else if(p_diffuse + p_specular < rd) {
        return SampleGTR1(xi_1, xi_2, V, N, alpha_GTR1);
    }
    return vec3(0, 1, 0);
}

// 采样预计算的 HDR cache
vec3 SampleHdr(float xi_1, float xi_2) {
    vec2 xy = texture2D(hdrCache, vec2(xi_1, xi_2)).rg; // x, y
    xy.y = 1.0 - xy.y; // flip y

    // 获取角度
    float phi = 2.0 * PI * (xy.x - 0.5);    // [-pi ~ pi]
    float theta = PI * (xy.y - 0.5);        // [-pi/2 ~ pi/2]   

    // 球坐标计算方向
    vec3 L = vec3(cos(theta)*cos(phi), sin(theta), cos(theta)*sin(phi));

    return L;
}

// ----------------------------------------------------------------------------- //

// 将三维向量 v 转为 HDR map 的纹理坐标 uv
vec2 toSphericalCoord(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv /= vec2(2.0 * PI, PI);
    uv += 0.5;
    uv.y = 1.0 - uv.y;
    return uv;
}

// 获取 HDR 环境颜色
vec3 hdrColor(vec3 L) {
    vec2 uv = toSphericalCoord(normalize(L));
    vec3 color = texture2D(hdrMap, uv).rgb;
    return color;
}

// 输入光线方向 L 获取 HDR 在该位置的概率密度
// hdr 分辨率为 4096 x 2048 --> hdrResolution = 4096
float hdrPdf(vec3 L, int hdrResolution) {
    vec2 uv = toSphericalCoord(normalize(L));   // 方向向量转 uv 纹理坐标

    float pdf = texture2D(hdrCache, uv).b;      // 采样概率密度
    float theta = PI * (0.5 - uv.y);            // theta 范围 [-pi/2 ~ pi/2]
    float sin_theta = max(sin(theta), 1e-10);

    // 球坐标和图片积分域的转换系数
    float p_convert = float(hdrResolution * hdrResolution / 2) / (2.0 * PI * PI * sin_theta);  
    
    return pdf * p_convert;
}



// 获取 BRDF 在 L 方向上的概率密度
float BRDF_Pdf(vec3 V, vec3 N, vec3 L, in Material material) {
    float NdotL = dot(N, L);
    float NdotV = dot(N, V);
    if(NdotL < 0 || NdotV < 0) return 0;

    vec3 H = normalize(L + V);
    float NdotH = dot(N, H);
    float LdotH = dot(L, H);
     
    // 镜面反射 -- 各向同性
    float alpha = max(0.001, sqr(material.roughness));
    float Ds = GTR2(NdotH, alpha); 
    float Dr = GTR1(NdotH, mix(0.1, 0.001, material.clearcoatGloss));   // 清漆

    // 分别计算三种 BRDF 的概率密度
    float pdf_diffuse = NdotL / PI;
    float pdf_specular = Ds * NdotH / (4.0 * dot(L, H));
    float pdf_clearcoat = Dr * NdotH / (4.0 * dot(L, H));

    // 辐射度统计
    float r_diffuse = (1.0 - material.metallic);
    float r_specular = 1.0;
    float r_clearcoat = 0.25 * material.clearcoat;
    float r_sum = r_diffuse + r_specular + r_clearcoat;

    // 根据辐射度计算选择某种采样方式的概率
    float p_diffuse = r_diffuse / r_sum;
    float p_specular = r_specular / r_sum;
    float p_clearcoat = r_clearcoat / r_sum;

    // 根据概率混合 pdf
    float pdf = p_diffuse   * pdf_diffuse 
              + p_specular  * pdf_specular
              + p_clearcoat * pdf_clearcoat;

    pdf = max(1e-10, pdf);
    return pdf;
}

float misMixWeight(float a, float b) {
    float t = a * a;
    return t / (b*b + t);
}

// ----------------------------------------------------------------------------- //

// nee light sampling advise 

// Your question is a bit hard to answer because it depends what is exactly meant by samples.
// When performing NEE, you can sample N light sources (figuring out how to select these N light sources is another issue). 
// For a point light source, you can simply perform an intersection test and add the contribution. 
// For an area light, you first need to randomly generate a point (a position) on the light source, 
// this is "sampling the light source". 
// If you have a textured area light, 
// then you may want to generate more random positions on your light source so that the textured effect is visible.
// For a point light, this is unnecessary since there is a singular point to sample anyway. For directional lights,
// all the light comes from exactly one direction (by definition) so only one sample is necessary. For an environment map,
// light can come from all directions, so you can try to sample several direction
// (based on the luminance of the map for instance).
// For each sample you take, there will be an intersection test, 
// and this is a performance tradeoff.

// if((is_previous_specular || i == 0) && any(greaterThan(hitMaterial.le, vec3(0)))) {
//                 color += throughput * hitMaterial.le;
//                 break;
// }

vec3 calculatePointLight(Ray ray, HitResult hitdata,inout float pdf){
    if(pointLightSize==0){
        pdf = 0;
        return vec3(0);
    }

    pdf = (float(pointLightSize)) / (2.0 * PI);//半球面采样

    PointLight light = getPointLight(int(rand()*pointLightSize));

    vec3 newDir = normalize((light.position - hitdata.hitPoint));
    
    float dist = length(light.position - hitdata.hitPoint);

     // Test if the point is visible from the light
    Ray shadowRay;
    shadowRay.startPoint = hitdata.hitPoint;
    shadowRay.direction = newDir;

    HitResult shadowHit = hitBVH(shadowRay);

    if(shadowHit.isHit){
        float shadowDist = length(shadowHit.hitPoint - hitdata.hitPoint);
        if(shadowDist < dist){
            return vec3(0);
        }
    }
    
    // Quadratic attenuation
    vec3 pointLightValue = (light.radiance / (dist * dist));

    vec3 brdf_Disney = BRDF_Evaluate(-hitdata.viewDir, hitdata.normal, newDir, hitdata.material);

    return pointLightValue * brdf_Disney * abs(dot(newDir, hitdata.normal)) / pdf;
    //radiance * fr * cos / pdf
}

//do not forget to use sobel sequence
//be awared about this pdf, it may cause NAN problem?
//maybe check if return vec3(0) to avoid this 
vec3 hdriLight(Ray ray, HitResult hitdata,inout float pdf){
    float r1 = rand();
    float r2 = rand();

    vec3 newDir= SampleHdr(r1,r2);

    Ray shadowRay;
    shadowRay.startPoint = hitdata.hitPoint;
    shadowRay.direction = newDir;

    HitResult shadowHit = hitBVH(shadowRay);
    //hit something ,then hdr sample is failed
    if(shadowHit.isHit){
        pdf = 0.0;
        return vec3(0);
    }

    vec3 hdriValue = hdrColor(newDir);

    vec3 brdf_Disney = BRDF_Evaluate(-hitdata.viewDir, hitdata.normal, newDir, hitdata.material);

    pdf = hdrPdf(shadowRay.direction, hdrResolution);

    return brdf_Disney * abs(dot(newDir, hitdata.normal)) * hdriValue / pdf;
}

void shade(Ray ray, HitResult hitdata, vec3 newDir, inout vec3 hitLight, inout vec3 reduction) {

    vec3 brdf_Disney = BRDF_Evaluate(-hitdata.viewDir, hitdata.normal, newDir, hitdata.material);

    float brdfPdf = BRDF_Pdf(-hitdata.viewDir, hitdata.normal, newDir, hitdata.material);
    float hdriPdf = 0;
    float pointPdf = 0;

    vec3 hdriLightCalc = hdriLight(ray,hitdata, hdriPdf);
    vec3 pointLightCalc = calculatePointLight(ray,hitdata,pointPdf);
    vec3 brdfLightCalc = hitdata.material.emissive * (brdf_Disney * abs(dot(newDir, hitdata.normal))) / brdfPdf;

    float sum_weight = hdriPdf + pointPdf + brdfPdf + epsilon; //hdriPdf won't be zero , check later

    float w1 = hdriPdf  / sum_weight;
    float w2 = pointPdf / sum_weight;
    float w3 = brdfPdf  / sum_weight;

    hitLight = reduction * (w1 * hdriLightCalc + w2 * pointLightCalc + w3 * brdfLightCalc);

    reduction *= (brdf_Disney * abs(dot(newDir, hitdata.normal))) / brdfPdf;
}



// ----------------------------------------------------------------------------- //

void main() {
    Ray ray;
    
    ray.startPoint = eye;
    vec2 AA = vec2((rand()-0.5)/float(width), (rand()-0.5)/float(height));

    //uint offsetIdx = frameCounter % 8u;
    //vec2 jitter = vec2(
    //    (Halton_2_3[offsetIdx].x - 0.5f) /float(width),
    //    (Halton_2_3[offsetIdx].y - 0.5f) /float(height)
    //);

    //add jitter, I try to use halton sequence, but the result looks not so good
    //so right now just use random jitter
    vec4 dir = cameraRotate * vec4(pix.xy+AA, -1.0, 0.0);
    ray.direction = normalize(dir.xyz);

//----------------------------
    //have three render target, remember to fill
    // Accumulated radiance
    vec3 color = vec3(0);//to get output(for convenience)

    vec3 light = vec3(0);

    // How much light is lost in the path
    vec3 reduction = vec3(1);

    //------------------
    HitResult firstHit;
    vec3 albe=vec3(1);
    //------------------
    int MAXBOUNCES = max_tracing_depth;
    for (int i = 0; i < MAXBOUNCES; i++){

        HitResult nearestHit = hitBVH(ray);

        if(i == 0){
            if(!nearestHit.isHit){
                Albedo = vec4(1);
            }else{
                Albedo = vec4(nearestHit.material.baseColor,1.0);
                albe=vec3(nearestHit.material.baseColor);
            }
            firstHit = nearestHit;
        }

        if(!nearestHit.isHit){
            light += hdrColor(ray.direction) * reduction;
            break;
        }

        vec2 uv = sobolVec2(frameCounter+1u, uint(i));
        uv = CranleyPattersonRotation(uv);
        float xi_1 = uv.x;
        float xi_2 = uv.y;
        float xi_3 = rand(); 

        // 采样 BRDF 得到一个方向 L
        vec3 L = SampleBRDF(xi_1, xi_2, xi_3, -nearestHit.viewDir, nearestHit.normal, nearestHit.material); 
        float NdotL = dot(nearestHit.normal, L);
        if(NdotL <= 0.0) break;

        vec3 hitLight;//contains the result at this iteration
        shade(ray,nearestHit,L,hitLight,reduction);


        light += hitLight;

        ray.startPoint = nearestHit.hitPoint;
        ray.direction = L;
    }

    light = clamp(light, 0, clamp_threshold);
    if(!isnan(light.x) && !isnan(light.y) && !isnan(light.z)){
        color = light;
    }


 //------------------------------   
    // 和上一帧混合
    if(accumulate){
        vec3 lastColor = texture2D(lastFrame, pix.xy*0.5+0.5).rgb;
        color = mix(lastColor, color, 1.0/float(frameCounter+1u));
    }
    


    fragColor=vec4(color,1.0);
    Emission = vec4(firstHit.material.emissive,1.0);
    Albedo = vec4(firstHit.material.baseColor,1.0);
    
    

    
}

