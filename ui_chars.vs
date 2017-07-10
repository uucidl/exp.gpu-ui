// vertex shader

$output v_color0
$output v_texcoord0

$input a_position
$input a_texcoord0
$input a_color0

#include "bgfx/src/bgfx_shader.sh"
#include "ui_common.s"

void main()
{
    gl_Position = vec4(glPosFromView(a_position), 0.0, 1.0);
    v_texcoord0 = a_texcoord0;
    v_color0 = a_color0;
}


