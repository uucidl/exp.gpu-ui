// view coordinate system

vec2 glPosFromView(vec2 viewPosition)
{
    viewPosition.y = u_viewRect.w - viewPosition.y;
    vec2 glPosPerViewPos = 1.0/(0.5*u_viewRect.zw);
    return viewPosition*glPosPerViewPos - vec2(1.0, 1.0);
}

vec2 viewFromGLFragCoord(vec4 fc)
{
    return vec2(fc.x, u_viewRect.w-fc.y);
}

// color creation

vec4 rgb8(int r, int g, int b)
{
    return vec4(float(r)/255.0, float(g)/255.0, float(b)/255.0, 0.0);
}


