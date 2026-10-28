import ctypes,json
v=(ctypes.c_uint8*3)();s=ctypes.CDLL('libSDL2-2.0.so.0');s.SDL_GetVersion(v)
print(json.dumps({'sdl_version':list(v)}))
