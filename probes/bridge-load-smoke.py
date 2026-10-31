import ctypes as c, os, sys
p=sys.argv[1]
os.environ['VK_DRIVER_FILES']=p+'/kosmickrisp.json'
os.environ['MESA_KK_EXPERIMENTAL']='xfb_gs_probe,custom_border,rgb32_texel'
lib=c.CDLL(p+'/lib/libmesa_wine.dylib')
assert lib.mesa_wine_get_abi_version()==1
lib.mesa_wine_init.restype=c.c_void_p
screen=lib.mesa_wine_init();assert screen
lib.mesa_wine_create_context.argtypes=[c.c_void_p,c.c_int,c.c_int,c.c_int,c.c_int,c.c_uint,c.c_void_p]
lib.mesa_wine_create_context.restype=c.c_void_p
ctx=lib.mesa_wine_create_context(screen,1,4,6,1,0,None);assert ctx
lib.mesa_wine_create_drawable.argtypes=[c.c_void_p,c.c_int,c.c_int,c.c_int];lib.mesa_wine_create_drawable.restype=c.c_void_p
draw=lib.mesa_wine_create_drawable(screen,1,64,64);assert draw
lib.mesa_wine_make_current.argtypes=[c.c_void_p,c.c_void_p,c.c_void_p];lib.mesa_wine_make_current.restype=c.c_bool
assert lib.mesa_wine_make_current(ctx,draw,draw)
lib.mesa_wine_get_gl_version.restype=c.c_char_p
print('VERSION',lib.mesa_wine_get_gl_version().decode(),flush=True)
lib.mesa_wine_get_proc_address.argtypes=[c.c_char_p];lib.mesa_wine_get_proc_address.restype=c.c_void_p
def fn(name,ret,*args):return c.CFUNCTYPE(ret,*args)(lib.mesa_wine_get_proc_address(name.encode()))
fn('glClearColor',None,*([c.c_float]*4))(1,0,0,1)
fn('glClear',None,c.c_uint)(0x4000)
pixel=(c.c_ubyte*4)()
fn('glReadPixels',None,c.c_int,c.c_int,c.c_int,c.c_int,c.c_uint,c.c_uint,c.c_void_p)(10,10,1,1,0x1908,0x1401,pixel)
print('PIXEL',list(pixel),flush=True);assert list(pixel)==[255,0,0,255]
assert fn('glGetError',c.c_uint)()==0
assert lib.mesa_wine_make_current(None,None,None)
for name,handle in [('destroy_context',ctx),('destroy_drawable',draw),('fini',screen)]:
 f=getattr(lib,'mesa_wine_'+name);f.argtypes=[c.c_void_p];f(handle)
print('BUNDLE_LOAD_PASS',flush=True)
