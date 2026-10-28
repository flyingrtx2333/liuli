#pragma once
#include "model_api.h"
#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace explicit_ui {
// Bounded UTF-8 editor. All positions are byte offsets at codepoint boundaries.
// Legacy codepoint methods remain for portable callers. The native host supplies
// Pango grapheme boundaries through move_to/erase_range.
class TextEdit {
    std::string text_, preedit_;
    std::size_t limit_, caret_=0, anchor_=0, preedit_start_=0, preedit_end_=0;
    struct Snapshot {std::string text;std::size_t caret,anchor;};
    std::vector<Snapshot> undo_,redo_;
    static constexpr std::size_t history_limit=64;
    Snapshot snapshot() const{return {text_,caret_,anchor_};}
    static void push(std::vector<Snapshot>& history,Snapshot value){
        history.push_back(std::move(value));
        if(history.size()>history_limit)history.erase(history.begin());
    }
    void restore(Snapshot value){text_.swap(value.text);caret_=value.caret;anchor_=value.anchor;cancel_preedit();}
    static bool boundary(const std::string& text, std::size_t position) {
        return position<=text.size() && (position==text.size() ||
            (static_cast<unsigned char>(text[position])&0xc0)!=0x80);
    }
    static void require_position(const std::string& text,std::size_t position) {
        if(!boundary(text,position)) throw std::invalid_argument("text position is not a UTF-8 boundary");
    }
    void require_text(const std::string& text) const {
        if(text.size()>limit_) throw std::invalid_argument("text exceeds byte limit");
        require_utf8(text);
    }
    std::size_t previous(std::size_t position) const {
        if(position==0)return 0;
        --position;
        while(position>0 && (static_cast<unsigned char>(text_[position])&0xc0)==0x80)--position;
        return position;
    }
    std::size_t next(std::size_t position) const {
        if(position==text_.size())return position;
        ++position;
        while(position<text_.size() && (static_cast<unsigned char>(text_[position])&0xc0)==0x80)++position;
        return position;
    }
    void move(std::size_t position,bool extend) {
        caret_=position;if(!extend)anchor_=caret_;cancel_preedit();
    }
    void replace(std::size_t start,std::size_t end,const std::string& inserted) {
        // Validate first and stage allocation before mutating text, caret or preedit.
        require_text(inserted);
        const auto retained=text_.size()-(end-start);
        if(inserted.size()>limit_-retained) throw std::invalid_argument("text exceeds byte limit");
        std::string candidate;
        candidate.reserve(retained+inserted.size());
        candidate.append(text_,0,start);candidate+=inserted;candidate.append(text_,end,std::string::npos);
        if(candidate!=text_){push(undo_,snapshot());redo_.clear();}
        text_.swap(candidate);caret_=start+inserted.size();anchor_=caret_;cancel_preedit();
    }
public:
    explicit TextEdit(std::size_t byte_limit=256):limit_(byte_limit){}
    TextEdit(std::string text,std::size_t byte_limit=256):limit_(byte_limit){reset(std::move(text));}
    const std::string& text() const noexcept{return text_;}
    std::size_t byte_limit() const noexcept{return limit_;}
    std::size_t caret() const noexcept{return caret_;}
    std::size_t anchor() const noexcept{return anchor_;}
    std::size_t selection_start() const noexcept{return std::min(anchor_,caret_);}
    std::size_t selection_end() const noexcept{return std::max(anchor_,caret_);}
    bool has_selection() const noexcept{return anchor_!=caret_;}
    std::string selected_text() const{return text_.substr(selection_start(),selection_end()-selection_start());}
    void reset(std::string text){require_text(text);text_.swap(text);caret_=anchor_=text_.size();cancel_preedit();undo_.clear();redo_.clear();}
    void select(std::size_t anchor,std::size_t caret){require_position(text_,anchor);require_position(text_,caret);anchor_=anchor;caret_=caret;cancel_preedit();}
    void select_all(){anchor_=0;caret_=text_.size();cancel_preedit();}
    std::size_t undo_size() const noexcept{return undo_.size();}
    std::size_t redo_size() const noexcept{return redo_.size();}
    void undo(){if(undo_.empty())return;auto value=undo_.back();push(redo_,snapshot());undo_.pop_back();restore(std::move(value));}
    void redo(){if(redo_.empty())return;auto value=redo_.back();push(undo_,snapshot());redo_.pop_back();restore(std::move(value));}
    void move_to(std::size_t position,bool extend=false){require_position(text_,position);move(position,extend);}
    void erase_range(std::size_t start,std::size_t end){require_position(text_,start);require_position(text_,end);if(start>end)throw std::invalid_argument("reversed deletion");replace(start,end,"");}
    void move_left(bool extend=false){move(!extend&&has_selection()?selection_start():previous(caret_),extend);}
    void move_right(bool extend=false){move(!extend&&has_selection()?selection_end():next(caret_),extend);}
    void move_home(bool extend=false){move(0,extend);}
    void move_end(bool extend=false){move(text_.size(),extend);}
    void insert(const std::string& text){replace(selection_start(),selection_end(),text);}
    void backspace(){if(has_selection())replace(selection_start(),selection_end(),"");else if(caret_>0)replace(previous(caret_),caret_,"");}
    void erase_forward(){if(has_selection())replace(selection_start(),selection_end(),"");else if(caret_<text_.size())replace(caret_,next(caret_),"");}
    // Preedit belongs to the OS composition session, not committed application state.
    // Selection offsets here are UTF-8 bytes in preedit. Adapters must convert their
    // platform's units and range-check before calling this method.
    void set_preedit(std::string text,std::size_t selection_start,std::size_t selection_end){
        require_text(text);require_position(text,selection_start);require_position(text,selection_end);
        if(selection_start>selection_end)throw std::invalid_argument("reversed preedit selection");
        preedit_.swap(text);preedit_start_=selection_start;preedit_end_=selection_end;
    }
    const std::string& preedit() const noexcept{return preedit_;}
    std::size_t preedit_selection_start() const noexcept{return preedit_start_;}
    std::size_t preedit_selection_end() const noexcept{return preedit_end_;}
    void cancel_preedit() noexcept{preedit_.clear();preedit_start_=preedit_end_=0;}
    void commit_preedit(const std::string& text){insert(text);}
};
}
