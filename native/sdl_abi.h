#pragma once
// Minimal SDL2 public ABI declarations for Linux builds without development headers.
// SDL2 definitions verified against https://github.com/libsdl-org/SDL/tree/SDL2/include .
// Prefer vendor headers on normal developer machines. Not an SDL implementation.
#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#elif __has_include(<SDL.h>)
#include <SDL.h>
#else
#include <cstdint>
using Uint8=std::uint8_t; using Uint16=std::uint16_t; using Uint32=std::uint32_t; using Sint32=std::int32_t;
struct SDL_Window; struct SDL_Surface;
struct SDL_Rect {int x,y,w,h;};
struct SDL_Keysym {Sint32 scancode,sym; Uint16 mod; Uint32 unused;};
struct SDL_KeyboardEvent {Uint32 type,timestamp,windowID; Uint8 state,repeat,padding2,padding3; SDL_Keysym keysym;};
struct SDL_TextEditingEvent {Uint32 type,timestamp,windowID; char text[32]; Sint32 start,length;};
struct SDL_TextInputEvent {Uint32 type,timestamp,windowID; char text[32];};
struct SDL_WindowEvent {Uint32 type,timestamp,windowID; Uint8 event,padding1,padding2,padding3; Sint32 data1,data2;};
struct SDL_MouseButtonEvent {Uint32 type,timestamp,windowID,which; Uint8 button,state,clicks,padding1; Sint32 x,y;};
struct SDL_MouseMotionEvent {Uint32 type,timestamp,windowID,which,state; Sint32 x,y,xrel,yrel;};
struct SDL_MouseWheelEvent {Uint32 type,timestamp,windowID,which; Sint32 x,y; Uint32 direction; float preciseX,preciseY; Sint32 mouseX,mouseY;};
union alignas(8) SDL_Event {Uint32 type; SDL_KeyboardEvent key; SDL_TextInputEvent text; SDL_TextEditingEvent edit; SDL_WindowEvent window; SDL_MouseButtonEvent button; SDL_MouseMotionEvent motion; SDL_MouseWheelEvent wheel; Uint8 padding[56];};
static_assert(sizeof(SDL_Event)==56 && sizeof(SDL_Keysym)==16,"Unsupported SDL2 ABI");
constexpr Uint32 SDL_INIT_VIDEO=0x20, SDL_WINDOW_SHOWN=4, SDL_WINDOW_RESIZABLE=0x20;
constexpr int SDL_WINDOWPOS_CENTERED=0x2fff0000;
constexpr Uint32 SDL_QUIT=0x100,SDL_WINDOWEVENT=0x200,SDL_KEYDOWN=0x300,SDL_TEXTEDITING=0x302,SDL_TEXTINPUT=0x303,SDL_MOUSEMOTION=0x400,SDL_MOUSEBUTTONDOWN=0x401,SDL_MOUSEBUTTONUP=0x402,SDL_MOUSEWHEEL=0x403;
constexpr Uint8 SDL_WINDOWEVENT_EXPOSED=3,SDL_WINDOWEVENT_RESIZED=5,SDL_WINDOWEVENT_SIZE_CHANGED=6,SDL_WINDOWEVENT_FOCUS_LOST=13,SDL_WINDOWEVENT_CLOSE=14,SDL_BUTTON_LEFT=1;
constexpr Sint32 SDLK_BACKSPACE=8,SDLK_TAB=9,SDLK_RETURN=13,SDLK_ESCAPE=27,SDLK_DELETE=127,SDLK_HOME=1073741898,SDLK_END=1073741901,SDLK_RIGHT=1073741903,SDLK_LEFT=1073741904;
constexpr Uint16 KMOD_CTRL=0x00c0,KMOD_GUI=0x0c00;
extern "C" {
int SDL_SetHint(const char*,const char*); const char* SDL_GetHint(const char*);
int SDL_Init(Uint32); void SDL_Quit(); const char* SDL_GetError(); void SDL_ClearError(); const char* SDL_GetCurrentVideoDriver();
SDL_Window* SDL_CreateWindow(const char*,int,int,int,int,Uint32); void SDL_DestroyWindow(SDL_Window*);
void SDL_SetWindowMinimumSize(SDL_Window*,int,int); void SDL_SetWindowMaximumSize(SDL_Window*,int,int);
void SDL_SetWindowTitle(SDL_Window*,const char*);
const char* SDL_GetWindowTitle(SDL_Window*); void SDL_GetWindowPosition(SDL_Window*,int*,int*); void SDL_GetWindowSize(SDL_Window*,int*,int*); void SDL_SetWindowSize(SDL_Window*,int,int); Uint32 SDL_GetWindowID(SDL_Window*);
SDL_Surface* SDL_GetWindowSurface(SDL_Window*); int SDL_UpdateWindowSurface(SDL_Window*);
void SDL_GetClipRect(SDL_Surface*,SDL_Rect*);int SDL_UpdateWindowSurfaceRects(SDL_Window*,const SDL_Rect*,int);
SDL_Surface* SDL_CreateRGBSurfaceFrom(void*,int,int,int,int,Uint32,Uint32,Uint32,Uint32);
void SDL_FreeSurface(SDL_Surface*); int SDL_UpperBlitScaled(SDL_Surface*,const SDL_Rect*,SDL_Surface*,SDL_Rect*);
Uint32 SDL_RegisterEvents(int); void SDL_FlushEvent(Uint32);
int SDL_WaitEventTimeout(SDL_Event*,int); int SDL_WaitEvent(SDL_Event*); int SDL_PollEvent(SDL_Event*); int SDL_PushEvent(SDL_Event*);
void SDL_StartTextInput(); void SDL_StopTextInput(); void SDL_SetTextInputRect(const SDL_Rect*);
char* SDL_GetClipboardText(); int SDL_SetClipboardText(const char*); void SDL_free(void*);
int SDL_GetModState();
Uint32 SDL_GetMouseState(int*,int*); void SDL_WarpMouseInWindow(SDL_Window*,int,int);
}
#endif
