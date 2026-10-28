#include "sdl_abi.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
extern "C" {
void SDL_GetClipRect(SDL_Surface*,SDL_Rect*);
int SDL_UpdateWindowSurfaceRects(SDL_Window*,const SDL_Rect*,int);
int SDL_FillRect(SDL_Surface*,const SDL_Rect*,Uint32);
void SDL_GetVersion(void*);
}
int main(int argc,char**argv){
 SDL_SetHint("SDL_FRAMEBUFFER_ACCELERATION","0");
 unsigned char v[3]{};SDL_GetVersion(v);std::cerr<<"SDL="<<int(v[0])<<'.'<<int(v[1])<<'.'<<int(v[2])<<"\n";
 if(SDL_Init(SDL_INIT_VIDEO)<0){std::cerr<<SDL_GetError()<<'\n';return 1;}
 const bool limits=argc>1&&std::strcmp(argv[1],"limits")==0;
 auto* w=SDL_CreateWindow(limits?"Surface probe / limits":"Surface probe / no limits",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1120,840,SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
 if(!w){std::cerr<<SDL_GetError()<<'\n';return 1;}
 if(limits){SDL_SetWindowMinimumSize(w,320,320);SDL_SetWindowMaximumSize(w,4096,4096);}
 auto present=[&]{
  int width=0,height=0;SDL_GetWindowSize(w,&width,&height);
  SDL_Surface* dst=SDL_GetWindowSurface(w);if(!dst){std::cerr<<SDL_GetError()<<'\n';std::exit(1);}
  SDL_Rect clip{};SDL_GetClipRect(dst,&clip);
  std::cerr<<"driver="<<SDL_GetCurrentVideoDriver()<<" limits="<<limits<<" window="<<width<<'x'<<height<<" clip="<<clip.x<<','<<clip.y<<','<<clip.w<<','<<clip.h<<"\n";
  if(SDL_FillRect(dst,nullptr,0xff335588)<0){std::cerr<<SDL_GetError()<<'\n';std::exit(1);}
  if(SDL_UpdateWindowSurfaceRects(w,&clip,1)<0){std::cerr<<SDL_GetError()<<'\n';std::exit(1);}
  std::cerr<<"presented\n";
 };
 present();SDL_Event event{};
 while(SDL_WaitEvent(&event)){
  if(event.type==SDL_QUIT)break;
  if(event.type==SDL_WINDOWEVENT){std::cerr<<"event="<<int(event.window.event)<<" dimensions="<<event.window.data1<<'x'<<event.window.data2<<"\n";
   if(event.window.event==SDL_WINDOWEVENT_CLOSE)break;
   if(event.window.event==SDL_WINDOWEVENT_EXPOSED||event.window.event==SDL_WINDOWEVENT_RESIZED||event.window.event==SDL_WINDOWEVENT_SIZE_CHANGED)present();
  }
 }
 SDL_DestroyWindow(w);SDL_Quit();return 0;
}
