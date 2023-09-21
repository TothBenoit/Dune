#include "PostProcessGlobals.h"
#include "DirectionalLight.h"

ConstantBuffer<PostProcessGlobals> globals : register(b0);

Texture2D sceneColor : register(t0);
Texture2D sceneDepth : register(t1);
StructuredBuffer<DirectionalLight> DirectionalLights : register(t2);
SamplerState sceneSampler : register(s0);

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

VSOutput VSMain(uint VertexID : SV_VertexID)
{
    VSOutput output;
    output.uv = float2((VertexID << 1) & 2, VertexID & 2);
    output.position = float4(output.uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return output;
}

struct PSInput
{
    float4 position: SV_Position;
    float2 uv : TEXCOORD;
};

/*************************************************/
/*     https://www.shadertoy.com/view/wlBXWK     */
/*************************************************/

/*
MIT License

Copyright (c) 2019 Dimas Leenman

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Scattering works by calculating how much light is scattered to the camera on a certain path/
This implementation does that by taking a number of samples across that path to check the amount of light that reaches the path
and it calculates the color of this light from the effects of scattering.

There are two types of scattering, rayleigh and mie
rayleigh is caused by small particles (molecules) and scatters certain colors better than others (causing a blue sky on earth)
mie is caused by bigger particles (like water droplets), and scatters all colors equally, but only in a certain direction.
Mie scattering causes the red sky during the sunset, because it scatters the remaining red light

To know where the ray starts and ends, we need to calculate where the ray enters and exits the atmosphere
We do this using a ray-sphere intersect

The scattering code is based on https://www.scratchapixel.com/lessons/procedural-generation-virtual-worlds/simulating-sky
with some modifications to allow moving the planet, as well as objects inside the atmosphere, correct light absorbsion
from objects in the scene and an ambient scattering term tp light up the dark side a bit if needed

the camera also moves up and down, and the sun rotates around the planet as well

Note: 	Because rayleigh is a long word to type, I use ray instead on most variable names
		the same goes for position (which becomes pos), direction (which becomes dir) and optical (becomes opt)
*/

// first, lets define some constants to use (planet radius, position, and scattering coefficients)
#define PLANET_RADIUS 6371e3 /* radius of the planet */
#define ATMOS_RADIUS 6471e3 /* radius of the atmosphere */
#define PLANET_POS float3(0.0f, -PLANET_RADIUS - 100.f, 0.0f) /* the position of the planet */
// scattering coeffs
#define RAY_BETA float3(5.5e-6, 13.0e-6, 22.4e-6) /* rayleigh, affects the color of the sky */
#define MIE_BETA float3(21e-6f, 21e-6f, 21e-6f) /* mie, affects the color of the blob around the sun */
#define AMBIENT_BETA float3(0.0f, 0.0f, 0.0f) /* ambient, affects the scattering color when there is no lighting from the sun */
#define ABSORPTION_BETA float3(2.04e-5, 4.97e-5, 1.95e-6) /* what color gets absorbed by the atmosphere (Due to things like ozone) */
#define G 0.7 /* mie scattering direction, or how big the blob around the sun is */
// and the heights (how far to go up before the scattering has no effect)
#define HEIGHT_RAY 8e3 /* rayleigh height */
#define HEIGHT_MIE 1.2e3 /* and mie */
#define HEIGHT_ABSORPTION 30e3 /* at what height the absorption is at it's maximum */
#define ABSORPTION_FALLOFF 4e3 /* how much the absorption decreases the further away it gets from the maximum height */

#define PRIMARY_STEPS 32 /* primary steps, affects quality the most */
#define LIGHT_STEPS 8 /* light steps, how much steps in the light direction are taken */

/*
Next we'll define the main scattering function.
This traces a ray from start to end and takes a certain amount of samples along this ray, in order to calculate the color.
For every sample, we'll also trace a ray in the direction of the light,
because the color that reaches the sample also changes due to scattering
*/
float3 calculate_scattering(
    float3 start, 				// the start of the ray (the camera position)
    float3 dir, 					// the direction of the ray (the camera vector)
    float max_dist, 			// the maximum distance the ray can travel (because something is in the way, like an object)
    float3 scene_color,			// the color of the scene
    float3 light_dir, 			// the direction of the light
    float3 light_intensity,		// how bright the light is, affects the brightness of the atmosphere
    float3 planet_position, 		// the position of the planet
    float planet_radius, 		// the radius of the planet
    float atmo_radius, 			// the radius of the atmosphere
    float3 beta_ray, 				// the amount rayleigh scattering scatters the colors (for earth: causes the blue atmosphere)
    float3 beta_mie, 				// the amount mie scattering scatters colors
    float3 beta_absorption,   	// how much air is absorbed
    float3 beta_ambient,			// the amount of scattering that always occurs, cna help make the back side of the atmosphere a bit brighter
    float g, 					// the direction mie scatters the light in (like a cone). closer to -1 means more towards a single direction
    float height_ray, 			// how high do you have to go before there is no rayleigh scattering?
    float height_mie, 			// the same, but for mie
    float height_absorption,	// the height at which the most absorption happens
    float absorption_falloff,	// how fast the absorption falls off from the absorption height
    int steps_i, 				// the amount of steps along the 'primary' ray, more looks better but slower
    int steps_l 				// the amount of steps along the light ray, more looks better but slower
) {
    // add an offset to the camera position, so that the atmosphere is in the correct position
    start -= planet_position;

    // calculate the start and end position of the ray, as a distance along the ray
    // we do this with a ray sphere intersect
    float a = dot(dir, dir);
    float b = 2.0f * dot(dir, start);
    float c = dot(start, start) - (atmo_radius * atmo_radius);
    float d = (b * b) - 4.0f * a * c;
    
    // stop early if there is no intersect
    if (d < 0.0f)
        return scene_color;
    
    // calculate the ray length
    float2 ray_length = float2(
        max((-b - sqrt(d)) / (2.0f * a), 0.0f),
        min((-b + sqrt(d)) / (2.0f * a), max_dist)
    );

    // if the ray did not hit the atmosphere, return a black color
    if (ray_length.x > ray_length.y)
        return scene_color;
    
    // prevent the mie glow from appearing if there's an object in front of the camera
    bool allow_mie = max_dist > ray_length.y;

    // make sure the ray is no longer than allowed
    ray_length.y = min(ray_length.y, max_dist);
    ray_length.x = max(ray_length.x, 0.0f);
    // get the step size of the ray
    float step_size_i = (ray_length.y - ray_length.x) / float(steps_i);

    // next, set how far we are along the ray, so we can calculate the position of the sample
    // if the camera is outside the atmosphere, the ray should start at the edge of the atmosphere
    // if it's inside, it should start at the position of the camera
    // the min statement makes sure of that
    float ray_pos_i = ray_length.x + step_size_i * 0.5f;

    // these are the values we use to gather all the scattered light
    float3 total_ray = float3(0.0f, 0.0f, 0.0f); // for rayleigh
    float3 total_mie = float3(0.0f, 0.0f, 0.0f); // for mie

    // initialize the optical depth. This is used to calculate how much air was in the ray
    float3 opt_i = float3(0.0f, 0.0f, 0.0f);

    // also init the scale height, avoids some float2's later on
    float2 scale_height = float2(height_ray, height_mie);

    // Calculate the Rayleigh and Mie phases.
    // This is the color that will be scattered for this ray
    // mu, mumu and gg are used quite a lot in the calculation, so to speed it up, precalculate them
    float mu = dot(dir, light_dir);
    float mumu = mu * mu;
    float gg = g * g;
    float phase_ray = 3.0f / (50.2654824574f /* (16 * pi) */) * (1.0f + mumu);
    float phase_mie = allow_mie ? 3.0f / (25.1327412287f /* (8 * pi) */) * ((1.0f - gg) * (mumu + 1.0f)) / (pow(1.0f + gg - 2.0f * mu * g, 1.5f) * (2.0f + gg)) : 0.0f;

    // now we need to sample the 'primary' ray. this ray gathers the light that gets scattered onto it
    for (int i = 0; i < steps_i; ++i) {

        // calculate where we are along this ray
        float3 pos_i = start + dir * ray_pos_i;

        // and how high we are above the surface
        float height_i = length(pos_i) - planet_radius;
        
        // now calculate the density of the particles (both for rayleigh and mie)
        float3 density = float3(exp(-height_i / scale_height), 0.0f);

        // and the absorption density. this is for ozone, which scales together with the rayleigh, 
        // but absorbs the most at a specific height, so use the sech function for a nice curve falloff for this height
        // clamp it to avoid it going out of bounds. This prevents weird black spheres on the night side
        float denom = (height_absorption - height_i) / absorption_falloff;
        density.z = (1.0f / (denom * denom + 1.0f)) * density.x;

        // multiply it by the step size here
        // we are going to use the density later on as well
        density *= step_size_i;

        // Add these densities to the optical depth, so that we know how many particles are on this ray.
        opt_i += density;

        // Calculate the step size of the light ray.
        // again with a ray sphere intersect
        // a, b, c and d are already defined
        a = dot(light_dir, light_dir);
        b = 2.0f * dot(light_dir, pos_i);
        c = dot(pos_i, pos_i) - (atmo_radius * atmo_radius);
        d = (b * b) - 4.0f * a * c;

        // no early stopping, this one should always be inside the atmosphere
        // calculate the ray length
        float step_size_l = (-b + sqrt(d)) / (2.0f * a * float(steps_l));

        // and the position along this ray
        // this time we are sure the ray is in the atmosphere, so set it to 0
        float ray_pos_l = step_size_l * 0.5f;

        // and the optical depth of this ray
        float3 opt_l = float3(0.0f, 0.0f, 0.0f);

        // now sample the light ray
        // this is similar to what we did before
        for (int l = 0; l < steps_l; ++l) {

            // calculate where we are along this ray
            float3 pos_l = pos_i + light_dir * ray_pos_l;

            // the heigth of the position
            float height_l = length(pos_l) - planet_radius;

            // calculate the particle density, and add it
            // this is a bit verbose
            // first, set the density for ray and mie
            float3 density_l = float3(exp(-height_l.rr / scale_height), 0.0f);

            // then, the absorption
            float denom = (height_absorption - height_l) / absorption_falloff;
            density_l.z = (1.0f / (denom * denom + 1.0f)) * density_l.x;

            // multiply the density by the step size
            density_l *= step_size_l;

            // and add it to the total optical depth
            opt_l += density_l;

            // and increment where we are along the light ray.
            ray_pos_l += step_size_l;

        }

        // Now we need to calculate the attenuation
        // this is essentially how much light reaches the current sample point due to scattering
        float3 attn = exp(-beta_ray * (opt_i.x + opt_l.x) - beta_mie * (opt_i.y + opt_l.y) - beta_absorption * (opt_i.z + opt_l.z));

        // accumulate the scattered light (how much will be scattered towards the camera)
        total_ray += density.x * attn;
        total_mie += density.y * attn;

        // and increment the position on this ray
        ray_pos_i += step_size_i;

    }

    // calculate how much light can pass through the atmosphere
    float3 opacity = exp(-(beta_mie * opt_i.y + beta_ray * opt_i.x + beta_absorption * opt_i.z));

    // calculate and return the final color
    return (
        phase_ray * beta_ray * total_ray // rayleigh color
        + phase_mie * beta_mie * total_mie // mie
        + opt_i.x * beta_ambient // and ambient
        ) * light_intensity + scene_color * opacity; // now make sure the background is rendered correctly
}

/*
A ray-sphere intersect
This was previously used in the atmosphere as well, but it's only used for the planet intersect now, since the atmosphere has this
ray sphere intersect built in
*/

float2 ray_sphere_intersect(
    float3 start, // starting position of the ray
    float3 dir, // the direction of the ray
    float radius // and the sphere radius
) {
    // ray-sphere intersection that assumes
    // the sphere is centered at the origin.
    // No intersection when result.x > result.y
    float a = dot(dir, dir);
    float b = 2.0 * dot(dir, start);
    float c = dot(start, start) - (radius * radius);
    float d = (b * b) - 4.0 * a * c;
    if (d < 0.0) return float2(1e5, -1e5);
    return float2(
        (-b - sqrt(d)) / (2.0 * a),
        (-b + sqrt(d)) / (2.0 * a)
    );
}

float4 get_camera_vector(float2 coord) {
    
    float2 screenPos = coord * float2(2, -2) + float2(-1, 1);
    float depth = sceneDepth.Sample(sceneSampler, coord).r;
    float4 wpos = mul(globals.m_invViewProj, float4(screenPos, depth, 1.f));
    wpos /= wpos.w;
    float3 camToPixel = wpos.xyz - globals.m_cameraPosition.xyz;
    float dist = length(camToPixel);
    return float4(camToPixel / dist, step(1, depth) * 1e12 + dist);
}

float4 PSMain(PSInput input) : SV_Target
{ 
    float4 scene = sceneColor.Sample(sceneSampler, input.uv);
    
    uint lightCount;
    uint stride;
    DirectionalLights.GetDimensions(lightCount, stride);
    
    if (lightCount == 0)
    {
        return scene;
    }
    
    float3 camera_position = globals.m_cameraPosition.xyz;
    float4 camera_vector = get_camera_vector(input.uv);
    float3 light_dir = -DirectionalLights[0].m_dir;

    // the color of this pixel
    float3 col = float3(0.0f, 0.0f, 0.0f);

    // get the atmosphere color
    col += calculate_scattering(
        camera_position,				// the position of the camera
        camera_vector.xyz, 				// the camera vector (ray direction of this pixel)
        camera_vector.w,                // max dist, essentially the scene depth
        scene.xyz,						// scene color, the color of the current pixel being rendered
        light_dir,						// light direction
        float3(40.f, 40.f, 40.f),		// light intensity, 40 looks nice
        PLANET_POS,						// position of the planet
        PLANET_RADIUS,                  // radius of the planet in meters
        ATMOS_RADIUS,                   // radius of the atmosphere in meters
        RAY_BETA,						// Rayleigh scattering coefficient
        MIE_BETA,                       // Mie scattering coefficient
        ABSORPTION_BETA,                // Absorbtion coefficient
        AMBIENT_BETA,					// ambient scattering, turned off for now. This causes the air to glow a bit when no light reaches it
        G,                          	// Mie preferred scattering direction
        HEIGHT_RAY,                     // Rayleigh scale height
        HEIGHT_MIE,                     // Mie scale height
        HEIGHT_ABSORPTION,				// the height at which the most absorption happens
        ABSORPTION_FALLOFF,				// how fast the absorption falls off from the absorption height 
        PRIMARY_STEPS, 					// steps in the ray direction 
        LIGHT_STEPS 					// steps in the light direction
    );

    // apply exposure, removing this makes the brighter colors look ugly
    // you can play around with removing this
    col = 1.0f - exp(-col);

    return float4(col, 1.0f);
}