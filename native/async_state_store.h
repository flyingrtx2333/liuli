#pragma once
#include "state_store.h"
#include <atomic>
#include <condition_variable>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace explicit_ui {
// One worker, one replaceable pending job, one latest completion. No UI/model
// references cross the worker boundary. Notification must be quick/nonblocking.
class AsyncStateStore {
public:
    using RequestId=std::uint64_t;
    enum class Operation { load, save };
    struct Completion {
        RequestId id=0;
        Operation operation=Operation::load;
        state_store::State state;
        std::exception_ptr error;
        bool cancelled=false;
        // True only when save() returned successfully. Cancellation may be too
        // late to prevent a committed write; it never rolls a write back.
        bool save_completed=false;
    };
private:
    struct Job {
        RequestId id;
        Operation operation;
        state_store::State state;
        std::shared_ptr<std::atomic<bool>> cancelled;
    };
    std::string path_;
    std::function<void()> notify_;
    std::mutex mutex_;
    std::condition_variable wake_;
    bool stopping_=false;
    RequestId latest_=0;
    std::optional<Job> pending_;
    std::optional<Completion> completed_;
    std::shared_ptr<std::atomic<bool>> active_cancel_;
    std::atomic<bool> notification_failed_{false};
    std::thread worker_;

    RequestId submit(Operation operation,state_store::State state) {
        auto cancelled=std::make_shared<std::atomic<bool>>(false);
        std::lock_guard<std::mutex> lock(mutex_);
        if(stopping_)throw std::logic_error("state worker is closed");
        if(latest_==std::numeric_limits<RequestId>::max())throw std::overflow_error("state request ID exhausted");
        if(active_cancel_)active_cancel_->store(true);
        // Replacing the pending snapshot releases its memory immediately.
        pending_=Job{++latest_,operation,std::move(state),std::move(cancelled)};
        completed_.reset();
        wake_.notify_one();
        return latest_;
    }
    void run() noexcept {
        for(;;) {
            std::optional<Job> job;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                wake_.wait(lock,[this]{return stopping_||pending_.has_value();});
                if(stopping_)return;
                job=std::move(pending_);pending_.reset();active_cancel_=job->cancelled;
            }
            Completion result;
            result.id=job->id;result.operation=job->operation;
            if(!job->cancelled->load()) {
                try {
                    if(job->operation==Operation::load)result.state=state_store::load(path_);
                    else {state_store::save(path_,job->state);result.save_completed=true;}
                }catch(...){result.error=std::current_exception();}
            }
            result.cancelled=job->cancelled->load();
            bool notify=false;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                active_cancel_.reset();
                if(!stopping_&&job->id==latest_) {
                    completed_=std::move(result);notify=true;
                }
            }
            if(notify&&notify_) {
                try{notify_();}catch(...){notification_failed_.store(true);}
            }
        }
    }
public:
    explicit AsyncStateStore(std::string path,std::function<void()> notify)
        :path_(std::move(path)),notify_(std::move(notify)) {
        if(path_.empty()||path_.size()>32768||path_.find('\0')!=std::string::npos)
            throw std::invalid_argument("invalid state worker path");
        worker_=std::thread([this]{run();});
    }
    AsyncStateStore(const AsyncStateStore&)=delete;
    AsyncStateStore& operator=(const AsyncStateStore&)=delete;
    ~AsyncStateStore(){close();}
    RequestId save(state_store::State state) {
        // Bounded pure validation on the caller thread; all disk I/O is on worker.
        (void)state_store::encode(state);
        return submit(Operation::save,std::move(state));
    }
    RequestId load(){return submit(Operation::load,{});}
    void cancel() {
        std::lock_guard<std::mutex> lock(mutex_);
        if(active_cancel_)active_cancel_->store(true);
        if(pending_)pending_->cancelled->store(true);
    }
    std::optional<Completion> take_completed() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto result=std::move(completed_);completed_.reset();return result;
    }
    bool notification_failed() const noexcept{return notification_failed_.load();}
    // Call from the owning/UI thread, never inside notification. close() is
    // idempotent but must not run concurrently with another close/destruction.
    void close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_=true;
            if(active_cancel_)active_cancel_->store(true);
            pending_.reset();completed_.reset();
        }
        wake_.notify_one();
        if(worker_.joinable())worker_.join();
    }
};
}
