// vertex shader

// TODO(nicolas): shouldn't shaders actually deal with n elements rather than
// one?

$output v_color0
$output flat v_texcoord0

$input a_position
$input a_color0
$input a_texcoord0

#include "bgfx/src/bgfx_shader.sh"
#include "ui_common.s"

void main()
{
    gl_Position = vec4(glPosFromView(a_position), 0.0, 1.0);
    v_color0 = a_color0 > 0.0?
        rgb8(255, 181, 50) :
        rgb8(180, 180, 180);
    v_texcoord0 = a_texcoord0;
}


