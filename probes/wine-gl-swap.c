/* SPDX-License-Identifier: MIT
 * Regression: multiple swaps with no intervening readback or framebuffer,
 * viewport, program, or VAO rebind. Read only after each complete batch.
 */
static void swap_checks(void);
#define EXTRA_CHECKS swap_checks
#include "wine-gl-window.c"
static void swap_checks(void) {
    const char *vs="#version 330 core\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2-1,0,1);}";
    const char *fs="#version 330 core\nuniform float value;out vec4 c;void main(){c=vec4(value,1-value,0.25,1);}";
    GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs),p=pCreateProgram();
    pAttachShader(p,v);pAttachShader(p,f);pLinkProgram(p);GLint ok;pGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok)fatal("swap probe link");
    pDeleteShader(v);pDeleteShader(f);pUseProgram(p);
    GLuint vao;pGenVertexArrays(1,&vao);pBindVertexArray(vao);
    glViewport(0,0,480,320);HDC dc=wglGetCurrentDC();GLint value=pGetUniformLocation(p,"value");
    for(int count=2;count<=9;count++) {
        int swaps=0;float last=0;
        for(int i=0;i<count;i++) {
            last=(i+1)/16.0f;pUniform1f(value,last);glDrawArrays(GL_TRIANGLES,0,3);
            if(SwapBuffers(dc))swaps++;
        }
        unsigned char got[4]={0};glReadBuffer(GL_FRONT);glReadPixels(120,240,1,1,GL_RGBA,GL_UNSIGNED_BYTE,got);glReadBuffer(GL_BACK);
        int red=(int)(last*255+.5f),green=(int)((1-last)*255+.5f);
        int match=abs(got[0]-red)<=1 && abs(got[1]-green)<=1 && abs(got[2]-64)<=1 && got[3]==(wrong?0:255);
        char name[180];snprintf(name,sizeof(name),"no-readback batch %d final front rgba=%u,%u,%u,%u expected=%d,%d,64,255",count,got[0],got[1],got[2],got[3],red,green);
        check(match,name);check(swaps==count && glGetError()==GL_NO_ERROR,"no-readback swaps and GL error");
    }
    pDeleteProgram(p);pDeleteVertexArrays(1,&vao);
}
