// vertex shader

$input a_position
$output v_color0

#include "bgfx/src/bgfx_shader.sh"
#include "ui_common.s"

void main()
{
    gl_Position = vec4(glPosFromView(a_position), 0.0, 1.0);
    v_color0 = vec4(1.0, 0.0, 0.0, 0.0);
}


