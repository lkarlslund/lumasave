#version 140
// SPDX-License-Identifier: MIT
#include "saturation.glsl"

uniform sampler2D sampler;
uniform vec4 modulation;
uniform float backlightScale;
uniform float blackThreshold;
in vec2 texcoord0;
out vec4 fragColor;

void main()
{
    vec4 encoded = texture2D(sampler, texcoord0);
    vec4 color = encoded;
    color = adjustSaturation(color);
    vec3 low = color.rgb / 12.92;
    vec3 high = pow((color.rgb + 0.055) / 1.055, vec3(2.4));
    vec3 linear = mix(high, low, lessThanEqual(color.rgb, vec3(0.04045)));
    float y = dot(linear, vec3(0.2126, 0.7152, 0.0722));
    if (y > blackThreshold && backlightScale < 0.9999) {
        float shoulder = backlightScale / (1.0 - backlightScale);
        float mapped = clamp(y * (1.0 + shoulder) / (y + shoulder), 0.0, 1.0);
        float blend = smoothstep(blackThreshold, blackThreshold * 2.0, y);
        float target = mix(y, mapped, blend);
        linear = max(linear * (target / max(y, 0.000001)), vec3(0.0));
    }
    low = linear * 12.92;
    high = 1.055 * pow(linear, vec3(1.0 / 2.4)) - 0.055;
    color.rgb = clamp(mix(high, low, lessThanEqual(linear, vec3(0.0031308))), 0.0, 1.0);
    fragColor = color * modulation;
}
