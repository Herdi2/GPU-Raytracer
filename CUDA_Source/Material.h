#pragma once

// Glossy materials with roughness below the cutoff don't use direct Light sampling
#define ROUGHNESS_CUTOFF (0.1f * 0.1f)

__device__ const Texture<float4> * textures;

__device__ inline float2 texture_get_size(int texture_id) {
	if (texture_id == INVALID) {
		return make_float2(0.0f, 0.0f);
	} else {
		return textures[texture_id].size;
	}
}

enum struct MaterialType : char {
	LIGHT      = 0,
	DIFFUSE    = 1,
	DIELECTRIC = 2,
	GLOSSY     = 3
};

struct Material {
	union {
		struct {
			float4 emission;
		} light;
		struct {
			float4 diffuse_and_texture_id;
		} diffuse;
		struct {
			float4 negative_absorption_and_ior;
		} dielectric;
		struct {
			float4 diffuse_and_texture_id;
			float2 ior_and_roughness;
		} glossy;
	};
};

__device__ __constant__ const MaterialType * material_types;
__device__ __constant__ const Material     * materials;

__device__ inline MaterialType material_get_type(int material_id) {
	return material_types[material_id];
}

struct MaterialLight {
	float3 emission;
};

struct MaterialDiffuse {
	float3 diffuse;
	int    texture_id;
};

struct MaterialDielectric {
	float3 negative_absorption;
	float  index_of_refraction;
};

struct MaterialGlossy {
	float3 diffuse;
	int    texture_id;
	float  index_of_refraction;
	float  roughness;
};

__device__ inline MaterialLight material_as_light(int material_id) {
	float4 emission = __ldg(&materials[material_id].light.emission);

	MaterialLight material;
	material.emission = make_float3(emission);
	return material;
}

__device__ inline MaterialDiffuse material_as_diffuse(int material_id) {
	float4 diffuse_and_texture_id = __ldg(&materials[material_id].diffuse.diffuse_and_texture_id);

	MaterialDiffuse material;
	material.diffuse    = make_float3(diffuse_and_texture_id);
	material.texture_id = __float_as_int(diffuse_and_texture_id.w);
	return material;
}

__device__ inline MaterialDielectric material_as_dielectric(int material_id) {
	float4 negative_absorption_and_ior = __ldg(&materials[material_id].dielectric.negative_absorption_and_ior);

	MaterialDielectric material;
	material.negative_absorption = make_float3(negative_absorption_and_ior);
	material.index_of_refraction = negative_absorption_and_ior.w;
	return material;
}

__device__ inline MaterialGlossy material_as_glossy(int material_id) {
	float4 diffuse_and_texture_id = __ldg(&materials[material_id].glossy.diffuse_and_texture_id);
	float2 ior_and_roughness      = __ldg(&materials[material_id].glossy.ior_and_roughness);

	MaterialGlossy material;
	material.diffuse             = make_float3(diffuse_and_texture_id);
	material.texture_id          = __float_as_int(diffuse_and_texture_id.w);
	material.index_of_refraction = ior_and_roughness.x;
	material.roughness           = ior_and_roughness.y;
	return material;
}

__device__ inline float3 material_get_albedo(const float3 & diffuse, int texture_id, float s, float t) {
	if (texture_id == INVALID) return diffuse;

	float4 tex_colour = textures[texture_id].get(s, t);
	return diffuse * make_float3(tex_colour);
}

__device__ inline float3 material_get_albedo(const float3 & diffuse, int texture_id, float s, float t, float lod) {
	if (texture_id == INVALID) return diffuse;

	float4 tex_colour = textures[texture_id].get_lod(s, t, lod);
	return diffuse * make_float3(tex_colour);
}

__device__ inline float3 material_get_albedo(const float3 & diffuse, int texture_id, float s, float t, float2 dx, float2 dy) {
	if (texture_id == INVALID) return diffuse;

	float4 tex_colour = textures[texture_id].get_grad(s, t, dx, dy);
	return diffuse * make_float3(tex_colour);
}

__device__ __constant__ float lights_total_power;

