#include <cstdint>
#include <cstdlib>
#include <iostream>
struct Display;struct Visual;struct XImage;struct _XGC;using GC=_XGC*;using Window=unsigned long;
union XEvent {int type;long pad[24];};
extern "C" {
Display* XOpenDisplay(const char*);int XCloseDisplay(Display*);int XDefaultScreen(Display*);Visual* XDefaultVisual(Display*,int);int XDefaultDepth(Display*,int);Window XRootWindow(Display*,int);
Window XCreateSimpleWindow(Display*,Window,int,int,unsigned,unsigned,unsigned,unsigned long,unsigned long);int XDestroyWindow(Display*,Window);int XMapWindow(Display*,Window);int XStoreName(Display*,Window,const char*);int XSelectInput(Display*,Window,long);int XNextEvent(Display*,XEvent*);int XSync(Display*,int);
GC XCreateGC(Display*,Window,unsigned long,void*);int XFreeGC(Display*,GC);
XImage* XCreateImage(Display*,Visual*,unsigned,int,int,char*,unsigned,unsigned,int,int);int XDestroyImage(XImage*);
int XPutImage(Display*,Window,GC,XImage*,int,int,int,int,unsigned,unsigned);
}
int main(){
 auto*d=XOpenDisplay(nullptr);if(!d){std::cerr<<"no display\n";return 1;}
 const int screen=XDefaultScreen(d),depth=XDefaultDepth(d,screen);const unsigned width=1120,height=840;
 auto w=XCreateSimpleWindow(d,XRootWindow(d,screen),50,50,width,height,0,0,0);
 XStoreName(d,w,"Direct XPutImage probe / any key exits");XSelectInput(d,w,(1L<<15)|(1L<<17)|1L);XMapWindow(d,w);
 auto*data=static_cast<std::uint32_t*>(std::malloc(width*height*4));if(!data)return 1;
 for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)data[y*width+x]=0xff000000|((x*255/width)<<16)|((y*255/height)<<8)|0x55;
 auto*image=XCreateImage(d,XDefaultVisual(d,screen),depth,2,0,reinterpret_cast<char*>(data),width,height,32,width*4);
 if(!image){std::cerr<<"image failed\n";std::free(data);return 1;}
 auto gc=XCreateGC(d,w,0,nullptr);int frames=0;XEvent e{};
 auto present=[&]{XPutImage(d,w,gc,image,0,0,0,0,width,height);XSync(d,0);std::cerr<<"XPutImage passed depth="<<depth<<" frame="<<++frames<<'\n';};
 present();while(true){XNextEvent(d,&e);if(e.type==2)break;if(e.type==12||e.type==22)present();}
 XDestroyImage(image);XFreeGC(d,gc);XDestroyWindow(d,w);XCloseDisplay(d);std::cerr<<"clean exit\n";return 0;
}
