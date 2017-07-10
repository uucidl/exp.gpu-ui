// fragment shader
$input v_color0, v_texcoord0

#include "bgfx/src/bgfx_shader.sh"
#include "ui_common.s"

SAMPLER2D(s_texAlpha, 0);

void main()
{
    vec2 alpha_uv = v_texcoord0;
    float alpha = texture2D(s_texAlpha, alpha_uv).a;
    gl_FragColor = vec4(
        alpha*v_color0.r, alpha*v_color0.g, alpha*v_color0.b,
          alpha);
}


