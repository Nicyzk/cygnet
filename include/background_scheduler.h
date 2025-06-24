#include <queue>
#include "common.h"
#include <future>
#include "disk_manager.h"

#pragma once

struct Request {
    bool read_;
    page_id_t page_id_;
    char *read_data_;         /* For read requests */
    const char *write_data_;  /* For write requests */
    std::promise<bool> promise_;

    Request(bool read, page_id_t page_id, char* read_data):
        read_(read), page_id_(page_id), read_data_(read_data) {}

    Request(bool read, page_id_t page_id, const char* write_data):
        read_(read), page_id_(page_id), write_data_(write_data) {}

    Request(const Request&) = delete;

    Request(Request&& that): read_(that.read_), page_id_(that.page_id_),
    read_data_(that.read_data_), write_data_(that.write_data_), promise_(std::move(that.promise_)) {}

};

class Background_Scheduler {
    private:
        std::vector<std::thread> background_threads_;
        std::queue<std::shared_ptr<Request>> request_queue_;
        std::mutex request_queue_lock_;
        DiskManager* disk_manager_;
        bool stop_ = false;
        /* 
        TODO: support multiple threads
        std::condition_variable cv_;
        std::mutex request_queue_lock_;
        */
    public:
        Background_Scheduler(DiskManager* disk_manager);
        ~Background_Scheduler();
        void Schedule(std::shared_ptr<Request> req); /* TODO: add error handling */
        void DeletePage(page_id_t page_id);
        bool CheckPageExists(page_id_t page_id);
};