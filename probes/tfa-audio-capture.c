/* SPDX-License-Identifier: MIT
 * Diagnostic-only IAT hooks in our TFA process. Forward every OpenAL call
 * unchanged; capture bounded PCM supplied by the game's own decoder.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
static void (*real_buffer)(unsigned,int,const void*,int,int);
static void (*real_queue)(unsigned,int,const unsigned*);
static void (*real_play)(unsigned);
static CRITICAL_SECTION lock;
static FILE *logfile,*pcm;
static size_t total;
static void hook_buffer(unsigned buffer,int format,const void *data,int size,int rate) {
    EnterCriticalSection(&lock);
    if(logfile) {
        long offset=-1;
        if(pcm && data && size>0 && total+(size_t)size<=64*1024*1024) {
            offset=(long)total;
            if(fwrite(data,1,size,pcm)==(size_t)size)total+=size;
            else offset=-1;
        }
        fprintf(logfile,"BUFFER tick=%llu thread=%lu id=%u format=%x size=%d rate=%d offset=%ld\n",GetTickCount64(),GetCurrentThreadId(),buffer,format,size,rate,offset);
        fflush(logfile);if(pcm)fflush(pcm);
    }
    LeaveCriticalSection(&lock);
    real_buffer(buffer,format,data,size,rate);
}
static void hook_queue(unsigned source,int count,const unsigned *buffers) {
    EnterCriticalSection(&lock);
    if(logfile){fprintf(logfile,"QUEUE tick=%llu source=%u count=%d",GetTickCount64(),source,count);for(int i=0;i<count;i++)fprintf(logfile," %u",buffers[i]);fprintf(logfile,"\n");fflush(logfile);}
    LeaveCriticalSection(&lock);real_queue(source,count,buffers);
}
static void hook_play(unsigned source) {
    EnterCriticalSection(&lock);
    if(logfile){fprintf(logfile,"PLAY tick=%llu source=%u\n",GetTickCount64(),source);fflush(logfile);}
    LeaveCriticalSection(&lock);real_play(source);
}
static int patch_module(HMODULE module) {
    unsigned char *base=(void*)module;IMAGE_DOS_HEADER *dos=(void*)base;
    IMAGE_NT_HEADERS64 *nt=(void*)(base+dos->e_lfanew);
    DWORD rva=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if(!rva)return 0;
    int patched=0;IMAGE_IMPORT_DESCRIPTOR *desc=(void*)(base+rva);
    for(;desc->Name;desc++) {
        if(_stricmp((char*)base+desc->Name,"OpenAL32.dll"))continue;
        if(!desc->OriginalFirstThunk)continue;
        IMAGE_THUNK_DATA64 *names=(void*)(base+desc->OriginalFirstThunk),*iat=(void*)(base+desc->FirstThunk);
        for(;names->u1.AddressOfData;names++,iat++) {
            if(IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal))continue;
            IMAGE_IMPORT_BY_NAME *name=(void*)(base+names->u1.AddressOfData);void *replacement=NULL;
            if(!strcmp((char*)name->Name,"alBufferData"))replacement=hook_buffer;
            if(!strcmp((char*)name->Name,"alSourceQueueBuffers"))replacement=hook_queue;
            if(!strcmp((char*)name->Name,"alSourcePlay"))replacement=hook_play;
            if(!replacement)continue;
            DWORD old; if(!VirtualProtect(&iat->u1.Function,sizeof(void*),PAGE_READWRITE,&old))continue;
            InterlockedExchangePointer((void*volatile*)&iat->u1.Function,replacement);
            VirtualProtect(&iat->u1.Function,sizeof(void*),old,&old);patched++;
        }
    }
    return patched;
}
static DWORD WINAPI setup(void *unused) {
    (void)unused;char dir[2048],path[2200];
    if(!GetEnvironmentVariableA("TFA_AUDIO_CAPTURE_DIR",dir,sizeof(dir)))return 1;
    InitializeCriticalSection(&lock);
    snprintf(path,sizeof(path),"%s/calls.txt",dir);logfile=fopen(path,"w");
    snprintf(path,sizeof(path),"%s/uploads.pcm",dir);pcm=fopen(path,"wb");
    if(!logfile || !pcm)return 2;
    const char *targets[]={"OgreOggSound.dll","Plankton.dll","CarriersPlugin.dll"};int done[3]={0};
    for(int attempt=0;attempt<200;attempt++) {
        HMODULE al=GetModuleHandleA("OpenAL32.dll");
        if(al && !real_buffer){real_buffer=(void*)GetProcAddress(al,"alBufferData");real_queue=(void*)GetProcAddress(al,"alSourceQueueBuffers");real_play=(void*)GetProcAddress(al,"alSourcePlay");}
        if(real_buffer&&real_queue&&real_play)for(int i=0;i<3;i++)if(!done[i]){
            HMODULE m=GetModuleHandleA(targets[i]);if(m){int n=patch_module(m);EnterCriticalSection(&lock);fprintf(logfile,"HOOK module=%s count=%d\n",targets[i],n);fflush(logfile);LeaveCriticalSection(&lock);done[i]=1;}
        }
        if(done[0]&&done[1]&&done[2])return 0;
        Sleep(50);
    }
    return 3;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,void *reserved) {
    (void)reserved;
    if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(instance);HANDLE thread=CreateThread(NULL,0,setup,NULL,0,NULL);if(thread)CloseHandle(thread);}
    return TRUE;
}
