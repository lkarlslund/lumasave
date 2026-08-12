#version 140
// SPDX-License-Identifier: MIT
#include "saturation.glsl"
#include "colormanagement.glsl"

uniform sampler2D sampler;
uniform vec4 modulation;
uniform float backlightScale;
uniform float blackThreshold;
in vec2 texcoord0;
out vec4 fragColor;

void main()
{
    vec4 color = sourceEncodingToNitsInDestinationColorspace(texture2D(sampler, texcoord0));
    color = adjustSaturation(color);
    float y = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
    if (y > blackThreshold && backlightScale < 0.9999) {
        float shoulder = backlightScale / (1.0 - backlightScale);
        float mapped = clamp(y * (1.0 + shoulder) / (y + shoulder), 0.0, 1.0);
        float blend = smoothstep(blackThreshold, blackThreshold * 2.0, y);
        float target = mix(y, mapped, blend);
        color.rgb = clamp(color.rgb * (target / max(y, 0.000001)), 0.0, 1.0);
    }
    fragColor = nitsToDestinationEncoding(color * modulation);
}
