/* SPDX-License-Identifier: MIT
 * Native presentation preflight, independent of Wine's WGL integration.
 */
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include "mesa_wine_api.h"
#include <stdio.h>
#include <string.h>

int main(void) { @autoreleasepool {
    setbuf(stdout,NULL);
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    NSWindow *window=[[NSWindow alloc] initWithContentRect:NSMakeRect(160,160,320,240)
        styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
    [window setTitle:@"Mesa bridge native presentation probe"];
    CAMetalLayer *layer=[CAMetalLayer layer];layer.device=MTLCreateSystemDefaultDevice();
    layer.framebufferOnly=NO;layer.drawableSize=CGSizeMake(320,240);
    window.contentView.wantsLayer=YES;window.contentView.layer=layer;
    [window makeKeyAndOrderFront:nil];[NSApp finishLaunching];[NSApp activateIgnoringOtherApps:YES];
    mesa_wine_screen_t screen=mesa_wine_init();if(!screen)return 2;
    mesa_wine_ctx_t ctx=mesa_wine_create_context(screen,1,4,6,MESA_WINE_PROFILE_CORE,0,NULL);
    mesa_wine_draw_t draw=mesa_wine_create_drawable(screen,1,320,240);
    if(!ctx||!draw||!mesa_wine_attach_presentation(screen,draw,layer)||!mesa_wine_make_current(ctx,draw,draw))return 3;
    void (*viewport)(int,int,int,int)=mesa_wine_get_proc_address("glViewport");
    void (*clearcolor)(float,float,float,float)=mesa_wine_get_proc_address("glClearColor");
    void (*clear)(unsigned)=mesa_wine_get_proc_address("glClear");
    void (*enable)(unsigned)=mesa_wine_get_proc_address("glEnable");
    void (*scissor)(int,int,int,int)=mesa_wine_get_proc_address("glScissor");
    unsigned (*error)(void)=mesa_wine_get_proc_address("glGetError");
    const float colors[4][3]={{1,0,0},{0,1,0},{0,0,1},{1,1,0}};
    for(int frame=0;frame<4;frame++) {
        viewport(0,0,320,240);enable(0x0c11);
        for(int q=0;q<4;q++){scissor((q&1)*160,(q>>1)*120,160,120);clearcolor(colors[q][0],colors[q][1],colors[q][2],1);clear(0x4000);}
        if(error() || !mesa_wine_swap_buffers(draw))return 4;
        printf("NATIVE_PRESENT %d\n",frame);
    }
    printf("CAPTURE_READY\n");
    NSDate *end=[NSDate dateWithTimeIntervalSinceNow:15];
    while([end timeIntervalSinceNow]>0) {
        NSEvent *event=[NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate dateWithTimeIntervalSinceNow:.1] inMode:NSDefaultRunLoopMode dequeue:YES];
        if(event)[NSApp sendEvent:event];[NSApp updateWindows];
    }
    mesa_wine_make_current(NULL,NULL,NULL);mesa_wine_destroy_context(ctx);mesa_wine_destroy_drawable(draw);mesa_wine_fini(screen);
    [window orderOut:nil];printf("NATIVE_WINDOW_PASS\n");return 0;
} }
