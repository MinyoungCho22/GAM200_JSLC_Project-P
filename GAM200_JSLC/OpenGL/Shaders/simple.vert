#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 projection;
uniform mat4 model;
uniform vec4 spriteRect; 
uniform bool flipX;     

void main()
{
    gl_Position = projection * model * vec4(aPos, 0.0, 1.0);
    
    vec2 transformedTexCoord = aTexCoord;
    
    
    if(flipX)
    {
        transformedTexCoord.x = 1.0 - transformedTexCoord.x;
    }
    
    
    TexCoord.x = transformedTexCoord.x * spriteRect.z + spriteRect.x;
    TexCoord.y = transformedTexCoord.y * spriteRect.w + spriteRect.y;
}