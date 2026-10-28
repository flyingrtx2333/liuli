#pragma once
#include <algorithm>
#include <string>
namespace explicit_ui {
// Cairo's toy text API requires valid UTF-8. Bound input work and replace invalid
// bytes so one malformed model string cannot poison the entire Cairo context.
inline std::string display_text(const std::string& s) {
    std::string out; const auto limit=std::min<std::size_t>(s.size(),512);
    for(std::size_t i=0;i<limit;) {
        const auto c=static_cast<unsigned char>(s[i]);
        if(c<128) {out+=(c<32||c==127)?' ':static_cast<char>(c);++i;continue;}
        int n=c>=0xc2&&c<=0xdf?2:c>=0xe0&&c<=0xef?3:c>=0xf0&&c<=0xf4?4:0;
        bool valid=n&&i+static_cast<std::size_t>(n)<=limit;
        for(int j=1;valid&&j<n;++j) valid=(static_cast<unsigned char>(s[i+j])&0xc0)==0x80;
        if(valid&&n>=3) {
            const auto b=static_cast<unsigned char>(s[i+1]);
            valid=!(c==0xe0&&b<0xa0)&&!(c==0xed&&b>=0xa0)&&!(c==0xf0&&b<0x90)&&!(c==0xf4&&b>=0x90);
        }
        if(valid){if(n==3&&c==0xe2&&static_cast<unsigned char>(s[i+1])==0x80&&(static_cast<unsigned char>(s[i+2])==0xa8||static_cast<unsigned char>(s[i+2])==0xa9))out+=' ';else out.append(s,i,n);i+=n;}else{out+='?';++i;}
    }
    if(s.size()>limit)out+="...";
    return out;
}
}
