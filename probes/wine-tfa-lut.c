/* SPDX-License-Identifier: MIT
 * Reuse the measured WGL integration setup, then execute installed TFA shaders.
 * Assets are read at runtime, never embedded or redistributed with this probe.
 */
static void tfa_checks(void);
#define EXTRA_CHECKS tfa_checks
#define main window_probe_main
#include "wine-gl-window.c"
#undef main
#include <math.h>
#define N 16
#define EXTRA_PROCS(X) \
 X(PFNGLGENBUFFERSPROC,GenBuffers) X(PFNGLBINDBUFFERPROC,BindBuffer) \
 X(PFNGLBUFFERDATAPROC,BufferData) X(PFNGLBINDBUFFERBASEPROC,BindBufferBase) \
 X(PFNGLDELETEBUFFERSPROC,DeleteBuffers) X(PFNGLGETUNIFORMBLOCKINDEXPROC,GetUniformBlockIndex) \
 X(PFNGLUNIFORMBLOCKBINDINGPROC,UniformBlockBinding) X(PFNGLUNIFORM1IPROC,Uniform1i) \
 X(PFNGLACTIVETEXTUREPROC,ActiveTexture) X(PFNGLTEXSTORAGE2DPROC,TexStorage2D) \
 X(PFNGLGENERATEMIPMAPPROC,GenerateMipmap) X(PFNGLGENFRAMEBUFFERSPROC,GenFramebuffers) \
 X(PFNGLBINDFRAMEBUFFERPROC,BindFramebuffer) X(PFNGLFRAMEBUFFERTEXTURE2DPROC,FramebufferTexture2D) \
 X(PFNGLCHECKFRAMEBUFFERSTATUSPROC,CheckFramebufferStatus) X(PFNGLDELETEFRAMEBUFFERSPROC,DeleteFramebuffers) \
 X(PFNGLBINDIMAGETEXTUREPROC,BindImageTexture) X(PFNGLDISPATCHCOMPUTEPROC,DispatchCompute) \
 X(PFNGLMEMORYBARRIERPROC,MemoryBarrier)
