#version 330 core

in vec3 view_pos;
in vec3 view_normal;
in vec3 world_pos;
in vec3 world_normal;
in vec3 world_view_pos;
in vec4 lightview_pos;
in vec2 tex_coord;

uniform vec4 camera_pos;
uniform vec4 light_pos;
uniform vec3 world_light;
uniform sampler2D texture_diffuse;
uniform sampler2D texture_specular;
uniform sampler2D model_shadow;
uniform sampler2DShadow model_shadow_cmp;
uniform samplerCube env;
uniform float blocker_search_num_samples = 16.0f;
uniform float pcf_num_samples = 16.0f;
uniform float light_world_size;
uniform bool do_pcss;

layout(location = 0) out vec3 color;
uniform vec2 poissonDisk[16];
uniform vec2 filter_neightbour_offsets[16] = vec2[16](
    vec2(-2.0f, -2.0f), vec2(-2.0f, -1.0f), vec2(-2.0f, 1.0f), vec2(-2.0f, 2.0f),
    vec2(-1.0f, -2.0f), vec2(-1.0f, -1.0f), vec2(-1.0f, 1.0f), vec2(-1.0f, 2.0f),
    vec2(1.0f, -2.0f), vec2(1.0f, -1.0f), vec2(1.0f, 1.0f), vec2(1.0f, 2.0f),
    vec2(2.0f, -2.0f), vec2(2.0f, -1.0f), vec2(2.0f, 1.0f), vec2(2.0f, 2.0f)
);

// https://developer.download.nvidia.com/whitepapers/2008/PCSS_Integration.pdf
void FindBlocker(out float avgBlockerDepth, out float numBlockers, float zReceiver, float bias)
{
    float blockerSum = 0;
    float searchWidth = (light_world_size) * (zReceiver - 0.1f) / zReceiver; 
    numBlockers = 0;
    for( int i = 0; i < blocker_search_num_samples; i++ )
    {
        vec4 lightview_pos_search = lightview_pos + vec4(poissonDisk[i] * searchWidth * lightview_pos.w, 0, 0);
        float shadowMapDepth = textureProj(model_shadow, lightview_pos_search).r;
        if ( shadowMapDepth < zReceiver + bias) {
            blockerSum += shadowMapDepth;
            numBlockers++;
        }
    }
    avgBlockerDepth = blockerSum / numBlockers;
}

float PCF_Filter(float filterRadiusUV, float zReceiver, float bias)
{
    float sum = 0.0f;
    vec2 texelSize = 1.0 / textureSize(model_shadow, 0);
    float x,y;
	for (int i = 0; i < pcf_num_samples; i++) {
        // vec2 offset = poissonDisk[i] * filterRadiusUV * 1000 * texelSize * lightview_pos.w;
        vec2 offset = filter_neightbour_offsets[i] * texelSize * lightview_pos.w;
        vec4 lightview_pos_filter = lightview_pos + vec4(offset, bias, 0);
        float shadowMapDepth = textureProj(model_shadow_cmp, lightview_pos_filter);
        sum += shadowMapDepth;
    }
    return sum / pcf_num_samples;
} 


float PCSS (vec3 coords)
{
    // STEP 1: Blocker search
    float avgBlockerDepth = 0;
    float numBlockers = 0;
    float bias = max(0.0005f * (1.0f - dot(normalize(world_normal), normalize(world_light - world_pos))), 0.00001f);
    FindBlocker(avgBlockerDepth, numBlockers, coords.z, bias);
    if( numBlockers < 1 )
    {
        return 0.0f;
    }
    // STEP 2: Estimate the Penumbra size
    float penumbraRatio = (avgBlockerDepth - coords.z) / avgBlockerDepth;
    float filterRadiusUV = penumbraRatio * light_world_size * 0.1f / coords.z;
    // STEP 3: Percentage Closer Filtering
    return PCF_Filter(filterRadiusUV, coords.z, bias);
}

void main()
{
    vec3 diffuse_color = texture(texture_diffuse, tex_coord).rgb;
    vec3 light_color = vec3(1.0, 1.0, 1.0);
    vec3 ambient_light_color = vec3(0.2, 0.2, 0.2);
    vec3 specular_color = texture(texture_specular, tex_coord).rgb;
    float specular_power = 20.0;
    vec3 view_dir = normalize(camera_pos.xyz - world_pos);
    vec3 half_vector = normalize(light_pos.xyz - view_dir.xyz);
    vec3 lightview_p = (lightview_pos.xyz / lightview_pos.w);
    if (lightview_p.x > 0.0 && lightview_p.x < 1 && lightview_p.y > 0.0 && lightview_p.y < 1)
    {
        if (do_pcss) {
            light_color *= (1 - PCSS(lightview_p));
        }
        else {
            light_color *= (1 - textureProj(model_shadow_cmp, lightview_pos));
        }   
    }
    vec3 diffuse_shading = (light_color * clamp(dot(normalize(view_normal), normalize(light_pos.xyz - view_pos)), 0, 1) * diffuse_color);
    vec3 specular_shading = (light_color * pow(clamp(dot(normalize(view_normal), normalize(half_vector)), 0, 1), specular_power) * specular_color);
    vec3 ambient_shading = (ambient_light_color * diffuse_color);
    
    color = (diffuse_shading + specular_shading + ambient_shading);
}