#version 450

struct Cluster
{
    vec3 pMin;
    vec3 pMax;
    uint triangleStart;
    uint triangleEnd;
    uint objectId;
};


layout(std430, set = 0, binding = 0) buffer readonly ClustersIn {
   Cluster inCluster[ ];
};

struct Vertex {
	vec3 pos;
	vec3 normal;
	vec2 uv;
	vec4 color;
	vec4 joint0;
	vec4 weight0;
	vec4 tangent;
};

layout(std430, set = 0, binding = 1) buffer readonly VerticesIn {
   Vertex inVertices[ ];
};

layout(std430, set = 0, binding = 2) buffer readonly TrianglesIn {
   uint inTriangles[ ];
};

layout(set = 0, binding = 3) buffer readonly ModelMatIn{
	mat4 inModelMats[];
};

layout(set = 0, binding = 4, r32ui) uniform uimage2D visBuffer;

layout(set = 0, binding = 5, r32f) uniform image2D depthBuffer;

layout(set = 0, binding = 6) uniform UBO{
	mat4 invView;
	mat4 invProj;
    vec3 camPos;
}ubo;


layout (binding = 7) uniform UBOParams {
	vec4 lights[4];
	float exposure;
	float gamma;
} uboParams;

layout (binding = 8) uniform samplerCube samplerIrradiance;
layout (binding = 9) uniform sampler2D samplerBRDFLUT;
layout (binding = 10) uniform samplerCube prefilteredMap;

layout(push_constant) uniform PushConstants {
    int vis_clusters;
} pcs;

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;

vec3 getBarycentricCoord(vec3 center, vec3 v0, vec3 v1, vec3 v2)
{
    vec3 v0v1 = v1 - v0;
    vec3 v0v2 = v2 - v0;
    float area2 = length(cross(v0v1, v0v2));

    vec3 barycentric;
    vec3 v1v2 = v2 - v1;
    vec3 v1p = center - v1;
    barycentric.x = length(cross(v1v2, v1p)) / area2;
    vec3 v2v0 = v0 - v2;
    vec3 v2p = center - v2;
    barycentric.y = length(cross(v2v0, v2p)) / area2;
    barycentric.z = 1.0 - barycentric.x - barycentric.y;

    return barycentric;
}


#define PI 3.1415926535897932384626433832795
//#define ALBEDO pow(texture(albedoMap, inUV).rgb, vec3(2.2))
#define ALBEDO vec3(0.5)
// From http://filmicgames.com/archives/75
vec3 Uncharted2Tonemap(vec3 x)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

// Normal Distribution function --------------------------------------
float D_GGX(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
	return (alpha2)/(PI * denom*denom); 
}

// Geometric Shadowing function --------------------------------------
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;
	float GL = dotNL / (dotNL * (1.0 - k) + k);
	float GV = dotNV / (dotNV * (1.0 - k) + k);
	return GL * GV;
}

// Fresnel function ----------------------------------------------------
vec3 F_Schlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
vec3 F_SchlickR(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 prefilteredReflection(vec3 R, float roughness)
{
	const float MAX_REFLECTION_LOD = 9.0; // todo: param/const
	float lod = roughness * MAX_REFLECTION_LOD;
	float lodf = floor(lod);
	float lodc = ceil(lod);
	vec3 a = textureLod(prefilteredMap, R, lodf).rgb;
	vec3 b = textureLod(prefilteredMap, R, lodc).rgb;
	return mix(a, b, lod - lodf);
}

vec3 specularContribution(vec3 L, vec3 V, vec3 N, vec3 F0, float metallic, float roughness)
{
	// Precalculate vectors and dot products	
	vec3 H = normalize (V + L);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);

	// Light color fixed
	vec3 lightColor = vec3(1.0);

	vec3 color = vec3(0.0);

	if (dotNL > 0.0) {
		// D = Normal distribution (Distribution of the microfacets)
		float D = D_GGX(dotNH, roughness); 
		// G = Geometric shadowing term (Microfacets shadowing)
		float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
		// F = Fresnel factor (Reflectance depending on angle of incidence)
		vec3 F = F_Schlick(dotNV, F0);		
		vec3 spec = D * F * G / (4.0 * dotNL * dotNV + 0.001);		
		vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);			
		color += (kD * ALBEDO / PI + spec) * dotNL;
	}

	return color;
}

// vec4 int64_hash_color(int64_t val)
// {
// 	vec4 col;
// 	col.x = uintBitsToFloat((uint)((val>>0)&0xFF)|((val>>32)&0xFF));
// 	col.y = uintBitsToFloat((uint)((val>>8)&0xFF)|((val>>40)&0xFF));
// 	col.z = uintBitsToFloat((uint)((val>>16)&0xFF)|((val>>48)&0xFF));
// 	col.w = uintBitsToFloat((uint)((val>>24)&0xFF)|((val>>54)&0xFF));
// 	return col;
// }


