#pragma once
#include "model_api.h"
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>
#include <system_error>
#include <variant>
#include <vector>
#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace explicit_ui::state_store {
using Value = std::variant<std::int64_t, std::string, std::vector<std::string>, bool>;
using State = std::map<std::string, Value>;
inline constexpr std::size_t max_file_bytes=65536, max_entries=64, max_text_bytes=4096, max_array_items=1000;
namespace detail {
inline void key(const std::string& s) {
    auto alpha=[](unsigned char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_';};
    if(s.empty()||s.size()>128||!alpha(s[0]))throw std::invalid_argument("invalid state key");
    for(unsigned char c:s)if(!alpha(c)&&!(c>='0'&&c<='9')&&c!='.'&&c!='-')throw std::invalid_argument("invalid state key");
}
inline std::size_t size(const std::string& s) {
    std::size_t n=0;const auto r=std::from_chars(s.data(),s.data()+s.size(),n);
    if(r.ec!=std::errc{}||r.ptr!=s.data()+s.size()||std::to_string(n)!=s)throw std::invalid_argument("invalid state length");
    return n;
}
inline std::string line(const std::string& bytes,std::size_t& at) {
    const auto end=bytes.find('\n',at);
    if(end==std::string::npos)throw std::invalid_argument("truncated state header");
    auto s=bytes.substr(at,end-at);at=end+1;return s;
}
#ifndef _WIN32
struct Fd {
    int value=-1;
    explicit Fd(int v):value(v){}
    ~Fd(){if(value>=0)::close(value);}
    Fd(const Fd&)=delete;Fd& operator=(const Fd&)=delete;
};
inline void io_error(const char* what){throw std::system_error(errno,std::generic_category(),what);}
#endif
}
inline std::string encode(const State& state) {
    if(state.size()>max_entries)throw std::invalid_argument("too many state entries");
    bool has_bool=false;
    for(const auto& entry:state) has_bool=has_bool||std::holds_alternative<bool>(entry.second);
    std::string bytes=has_bool?"EUISTATE 3\n":"EUISTATE 2\n";
    for(const auto& entry:state) {
        detail::key(entry.first);
        std::string payload;
        const std::string* value=nullptr;
        char kind='T';
        if(const auto* integer=std::get_if<std::int64_t>(&entry.second)) {
            kind='I';payload=std::to_string(*integer);value=&payload;
        }else if(const auto* boolean=std::get_if<bool>(&entry.second)) {
            kind='B';payload=*boolean?"true":"false";value=&payload;
        }else if(const auto* text=std::get_if<std::string>(&entry.second)) {
            if(text->size()>max_text_bytes)throw std::invalid_argument("state text too large");
            require_utf8(*text);value=text;
        }else {
            kind='A';const auto& array=std::get<std::vector<std::string>>(entry.second);
            if(array.size()>max_array_items)throw std::invalid_argument("too many state array items");
            payload=std::to_string(array.size())+"\n";
            for(const auto& text:array) {
                if(text.size()>max_text_bytes)throw std::invalid_argument("state array text too large");
                require_utf8(text);
                payload+=std::to_string(text.size())+"\n"+text+"\n";
                if(payload.size()>max_file_bytes)throw std::invalid_argument("state array payload too large");
            }
            value=&payload;
        }
        bytes+=std::string(1,kind)+" "+std::to_string(entry.first.size())+" "+std::to_string(value->size())+"\n"+entry.first+*value+"\n";
        if(bytes.size()>max_file_bytes-4)throw std::invalid_argument("state file too large");
    }
    return bytes+"END\n";
}
inline State decode(const std::string& bytes) {
    if(bytes.size()>max_file_bytes)throw std::invalid_argument("state file too large");
    std::size_t at=0;
    const auto version=detail::line(bytes,at);
    if(version!="EUISTATE 1"&&version!="EUISTATE 2"&&version!="EUISTATE 3")throw std::invalid_argument("unsupported state version");
    State state;
    for(;;) {
        const auto header=detail::line(bytes,at);
        if(header=="END") {if(at!=bytes.size())throw std::invalid_argument("trailing state bytes");return state;}
        if(state.size()>=max_entries)throw std::invalid_argument("too many state entries");
        if(header.size()<5||(header[0]!='I'&&header[0]!='T'&&!(header[0]=='A'&&version!="EUISTATE 1")&&!(header[0]=='B'&&version=="EUISTATE 3"))||header[1]!=' ')throw std::invalid_argument("invalid state record");
        const auto space=header.find(' ',2);
        if(space==std::string::npos)throw std::invalid_argument("invalid state record");
        const auto k=detail::size(header.substr(2,space-2)), n=detail::size(header.substr(space+1));
        const auto limit=header[0]=='B'?5:(header[0]=='I'?20:(header[0]=='T'?max_text_bytes:max_file_bytes));
        if(k==0||k>128||n>limit||k>bytes.size()-at||n>=bytes.size()-at-k)throw std::invalid_argument("invalid or truncated state payload");
        auto name=bytes.substr(at,k);at+=k;auto value=bytes.substr(at,n);at+=n;
        if(bytes[at++]!='\n')throw std::invalid_argument("missing state separator");
        detail::key(name);
        Value parsed;
        if(header[0]=='I') {
            std::int64_t number=0;const auto r=std::from_chars(value.data(),value.data()+value.size(),number);
            if(r.ec!=std::errc{}||r.ptr!=value.data()+value.size()||std::to_string(number)!=value)throw std::invalid_argument("invalid state integer");
            parsed=number;
        }else if(header[0]=='B') {
            if(value!="true"&&value!="false")throw std::invalid_argument("invalid state boolean");
            parsed=value=="true";
        }else if(header[0]=='T') {require_utf8(value);parsed=std::move(value);}
        else {
            std::size_t cursor=0;const auto count=detail::size(detail::line(value,cursor));
            // Even empty items require a length line plus separator: 0\n\n.
            if(count>max_array_items||count>(value.size()-cursor)/3)throw std::invalid_argument("invalid state array count");
            std::vector<std::string> array;array.reserve(count);
            for(std::size_t i=0;i<count;++i) {
                const auto length=detail::size(detail::line(value,cursor));
                if(length>max_text_bytes||length>=value.size()-cursor)throw std::invalid_argument("invalid state array item length");
                auto text=value.substr(cursor,length);cursor+=length;
                if(value[cursor++]!='\n')throw std::invalid_argument("missing state array separator");
                require_utf8(text);array.push_back(std::move(text));
            }
            if(cursor!=value.size())throw std::invalid_argument("trailing state array bytes");
            parsed=std::move(array);
        }
        if(!state.emplace(std::move(name),std::move(parsed)).second)throw std::invalid_argument("duplicate state key");
    }
}
inline State load(const std::string& path) {
    if(path.empty()||path.find('\0')!=std::string::npos)throw std::invalid_argument("invalid state path");
    std::string bytes;
#ifndef _WIN32
    detail::Fd fd(::open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK));
    if(fd.value<0)detail::io_error("open state");
    struct stat info{};if(::fstat(fd.value,&info)!=0)detail::io_error("stat state");
    if(!S_ISREG(info.st_mode)||info.st_size<0||static_cast<std::uintmax_t>(info.st_size)>max_file_bytes)throw std::invalid_argument("state must be a bounded regular file");
    std::array<char,4096> buffer{};
    for(;;){const auto n=::read(fd.value,buffer.data(),buffer.size());if(n<0){if(errno==EINTR)continue;detail::io_error("read state");}if(!n)break;
        if(static_cast<std::size_t>(n)>max_file_bytes-bytes.size())throw std::invalid_argument("state file too large");
        bytes.append(buffer.data(),static_cast<std::size_t>(n));}
#else
    std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("cannot open state file");
    std::array<char,4096> buffer{};while(input){input.read(buffer.data(),buffer.size());const auto n=static_cast<std::size_t>(input.gcount());if(n>max_file_bytes-bytes.size())throw std::invalid_argument("state file too large");bytes.append(buffer.data(),n);}
    if(!input.eof())throw std::runtime_error("cannot read state file");
#endif
    return decode(bytes);
}
inline void save(const std::string& path,const State& state) {
    if(path.empty()||path.find('\0')!=std::string::npos)throw std::invalid_argument("invalid state path");
    const auto bytes=encode(state); // Validate before touching filesystem.
#ifdef _WIN32
    (void)bytes;throw std::runtime_error("atomic state replacement backend not implemented on Windows");
#else
    // POSIX rename replaces a directory entry atomically. Never truncate target.
    std::string temporary=path+".tmp.XXXXXX";
    detail::Fd fd(::mkstemp(temporary.data()));if(fd.value<0)detail::io_error("create state temporary");
    struct Cleanup {const std::string& path;~Cleanup(){::unlink(path.c_str());}} cleanup{temporary};
    if(::fcntl(fd.value,F_SETFD,FD_CLOEXEC)<0)detail::io_error("protect state descriptor");
    std::size_t at=0;
    while(at<bytes.size()) {const auto n=::write(fd.value,bytes.data()+at,bytes.size()-at);if(n<0){if(errno==EINTR)continue;detail::io_error("write state");}if(n==0)throw std::runtime_error("zero-byte state write");at+=static_cast<std::size_t>(n);}
    if(::fsync(fd.value)!=0)detail::io_error("flush state");
    const int raw=fd.value;fd.value=-1;if(::close(raw)!=0)detail::io_error("close state");
    if(::rename(temporary.c_str(),path.c_str())!=0)detail::io_error("replace state");
    // No throwing operations after rename: successful replacement is the commit.
#endif
}
} // namespace explicit_ui::state_store
