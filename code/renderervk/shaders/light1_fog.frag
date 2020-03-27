#version 450

layout(set = 0, binding = 0) uniform UBO {
	// VERTEX
	vec4 eyePos;
	vec4 lightPos;
	//  VERTEX-FOG
	vec4 fogDistanceVector;
	vec4 fogDepthVector;
	vec4 fogEyeT;
	// FRAGMENT
	vec4 lightColor;
	vec4 fogColor;
	// linear dynamic light
    vec4 lightVector;
};

layout(set = 1, binding = 0) uniform sampler2D texture0;
//layout(set = 2, binding = 0) uniform sampler2D texture1;
//layout(set = 3, binding = 0) uniform sampler2D texture2;
layout(set = 4, binding = 0) uniform sampler2D fogtexture;

layout(location = 0) centroid in vec2 frag_tex_coord;
layout(location = 1) in vec3 N;  // normalized object-space normal vector
layout(location = 2) in vec4 L;  // object-space light vector
layout(location = 3) in vec4 V;  // object-space view vector
layout(location = 4) centroid in vec2 fog_tex_coord;

layout(location = 0) out vec4 out_color;

layout (constant_id = 0) const int alpha_test_func = 0;
layout (constant_id = 1) const float alpha_test_value = 0.0;
//layout (constant_id = 2) const float depth_fragment = 0.85;
layout (constant_id = 3) const int alpha_to_coverage = 0;
//layout (constant_id = 4) const int color_mode = 0;
layout (constant_id = 5) const int abs_light = 0;

float CorrectAlpha(float threshold, float alpha, vec2 tc)
{
	ivec2 ts = textureSize(texture0, 0);
	float dx = max(abs(dFdx(tc.x * float(ts.x))), 0.001);
	float dy = max(abs(dFdy(tc.y * float(ts.y))), 0.001);
	float dxy = max(dx, dy); // apply the smallest boost
	float scale = max(1.0 / dxy, 1.0);
	float ac = threshold + (alpha - threshold) * scale;
	return ac;
}

void main() {
	vec4 base = texture(texture0, frag_tex_coord);
	vec4 fog = texture(fogtexture, fog_tex_coord);

	if (alpha_to_coverage != 0) {
		if (alpha_test_func == 1) {
			base.a = CorrectAlpha(alpha_test_value, base.a, frag_tex_coord);
		} else if (alpha_test_func == 2) {
			base.a = CorrectAlpha(alpha_test_value, 1.0 - base.a, frag_tex_coord);
		} else if (alpha_test_func == 3) {
			base.a = CorrectAlpha(alpha_test_value, base.a, frag_tex_coord);
		}
	} else
	// specialization: alpha-test function
	if (alpha_test_func == 1) {
		if (base.a == alpha_test_value) discard;
	} else if (alpha_test_func == 2) {
		if (base.a >= alpha_test_value) discard;
	} else if (alpha_test_func == 3) {
		if (base.a < alpha_test_value) discard;
	}

	vec4 lightColorRadius = lightColor;

	// project fragment on light vector
	float scale = clamp( dot( -L.xyz, lightVector.xyz ) * lightVector.w, 0.0, 1.0 );
	//
	vec4 LL = lightVector * scale + L;

	vec3 nL = normalize(LL.xyz);	// normalized light vector
	vec3 nV = normalize(V.xyz);	// normalized view vector

	// light intensity
	float intensFactor = 1.0 - dot(LL.xyz, LL.xyz) * lightColorRadius.w;
	if (intensFactor <= 0.0)
		discard;
	vec3 intens = lightColorRadius.rgb * intensFactor;

	// modulate base by inverted fog alpha
	base.xyz = base.xyz * ( 1.0 - fog.a );

	// Lambertian diffuse reflection term (N.L)
	float diffuse = dot(N, nL);

	// specular reflection term (N.H)
	float specFactor = dot(N, normalize(nL + nV));

	if ( abs_light != 0 )
	{
		// make sure that light and eye vectors are on the same plane side
		if ( diffuse * dot(N, nV) <= 0 )
			discard;

		diffuse = abs( diffuse );
		specFactor = abs( specFactor );
	}

	//float spec = pow(specFactor, 8.0) * 0.25;
	vec4 spec = vec4(pow(specFactor, 10.0)*0.25) * base * 0.8;

	out_color = (base * vec4(diffuse) + spec) * vec4(intens, 1.0);
}
