"""Bounded worker lifecycle and real temporary-file effects, no SDL dependency."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]

class AsyncStateStoreTests(unittest.TestCase):
    def test_worker_effects_cancellation_supersession_and_shutdown(self):
        source=r'''
#include "async_state_store.h"
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
using explicit_ui::AsyncStateStore;
namespace ss=explicit_ui::state_store;
struct Notifications {
 std::mutex mutex;std::condition_variable changed;int count=0;
 bool block=false,entered=false,released=false;
 void notify(){std::unique_lock<std::mutex> lock(mutex);++count;entered=true;changed.notify_all();if(block)changed.wait(lock,[&]{return released;});}
 void wait(int n){std::unique_lock<std::mutex> lock(mutex);assert(changed.wait_for(lock,std::chrono::seconds(5),[&]{return count>=n;}));}
 void release(){std::lock_guard<std::mutex> lock(mutex);released=true;changed.notify_all();}
};
ss::State value(std::int64_t n){return {{"count",n},{"text",std::string("中文")},{"items",std::vector<std::string>{"a","😀"}}};}
int main(int argc,char** argv){
 assert(argc==2);const std::string path=std::string(argv[1])+"/state";
 Notifications n;AsyncStateStore worker(path,[&]{n.notify();});
 auto caller=value(1);const auto id=worker.save(caller);caller["count"]=std::int64_t{99};n.wait(1);
 auto done=worker.take_completed();assert(done&&done->id==id&&!done->error&&!done->cancelled&&done->save_completed);
 assert(!worker.take_completed());assert(ss::load(path)==value(1));
 const auto read=worker.load();n.wait(2);done=worker.take_completed();assert(done&&done->id==read&&done->state==value(1)&&!done->save_completed);
 {std::ofstream bad(path);bad<<"broken";}
 worker.load();n.wait(3);done=worker.take_completed();assert(done&&done->error);bool caught=false;try{std::rethrow_exception(done->error);}catch(const std::exception&){caught=true;}assert(caught);
 // Input validation rejects before job submission and never touches original file.
 auto too_large=value(9);too_large["text"]=std::string(4097,'x');caught=false;try{worker.save(too_large);}catch(const std::invalid_argument&){caught=true;}assert(caught);
 worker.close();worker.close();caught=false;try{worker.load();}catch(const std::logic_error&){caught=true;}assert(caught);
 // Pause only the notification callback to deterministically occupy the worker.
 // Production notification callbacks must not block; no fake filesystem backend.
 Notifications gate;gate.block=true;
 AsyncStateStore bounded(path,[&]{gate.notify();});bounded.save(value(10));gate.wait(1);
 const auto skipped=bounded.save(value(11));const auto newest=bounded.save(value(12));assert(newest>skipped);
 assert(!bounded.take_completed());gate.release();gate.wait(2);
 done=bounded.take_completed();assert(done&&done->id==newest&&done->save_completed);assert(ss::load(path)==value(12));bounded.close();
 Notifications cancel_gate;cancel_gate.block=true;
 AsyncStateStore cancelled(path,[&]{cancel_gate.notify();});cancelled.save(value(20));cancel_gate.wait(1);
 const auto cancelled_id=cancelled.save(value(21));cancelled.cancel();cancel_gate.release();cancel_gate.wait(2);
 done=cancelled.take_completed();assert(done&&done->id==cancelled_id&&done->cancelled&&!done->save_completed&&!done->error);assert(ss::load(path)==value(20));cancelled.close();
 // Newer work invalidates an unconsumed prior completion; mailbox stays bounded.
 Notifications repeated;AsyncStateStore many(path,[&]{repeated.notify();});
 for(int i=0;i<40;++i){const auto current=many.save(value(i));repeated.wait(i+1);done=many.take_completed();assert(done&&done->id==current);}
 many.close();assert(ss::load(path)==value(39));
 // Notification exceptions cannot terminate the worker or discard its completion.
 AsyncStateStore throwing(path,[]{throw std::runtime_error("notification failure");});throwing.load();
 for(int i=0;i<500&&!throwing.notification_failed();++i)std::this_thread::sleep_for(std::chrono::milliseconds(2));
 assert(throwing.notification_failed());done=throwing.take_completed();assert(done&&!done->error&&done->state==value(39));throwing.close();
 // Destructor joins outstanding work and does not notify after returning.
 Notifications shutdown;
 {AsyncStateStore closing(path,[&]{shutdown.notify();});closing.save(value(100));}
 const int callbacks=shutdown.count;std::this_thread::sleep_for(std::chrono::milliseconds(10));assert(shutdown.count==callbacks);
 Notifications failed;AsyncStateStore failure(std::string(argv[1])+"/missing/state",[&]{failed.notify();});failure.save(value(1));failed.wait(1);done=failure.take_completed();assert(done&&done->error&&!done->save_completed);failure.close();
}
'''
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'test.cpp').write_text(source)
            subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-O1','-Wall','-Wextra','-Wpedantic','-Werror','-pthread','-I',str(ROOT/'native'),str(p/'test.cpp'),'-o',str(p/'test')],check=True,capture_output=True)
            subprocess.run([str(p/'test'),str(p)],check=True,capture_output=True,timeout=15)

if __name__=='__main__':unittest.main()
