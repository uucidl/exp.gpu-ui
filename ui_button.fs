// fragment shader
$input v_color0
$input flat v_texcoord0

const float border = 4.0;

#include "bgfx/src/bgfx_shader.sh"
#include "ui_common.s"

void main()
{
    vec4 aabb2 = v_texcoord0;
    vec2 viewPos = viewFromGLFragCoord(gl_FragCoord);
    if (abs(viewPos.x - aabb2.x) <= border ||
        abs(viewPos.x - aabb2.y) <= border ||
        abs(viewPos.y - aabb2.z) <= border ||
        abs(viewPos.y - aabb2.w) <= border) {
        gl_FragColor = rgb8(100, 100, 100);
    } else {
        gl_FragColor = v_color0;
    }
}


