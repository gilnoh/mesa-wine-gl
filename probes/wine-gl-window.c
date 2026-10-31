/* SPDX-License-Identifier: MIT
 * Windows-side integration probe: real WGL, GL dispatch, sharing and presentation.
 * Pixel readback proves rendering; a separate window capture proves presentation.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/wglext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROCS(X) \
 X(PFNGLCREATESHADERPROC,CreateShader) X(PFNGLSHADERSOURCEPROC,ShaderSource) \
 X(PFNGLCOMPILESHADERPROC,CompileShader) X(PFNGLGETSHADERIVPROC,GetShaderiv) \
 X(PFNGLGETSHADERINFOLOGPROC,GetShaderInfoLog) X(PFNGLCREATEPROGRAMPROC,CreateProgram) \
 X(PFNGLATTACHSHADERPROC,AttachShader) X(PFNGLLINKPROGRAMPROC,LinkProgram) \
 X(PFNGLGETPROGRAMIVPROC,GetProgramiv) X(PFNGLGETPROGRAMINFOLOGPROC,GetProgramInfoLog) \
 X(PFNGLUSEPROGRAMPROC,UseProgram) X(PFNGLGENVERTEXARRAYSPROC,GenVertexArrays) \
 X(PFNGLBINDVERTEXARRAYPROC,BindVertexArray) X(PFNGLGETUNIFORMLOCATIONPROC,GetUniformLocation) \
 X(PFNGLUNIFORM1FPROC,Uniform1f) X(PFNGLUNIFORM2FPROC,Uniform2f) X(PFNGLFENCESYNCPROC,FenceSync) \
 X(PFNGLCLIENTWAITSYNCPROC,ClientWaitSync) X(PFNGLDELETESYNCPROC,DeleteSync) \
 X(PFNGLDELETEPROGRAMPROC,DeleteProgram) X(PFNGLDELETESHADERPROC,DeleteShader) \
 X(PFNGLDELETEVERTEXARRAYSPROC,DeleteVertexArrays)
#define DECL(type,name) static type p##name;
PROCS(DECL)
static unsigned checks, failures;
static int wrong, closed;
static void check(int ok, const char *name) {
    checks++; if (!ok) failures++;
    printf("%s %s\n",ok?"PASS":"FAIL",name);
}
static void fatal(const char *name) { fprintf(stderr,"FATAL %s (Win32=%lu GL=%x)\n",name,GetLastError(),glGetError()); exit(2); }
static void messages(void) { MSG msg; while(PeekMessageA(&msg,NULL,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); } }
static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if(msg==WM_CLOSE) { closed=1; return 0; }
    return DefWindowProcA(hwnd,msg,w,l);
}
static GLuint shader(GLenum type,const char *src) {
    GLuint s=pCreateShader(type); GLint ok; char log[4096];
    pShaderSource(s,1,&src,NULL);pCompileShader(s);pGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){pGetShaderInfoLog(s,sizeof(log),NULL,log);fprintf(stderr,"%s\n",log);fatal("shader compilation");}
    return s;
}
static void draw(HWND hwnd, GLuint program, GLuint vao, int verify) {
    RECT rc; GetClientRect(hwnd,&rc); int w=rc.right,h=rc.bottom;
    glViewport(0,0,w,h); glDisable(GL_DITHER); glDisable(GL_BLEND); glDisable(GL_DEPTH_TEST);
    pUseProgram(program); pUniform2f(pGetUniformLocation(program,"size"),(float)w,(float)h);
    pUniform1f(pGetUniformLocation(program,"phase"),0);
    pBindVertexArray(vao); glDrawArrays(GL_TRIANGLES,0,3);
    if(verify) {
        const unsigned char want[4][4]={{255,0,0,255},{0,255,0,255},{0,0,255,255},{255,255,0,255}};
        for(int i=0;i<4;i++) {
            unsigned char pixel[4]={0}; char name[100];
            glReadPixels((i&1)?3*w/4:w/4,(i&2)?3*h/4:h/4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
            unsigned char expected[4];memcpy(expected,want[i],4);if(wrong)expected[3]=0;
            snprintf(name,sizeof(name),"window %dx%d quadrant %d rgba=%u,%u,%u,%u",w,h,i,pixel[0],pixel[1],pixel[2],pixel[3]);
            check(!memcmp(pixel,expected,4),name);
        }
        check(glGetError()==GL_NO_ERROR,"default framebuffer GL error");
    }
}
static void front_pixel(HWND hwnd) {
    RECT rc; GetClientRect(hwnd,&rc);
    unsigned char pixel[4]={0}, expected[4]={0,0,255,255};
    if(wrong)expected[3]=0;
    glReadBuffer(GL_FRONT);glReadPixels(rc.right/4,3*rc.bottom/4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    check(!memcmp(pixel,expected,4) && glGetError()==GL_NO_ERROR,"front buffer after swap");
    glReadBuffer(GL_BACK);
}
int main(int argc,char **argv) {
    setbuf(stdout,NULL); int hold=0;
    for(int i=1;i<argc;i++){ if(!strcmp(argv[i],"--wrong-expected"))wrong=1; else if(!strcmp(argv[i],"--hold"))hold=1; }
    WNDCLASSA wc={0};wc.style=CS_OWNDC;wc.lpfnWndProc=wndproc;wc.hInstance=GetModuleHandleA(NULL);wc.lpszClassName="MesaWineGLProbe";
    if(!RegisterClassA(&wc))fatal("RegisterClass");
    RECT size={0,0,640,480};AdjustWindowRect(&size,WS_OVERLAPPEDWINDOW,FALSE);
    HWND hwnd=CreateWindowA(wc.lpszClassName,"Mesa Wine GL integration probe",WS_OVERLAPPEDWINDOW|WS_VISIBLE,160,140,size.right-size.left,size.bottom-size.top,NULL,NULL,wc.hInstance,NULL);
    if(!hwnd)fatal("CreateWindow");
    HDC dc=GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd={0};pfd.nSize=sizeof(pfd);pfd.nVersion=1;pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;pfd.iPixelType=PFD_TYPE_RGBA;pfd.cColorBits=32;pfd.cAlphaBits=8;pfd.cDepthBits=24;pfd.cStencilBits=8;
    int pf=ChoosePixelFormat(dc,&pfd);if(!pf||!SetPixelFormat(dc,pf,&pfd))fatal("pixel format");
    HGLRC bootstrap=wglCreateContext(dc);if(!bootstrap||!wglMakeCurrent(dc,bootstrap))fatal("bootstrap context");
    PFNWGLCREATECONTEXTATTRIBSARBPROC create=(void*)wglGetProcAddress("wglCreateContextAttribsARB");if(!create)fatal("WGL context entry point");
    const int attrs[]={WGL_CONTEXT_MAJOR_VERSION_ARB,4,WGL_CONTEXT_MINOR_VERSION_ARB,6,WGL_CONTEXT_PROFILE_MASK_ARB,WGL_CONTEXT_CORE_PROFILE_BIT_ARB,0};
    HGLRC a=create(dc,NULL,attrs);if(!a)fatal("4.6 core context");
    if(!wglMakeCurrent(dc,a))fatal("core make current");
    wglDeleteContext(bootstrap);
    const char *renderer=(const char*)glGetString(GL_RENDERER);
    printf("VERSION %s\nRENDERER %s\nGLSL %s\n",glGetString(GL_VERSION),renderer,glGetString(GL_SHADING_LANGUAGE_VERSION));
    check(renderer && strstr(renderer,"zink") && strstr(renderer,"KOSMICKRISP"),"Zink and KosmicKrisp renderer identity");
#define LOAD(type,name) p##name=(type)wglGetProcAddress("gl"#name);if(!p##name)fatal("gl"#name);
    PROCS(LOAD)
    GLint profile=0;glGetIntegerv(GL_CONTEXT_PROFILE_MASK,&profile);check(profile==GL_CONTEXT_CORE_PROFILE_BIT,"core profile");
    const char *vs="#version 330 core\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2-1,0,1);}";
    const char *fs="#version 330 core\nuniform vec2 size;uniform float phase;out vec4 c;void main(){bool r=gl_FragCoord.x>size.x/2;bool t=gl_FragCoord.y>size.y/2;c=t?(r?vec4(1,1,0,1):vec4(0,0,1,1)):(r?vec4(0,1,0,1):vec4(1,0,0,1));if(phase>0.5)c.rgb=vec3(1)-c.rgb;}";
    GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs),program=pCreateProgram();pAttachShader(program,v);pAttachShader(program,f);pLinkProgram(program);GLint linked;pGetProgramiv(program,GL_LINK_STATUS,&linked);if(!linked)fatal("link");pDeleteShader(v);pDeleteShader(f);
    GLuint vao;pGenVertexArrays(1,&vao);
    draw(hwnd,program,vao,1);check(SwapBuffers(dc),"initial SwapBuffers");front_pixel(hwnd);
    GLuint texture;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
    const unsigned char texels[8]={13,71,149,255,233,17,89,255};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,1,0,GL_RGBA,GL_UNSIGNED_BYTE,texels);
    GLsync fence=pFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);glFlush();
    HGLRC b=create(dc,a,attrs);if(!b||!wglMakeCurrent(dc,b))fatal("shared context");
    GLenum waited=pClientWaitSync(fence,0,5000000000ULL);check(waited==GL_ALREADY_SIGNALED||waited==GL_CONDITION_SATISFIED,"shared GL fence");
    check(glIsTexture(texture),"shared texture object");glBindTexture(GL_TEXTURE_2D,texture);unsigned char got[8]={0};glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,got);
    unsigned char expected_texels[8];memcpy(expected_texels,texels,8);if(wrong)expected_texels[0]^=1;
    check(!memcmp(got,expected_texels,8),"shared texture contents");check(glGetError()==GL_NO_ERROR,"shared context GL error");pDeleteSync(fence);
    if(!wglMakeCurrent(dc,a))fatal("restore context");
    wglDeleteContext(b);glDeleteTextures(1,&texture);
    size=(RECT){0,0,480,320};AdjustWindowRect(&size,WS_OVERLAPPEDWINDOW,FALSE);
    SetWindowPos(hwnd,NULL,0,0,size.right-size.left,size.bottom-size.top,SWP_NOMOVE|SWP_NOZORDER);
    for(int i=0;i<30;i++){messages();Sleep(10);}
    /* Explicit rebind makes the resized surface observable to both WGL and Mesa. */
    wglMakeCurrent(NULL,NULL);if(!wglMakeCurrent(dc,a))fatal("resize rebind");
    draw(hwnd,program,vao,1);check(SwapBuffers(dc),"resized SwapBuffers");front_pixel(hwnd);
    /* Alternate colours so stale front/back resources cannot accidentally pass.
     * Keep viewport, framebuffer, program, and VAO bindings unchanged. */
    for(int frame=0;frame<6;frame++) {
        int inverted=(frame+1)&1; unsigned char pixel[4]={0};
        unsigned char expected[4]={inverted?255:0,inverted?255:0,inverted?0:255,255};
        if(wrong)expected[3]=0;
        pUniform1f(pGetUniformLocation(program,"phase"),(float)inverted);
        glDrawArrays(GL_TRIANGLES,0,3);
        glReadPixels(120,240,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        char name[100];snprintf(name,sizeof(name),"cached-state frame %d rgba=%u,%u,%u,%u",frame,pixel[0],pixel[1],pixel[2],pixel[3]);
        check(!memcmp(pixel,expected,4) && glGetError()==GL_NO_ERROR,name);
        check(SwapBuffers(dc),"cached-state SwapBuffers");
    }
    unsigned swaps=0;for(int i=0;i<60;i++){messages();draw(hwnd,program,vao,0);if(!SwapBuffers(dc))break;swaps++;Sleep(16);}
    check(swaps==60,"60 consecutive GPU presentations");
    printf("CAPTURE_READY top-left=blue top-right=yellow bottom-left=red bottom-right=green\n");
    if(hold){ULONGLONG until=GetTickCount64()+90000;while(!closed && GetTickCount64()<until){messages();Sleep(20);}}
    pDeleteVertexArrays(1,&vao);pDeleteProgram(program);wglMakeCurrent(NULL,NULL);wglDeleteContext(a);ReleaseDC(hwnd,dc);DestroyWindow(hwnd);
    printf("SUMMARY checks=%u failures=%u\n",checks,failures);return failures?1:0;
}
