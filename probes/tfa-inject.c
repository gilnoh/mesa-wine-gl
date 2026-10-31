/* SPDX-License-Identifier: MIT
 * Launch TFA and inject our diagnostic DLL only into that child. No game files
 * are replaced. Keep the parent alive and report the child's actual exit.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdint.h>
#include <stdio.h>
static uintptr_t remote_base(DWORD pid,const char *name) {
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);
    if(snap==INVALID_HANDLE_VALUE)return 0;
    MODULEENTRY32 me={.dwSize=sizeof(me)};uintptr_t base=0;
    if(Module32First(snap,&me))do{if(!_stricmp(me.szModule,name)){base=(uintptr_t)me.modBaseAddr;break;}}while(Module32Next(snap,&me));
    CloseHandle(snap);return base;
}
int main(int argc,char **argv) {
    setbuf(stdout,NULL);if(argc!=2){fprintf(stderr,"usage: tfa-inject.exe DIAGNOSTIC_DLL\n");return 2;}
    STARTUPINFOA si={.cb=sizeof(si)};PROCESS_INFORMATION pi;char cmd[]="TFA.exe";
    if(!CreateProcessA(NULL,cmd,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi)){printf("CreateProcess failed %lu\n",GetLastError());return 2;}
    printf("TFA Windows PID %lu\n",pi.dwProcessId);
    FARPROC load=GetProcAddress(GetModuleHandleA("kernel32.dll"),"LoadLibraryA");HMODULE owner;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(char*)load,&owner);
    char path[MAX_PATH];GetModuleFileNameA(owner,path,sizeof(path));char *name=strrchr(path,'\\');name=name?name+1:path;
    uintptr_t base=0;for(int i=0;i<200&&!base;i++){base=remote_base(pi.dwProcessId,name);if(!base)Sleep(10);}
    void *remote=VirtualAllocEx(pi.hProcess,NULL,strlen(argv[1])+1,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!base||!remote||!WriteProcessMemory(pi.hProcess,remote,argv[1],strlen(argv[1])+1,NULL)){fprintf(stderr,"Injection preparation failed %lu; child remains running\n",GetLastError());}
    else {
        HANDLE thread=CreateRemoteThread(pi.hProcess,NULL,0,(LPTHREAD_START_ROUTINE)(base+(uintptr_t)load-(uintptr_t)owner),remote,0,NULL);
        if(thread){DWORD wait=WaitForSingleObject(thread,15000);DWORD result=0;if(wait==WAIT_OBJECT_0)GetExitCodeThread(thread,&result);printf("LoadLibrary wait=%lu result=%lx\n",wait,result);CloseHandle(thread);}
        else fprintf(stderr,"CreateRemoteThread failed %lu\n",GetLastError());
    }
    WaitForSingleObject(pi.hProcess,INFINITE);DWORD code;GetExitCodeProcess(pi.hProcess,&code);printf("TFA exit %lu\n",code);
    CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return (int)code;
}