vec4 hashUIntToVec4(uint value) {
    // Constants are chosen arbitrarily for demonstration purposes
    const uint prime1 = 2654435761u;
    const uint prime2 = 2246822519u;
    const uint prime3 = 3266489917u;
    const uint prime4 = 668265263u;

    uint h1 = value * prime1;
    uint h2 = value * prime2;
    uint h3 = value * prime3;
    uint h4 = value * prime4;

    // Apply a non-linear transformation using exp function
    // Adjust the scale to control the range and rate of the exponential growth
    float scale = 4.0 / 255.0;
    return vec4(
        exp(-scale * float(255 - (h1 & 0xFF))),
        exp(-scale * float(255 - (h2 & 0xFF))),
        exp(-scale * float(255 - (h3 & 0xFF))),
        exp(-scale * float(255 - (h4 & 0xFF)))
    );
}



void visualizeID(uint ID)
{
	vec3 color = hashUIntToVec4(ID).xyz;
	// Tone mapping
	color = Uncharted2Tonemap(color * uboParams.exposure);
	color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));	
	// Gamma correction
	color = pow(color, vec3(1.0f / uboParams.gamma));

	outColor = vec4(color,1.0);

}


void main()
{
    ivec2 screenSize = imageSize(visBuffer);
    ivec2 screenPos = ivec2(gl_FragCoord.xy);
    uint ID = imageLoad(visBuffer,screenPos).x;
	if(ID==0xFFFFFFFF) discard;
    float depth = imageLoad(depthBuffer,screenPos).x;
    uint clusterID = ID>>8;
    uint triangleID = ID&0xFF;
	Cluster currCluster = inCluster[clusterID];
    uint globalTriangleID = currCluster.triangleStart+triangleID;
    uint v0i = inTriangles[globalTriangleID*3+0];
    uint v1i = inTriangles[globalTriangleID*3+1];
    uint v2i = inTriangles[globalTriangleID*3+2];
	if(pcs.vis_clusters==2)
	{
		visualizeID(ID); 
		return;
	}
	else if(pcs.vis_clusters==1)
	{
		vec4 inClusterInfos = inVertices[v0i].joint0;
		if(inClusterInfos.x<0.5)
			outColor = vec4(vec3(1.0,0.0,0.0),1.0);
		else if(inClusterInfos.x<1.5)
			outColor = vec4(vec3(0.0,1.0,0.0),1.0);
		else if(inClusterInfos.x<2.5)
			outColor = vec4(vec3(0.0,0.0,1.0),1.0);
		else if(inClusterInfos.x<3.5)
			outColor = vec4(vec3(1.0,1.0,0.0),1.0);
		else if(inClusterInfos.x<4.5)
			outColor = vec4(vec3(1.0,0.0,1.0),1.0);
		else 
			outColor = vec4(vec3(0.0,1.0,1.0),1.0);
		return;
	}
	// outColor = vec4(vec3(triangleID/(0xFF*1.0)),1.0);
	// return;
    
    vec4 ndc = vec4(inUV*2.0-1.0,depth,1.0);
    vec4 worldPos = ubo.invView*ubo.invProj*ndc;
    worldPos.xyz/=worldPos.w;
    mat4 model = inModelMats[currCluster.objectId];
    vec4 v0wpos = model*vec4(inVertices[v0i].pos,1);
    vec4 v1wpos = model*vec4(inVertices[v1i].pos,1);
    vec4 v2wpos = model*vec4(inVertices[v2i].pos,1);
    vec3 bary = getBarycentricCoord(worldPos.xyz,v0wpos.xyz,v1wpos.xyz,v2wpos.xyz);

    vec4 v0nwpos = model*vec4(inVertices[v0i].normal,0);
    vec4 v1nwpos = model*vec4(inVertices[v1i].normal,0);
    vec4 v2nwpos = model*vec4(inVertices[v2i].normal,0);

    vec3 N = normalize(v0nwpos.xyz*bary.x+v1nwpos.xyz*bary.y+v2nwpos.xyz*bary.z);

	vec3 V = normalize(ubo.camPos - worldPos.xyz);
	vec3 R = reflect(-V, N); 

	float metallic = 0.1f;
	float roughness = 0.8f;

	vec3 F0 = vec3(0.04); 
	F0 = mix(F0, ALBEDO, metallic);

	vec3 Lo = vec3(0.0);
	for(int i = 0; i < uboParams.lights[i].length(); i++) {
		vec3 L = normalize(uboParams.lights[i].xyz - worldPos.xyz);
		Lo += specularContribution(L, V, N, F0, metallic, roughness);
	}   
	
	vec2 brdf = texture(samplerBRDFLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
	vec3 reflection = prefilteredReflection(R, roughness).rgb;	
	vec3 irradiance = texture(samplerIrradiance, N).rgb;

	// Diffuse based on irradiance
	vec3 diffuse = irradiance * ALBEDO;	

	vec3 F = F_SchlickR(max(dot(N, V), 0.0), F0, roughness);

	// Specular reflectance
	vec3 specular = reflection * (F * brdf.x + brdf.y);

	// Ambient part
	vec3 kD = 1.0 - F;
	kD *= 1.0 - metallic;	  
	//vec3 ambient = (kD * diffuse + specular) * texture(aoMap, inUV).rrr;
	vec3 ambient = (kD * diffuse + specular);
	vec3 color = ambient + Lo;

	// Tone mapping
	color = Uncharted2Tonemap(color * uboParams.exposure);
	color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));	
	// Gamma correction
	color = pow(color, vec3(1.0f / uboParams.gamma));

	outColor = vec4(color, 1.0);

}