__device__ __constant__ const int   * light_indices;
__device__ __constant__ const float * light_power_cumulative;

__device__ __constant__ const int   * light_mesh_triangle_count;
__device__ __constant__ const int   * light_mesh_triangle_first_index;
__device__ __constant__ const float * light_mesh_power_scaled;
__device__ __constant__ const float * light_mesh_power_unscaled;
__device__ __constant__ const int   * light_mesh_transform_indices;

// Assumes no Total Internal Reflection
__device__ inline float fresnel(float eta_1, float eta_2, float cos_theta_i, float cos_theta_t) {
	float s = (eta_1 * cos_theta_i - eta_2 * cos_theta_t) / (eta_1 * cos_theta_i + eta_2 * cos_theta_t);
	float p = (eta_1 * cos_theta_t - eta_2 * cos_theta_i) / (eta_1 * cos_theta_t + eta_2 * cos_theta_i);

	return 0.5f * (s*s + p*p);
}

__device__ inline float fresnel_schlick(float eta_1, float eta_2, float cos_theta_i) {
	float r_0 = (eta_1 - eta_2) / (eta_1 + eta_2);
	r_0 *= r_0;

	return r_0 + (1.0f - r_0) * (cos_theta_i * cos_theta_i * cos_theta_i * cos_theta_i * cos_theta_i);
}

// Distribution of Normals term D for the GGX microfacet model
__device__ inline float ggx_D(const float3 & micro_normal, float alpha_x, float alpha_y) {
	float sx = -micro_normal.x / (micro_normal.z * alpha_x);
	float sy = -micro_normal.y / (micro_normal.z * alpha_y);

	float sl = 1.0f + sx * sx + sy * sy;

	float cos_theta_2 = micro_normal.z * micro_normal.z;
	float cos_theta_4 = cos_theta_2 * cos_theta_2;

	return 1.0f / (sl * sl * PI * alpha_x * alpha_y * cos_theta_4);
}

__device__ inline float ggx_lambda(const float3 & omega, float alpha_x2, float alpha_y2) {
	return (sqrtf(1.0f + (alpha_x2 * omega.x * omega.x + alpha_y2 * omega.y * omega.y) / (omega.z * omega.z)) - 1.0f) * 0.5f;
}

// Monodirectional Smith shadowing/masking term
__device__ inline float ggx_G1(const float3 & omega, float alpha_x2, float alpha_y2) {
	return 1.0f / (1.0f + ggx_lambda(omega, alpha_x2, alpha_y2));
}

// Height correlated shadowing and masking term
__device__ inline float ggx_G2(const float3 & omega_o, const float3 & omega_i, const float3 & omega_m, float alpha_x2, float alpha_y2) {
	float o_dot_m = dot(omega_o, omega_m);
	float i_dot_m = dot(omega_i, omega_m);

	if (o_dot_m <= 0.0f || i_dot_m <= 0.0f) {
		return 0.0f;
	} else {
		return 1.0f / (1.0f + ggx_lambda(omega_o, alpha_x2, alpha_y2) + ggx_lambda(omega_i, alpha_x2, alpha_y2));
	}
}

__device__ inline float ggx_eval(const float3 & omega_o, const float3 & omega_i, float ior, float alpha_x, float alpha_y, float & pdf) {
	float alpha_x2 = alpha_x * alpha_x;
	float alpha_y2 = alpha_y * alpha_y;

	float3 half_vector = normalize(omega_o + omega_i);
	float mu = fmaxf(0.0, dot(omega_o, half_vector));

	float F  = fresnel_schlick(ior, 1.0f, mu);
	float D  = ggx_D(half_vector, alpha_x, alpha_y);
	float G1 = ggx_G1(omega_o, alpha_x2, alpha_y2);
	float G2 = ggx_G2(omega_o, omega_i, half_vector, alpha_x2, alpha_y2);

	float denom_inv = 1.0f / (4.0f * omega_i.z * omega_o.z);

	pdf = G1 * D * mu * denom_inv;
	return F * D * G2 * denom_inv;
}
