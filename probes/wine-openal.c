/* SPDX-License-Identifier: MIT
 * Exercise the game's OpenAL DLL in isolation: identical music as float and
 * signed 16-bit, either offline loopback or real-device playback.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static void *(*alcOpenDevice)(const char*);
static void *(*alcCreateContext)(void*,const int*);
static char (*alcMakeContextCurrent)(void*);
static void (*alcDestroyContext)(void*);
static char (*alcCloseDevice)(void*);
static void *(*alcGetProcAddress)(void*,const char*);
static void (*alGenBuffers)(int,unsigned*);
static void (*alBufferData)(unsigned,int,const void*,int,int);
static void (*alGenSources)(int,unsigned*);
static void (*alSourcei)(unsigned,int,int);
static void (*alSourcePlay)(unsigned);
static void (*alSourceStop)(unsigned);
static void (*alDeleteSources)(int,const unsigned*);
static void (*alDeleteBuffers)(int,const unsigned*);
static int (*alGetError)(void);
static const char *(*alGetString)(int);
static void die(const char *s){fprintf(stderr,"FATAL %s Windows=%lu\n",s,GetLastError());exit(2);}
int main(int argc,char **argv) {
 setbuf(stdout,NULL);if(argc!=5){fprintf(stderr,"usage: wine-openal.exe OPENAL_DLL PCM_F32 OUTPUT_PREFIX loopback|play\n");return 2;}
 HMODULE lib=LoadLibraryExA(argv[1],NULL,LOAD_WITH_ALTERED_SEARCH_PATH);if(!lib)die("OpenAL DLL");
#define LOAD(name) name=(void*)GetProcAddress(lib,#name);if(!name)die(#name)
 LOAD(alcOpenDevice);LOAD(alcCreateContext);LOAD(alcMakeContextCurrent);LOAD(alcDestroyContext);LOAD(alcCloseDevice);LOAD(alcGetProcAddress);
 LOAD(alGenBuffers);LOAD(alBufferData);LOAD(alGenSources);LOAD(alSourcei);LOAD(alSourcePlay);LOAD(alSourceStop);LOAD(alDeleteSources);LOAD(alDeleteBuffers);LOAD(alGetError);LOAD(alGetString);
 FILE *f=fopen(argv[2],"rb");if(!f)die("input");fseek(f,0,SEEK_END);long bytes=ftell(f);rewind(f);if(bytes<=0||bytes%8)die("input dimensions");
 float *samples=malloc(bytes);if(!samples||fread(samples,1,bytes,f)!=(size_t)bytes)die("read");fclose(f);int frames=bytes/8;
 short *integer=malloc(bytes/2);for(int i=0;i<bytes/4;i++){float v=samples[i];integer[i]=v>=1?32767:v<=-1?-32768:(short)lrintf(v*32767);}
 int offline=!strcmp(argv[4],"loopback");
 int rate=getenv("TFA_AUDIO_RATE")?atoi(getenv("TFA_AUDIO_RATE")):48000;
 int output_frames=(int)(frames*(long long)rate/48000)+2048;
 void *(*loopback)(const char*)=alcGetProcAddress(NULL,"alcLoopbackOpenDeviceSOFT");
 void (*render)(void*,void*,int)=alcGetProcAddress(NULL,"alcRenderSamplesSOFT");
 if(offline&&(!loopback||!render))die("loopback extension");
 unsigned failures=0;
 for(int mode=0;mode<2;mode++) {
  printf("BEGIN %s %s\n",mode?"s16":"float",offline?"loopback":"playback");
  void *dev=offline?loopback(NULL):alcOpenDevice(NULL);if(!dev)die("device");
  int attr[]={0x1990,0x1501,0x1991,0x1406,0x1007,rate,0};
  void *ctx=alcCreateContext(dev,offline?attr:NULL);if(!ctx||!alcMakeContextCurrent(ctx))die("context");
  printf("OpenAL %s / %s\n",alGetString(0xb002),alGetString(0xb003));
  unsigned buf,source;alGenBuffers(1,&buf);alBufferData(buf,mode?0x1103:0x10011,mode?(void*)integer:samples,mode?bytes/2:bytes,48000);
  alGenSources(1,&source);alSourcei(source,0x1009,buf);alSourcei(source,0x1033,1);alSourcePlay(source);
  if(offline) {
   float *output=calloc(output_frames*2,sizeof(float));render(dev,output,output_frames);
   char path[2048];snprintf(path,sizeof(path),"%s-%s.f32",argv[3],mode?"s16":"float");f=fopen(path,"wb");if(!f)die("output");fwrite(output,sizeof(float),output_frames*2,f);fclose(f);free(output);
  } else Sleep((DWORD)(frames*1000LL/48000+500));
  int error=alGetError();printf("END %s AL_ERROR=%x\n",mode?"s16":"float",error);failures+=error!=0;
  alSourceStop(source);alDeleteSources(1,&source);alDeleteBuffers(1,&buf);alcMakeContextCurrent(NULL);alcDestroyContext(ctx);alcCloseDevice(dev);
  if(!offline)Sleep(1000);
 }
 free(integer);free(samples);printf("SUMMARY failures=%u\n",failures);return failures?1:0;
}
