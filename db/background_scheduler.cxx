#include "background_scheduler.h"
#include "common.h"
#include "disk_manager.h"
#include <exception>

/* TODO: support multiple background threads */
Background_Scheduler::Background_Scheduler(DiskManager* disk_manager): disk_manager_(disk_manager) {
    for (int i=0; i<NUM_BACKGROUND_THREADS; i++) {
        /* TODO: Learn about lamdba functions */
        background_threads_.emplace_back([this] {
            while (true) {
                if (stop_) {
                    return;
                }
                if (request_queue_.empty())
                    continue;

                std::shared_ptr<Request> req = std::move(request_queue_.front());
                request_queue_.pop();
                Request& r = *req;
                /* Check if page_id is still valid as page could have been deleted. */
                try {
                    if (r.read_) {
                        disk_manager_->ReadPage(r.page_id_, r.read_data_);
                    } else {
                        disk_manager_->WritePage(r.page_id_, r.write_data_);
                    }
                    r.promise_.set_value(true);
                } catch (...) {
                    r.promise_.set_exception(std::current_exception());
                }
            }
        });
    }
}

Background_Scheduler::~Background_Scheduler() {
    stop_ = true;
    for (int i=0; i<NUM_BACKGROUND_THREADS; i++) {
        background_threads_[i].join();
    }
}

void Background_Scheduler::Schedule(std::shared_ptr<Request> req) {
    request_queue_.push(std::move(req));
}

void Background_Scheduler::DeletePage(page_id_t page_id) {
    disk_manager_->DeletePage(page_id);
}

bool Background_Scheduler::CheckPageExists(page_id_t page_id) {
    return disk_manager_->CheckPageExists(page_id);
}