EXTRA_PROCS(DECL)
static const char *asset_dir;
static int wrong_lut;
static void *read_asset(const char *name, size_t *size) {
    char path[2048];snprintf(path,sizeof(path),"%s/%s",asset_dir,name);
    FILE *f=fopen(path,"rb");if(!f)fatal(path);
    fseek(f,0,SEEK_END);long length=ftell(f);rewind(f);
    if(length<0)fatal("asset size");
    char *data=calloc((size_t)length+1,1);if(!data)fatal("asset allocation");
    if(fread(data,1,length,f)!=(size_t)length)fatal("asset read");
    fclose(f);if(size)*size=(size_t)length;return data;
}
static GLuint link_stages(const GLuint *stages,int count) {
    GLuint p=pCreateProgram();for(int i=0;i<count;i++)pAttachShader(p,stages[i]);
    pLinkProgram(p);GLint ok;pGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){char log[8192];pGetProgramInfoLog(p,sizeof(log),NULL,log);fprintf(stderr,"%s\n",log);fatal("TFA shader link");}
    for(int i=0;i<count;i++)pDeleteShader(stages[i]);
    return p;
}
static void bind_block(GLuint p,const char *name,int binding,GLuint buffer,const void *data,size_t size) {
    GLuint index=pGetUniformBlockIndex(p,name);if(index==GL_INVALID_INDEX)fatal(name);
    pUniformBlockBinding(p,index,binding);pBindBuffer(GL_UNIFORM_BUFFER,buffer);
    pBufferData(GL_UNIFORM_BUFFER,size,data,GL_STATIC_DRAW);pBindBufferBase(GL_UNIFORM_BUFFER,binding,buffer);
}
static void tfa_checks(void) {
    EXTRA_PROCS(LOAD)
    size_t bytes;unsigned char *lut=read_asset("lut.rgba",&bytes);
    if(bytes!=1024*32*4)fatal("LUT dimensions");
    float *inputs=read_asset("inputs.bin",&bytes);if(bytes!=N*4*sizeof(float))fatal("input size");
    float *expected=read_asset("expected.bin",&bytes);if(bytes!=2*N*4*sizeof(float))fatal("reference size");
    char *fs=read_asset("final.glsl",NULL),*utility=read_asset("utility.glsl",NULL);
    const char *vs="#version 430\nout vec2 oUv0;void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);oUv0=p;gl_Position=vec4(p*2-1,0,1);}";
    GLuint stages[]={shader(GL_VERTEX_SHADER,vs),shader(GL_FRAGMENT_SHADER,fs),shader(GL_FRAGMENT_SHADER,utility)};
    GLuint program=link_stages(stages,3);
    check(1,"TFA installed final shader and attached utility compile/link unchanged");
    free(fs);free(utility);
    const char *cs="#version 430\nlayout(local_size_x=16) in;layout(std430,binding=0) readonly buffer Inputs {vec4 values[];};layout(rgba32f,binding=0) writeonly uniform image2D scene;uniform int shift;void main(){uint i=gl_GlobalInvocationID.x;imageStore(scene,ivec2(i,0),values[(i+uint(shift))%16u]);}";
    GLuint compute_stage=shader(GL_COMPUTE_SHADER,cs),compute=link_stages(&compute_stage,1);
    GLuint buffers[3];pGenBuffers(3,buffers);
    pBindBuffer(GL_SHADER_STORAGE_BUFFER,buffers[0]);pBufferData(GL_SHADER_STORAGE_BUFFER,N*4*sizeof(float),inputs,GL_STATIC_DRAW);pBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,buffers[0]);
    const float compositor[16]={0,-1,0,0, .1f,.1f,.1f,1, 1,1,.1f,1, 0,0,0,0};
    const float curve[8]={.15f,.50f,.10f,.20f,.02f,.30f,0,0};
    bind_block(program,"Plankton_CompositorParams",0,buffers[1],compositor,sizeof(compositor));
    bind_block(program,"Hidden_Plankton_HDR_Params",1,buffers[2],curve,sizeof(curve));
    GLuint tex[4];glGenTextures(4,tex);
    pActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,tex[0]);pTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA32F,N,1);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    float zero[4]={0};glBindTexture(GL_TEXTURE_2D,tex[1]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,zero);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D,tex[2]);pTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA32F,N,1);
    GLuint fbo;pGenFramebuffers(1,&fbo);pBindFramebuffer(GL_FRAMEBUFFER,fbo);pFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex[2],0);
    if(pCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)fatal("float framebuffer");
    GLuint vao;pGenVertexArrays(1,&vao);pBindVertexArray(vao);glViewport(0,0,N,1);glDisable(GL_FRAMEBUFFER_SRGB);
    pUseProgram(program);
    const char *samplers[]={"inRTT","smallBloomSampler","mediumBloomSampler","largeBloomSampler","lutSampler","lightShaftsSampler"};
    const int units[]={0,2,3,4,5,6};
    for(int i=0;i<6;i++) {GLint loc=pGetUniformLocation(program,samplers[i]);if(loc<0)fatal(samplers[i]);pUniform1i(loc,units[i]);}
    for(int unit=0;unit<=6;unit++){pActiveTexture(GL_TEXTURE0+unit);glBindTexture(GL_TEXTURE_2D,unit==0?tex[0]:tex[1]);}
    for(int variant=0;variant<3;variant++) {
        int inverted=variant==2,levels=variant?11:1;
        unsigned char upload[1024*32*4],readback[sizeof(upload)];memcpy(upload,lut,sizeof(upload));
        if(inverted)for(size_t i=0;i<sizeof(upload);i++)if(i%4!=3)upload[i]=255-upload[i];
        glDeleteTextures(1,&tex[3]);glGenTextures(1,&tex[3]);pActiveTexture(GL_TEXTURE5);glBindTexture(GL_TEXTURE_2D,tex[3]);
        pTexStorage2D(GL_TEXTURE_2D,levels,GL_RGBA8,1024,32);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1024,32,GL_RGBA,GL_UNSIGNED_BYTE,upload);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,levels>1?GL_LINEAR_MIPMAP_NEAREST:GL_LINEAR);
        if(levels>1)pGenerateMipmap(GL_TEXTURE_2D);
        glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,readback);
        char name[160];snprintf(name,sizeof(name),"TFA LUT variant %d exact base-level upload/readback",variant);
        check(!memcmp(upload,readback,sizeof(upload))&&glGetError()==GL_NO_ERROR,name);
        for(int update=0;update<2;update++) {
            int shift=update*7;
            pUseProgram(compute);pUniform1i(pGetUniformLocation(compute,"shift"),shift);
            pBindImageTexture(0,tex[0],0,GL_FALSE,0,GL_WRITE_ONLY,GL_RGBA32F);pDispatchCompute(1,1,1);
            pMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
            pUseProgram(program);glDrawArrays(GL_TRIANGLES,0,3);
            float pixels[N*4];glReadPixels(0,0,N,1,GL_RGBA,GL_FLOAT,pixels);
            for(int i=0;i<N;i++) {
                float error=0;for(int c=0;c<4;c++){float want=expected[(inverted*N+(i+shift)%N)*4+c];if(wrong_lut&&c<3)want+=.125f;float delta=fabsf(pixels[i*4+c]-want);if(!isfinite(delta))error=INFINITY;else if(delta>error)error=delta;}
                snprintf(name,sizeof(name),"TFA LUT variant %d update %d sample %d max_error=%.7g rgba=%.5g,%.5g,%.5g,%.5g",variant,update,i,error,pixels[i*4],pixels[i*4+1],pixels[i*4+2],pixels[i*4+3]);
                check(error<.002f,name);
            }
            check(glGetError()==GL_NO_ERROR,"TFA compute-to-tone-map GL error");
        }
    }
    pBindFramebuffer(GL_FRAMEBUFFER,0);pDeleteFramebuffers(1,&fbo);glDeleteTextures(4,tex);pDeleteBuffers(3,buffers);pDeleteProgram(program);pDeleteProgram(compute);pDeleteVertexArrays(1,&vao);free(lut);free(inputs);free(expected);
}
int main(int argc,char **argv) {
    if(argc<2){fprintf(stderr,"usage: wine-tfa-lut.exe ASSET_DIR [--wrong-lut]\n");return 2;}
    asset_dir=argv[1];wrong_lut=argc>2&&!strcmp(argv[2],"--wrong-lut");
    return window_probe_main(1,argv);
}
