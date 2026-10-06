/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 21/09/2026 by @author Tsukini

File Name:
##  @file Scheduler.hpp

File Description:
##  Scheduler definition
\**************************************************************/

#ifndef SCHEDULER_H
    #define SCHEDULER_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #include "../security/observer/Observer.hpp"    // utils::security::observer::Observer
    #include "../attribute/Attribute.hpp"           // _cold, _hot, _unlikely
    #include "IdHandler.hpp"                        // utils::system::IdHandler
    #include <condition_variable>                   // std::condition_variable_any
    #include <unordered_map>                        // std::unordered_map
    #include <type_traits>                          // std::is_invocable_r_v
    #include <cstddef>                              // std::size_t
    #include <thread>                               // std::jthread
    #include <chrono>                               // std::chrono::milliseconds
    #include <vector>                               // std::vector
    #include <mutex>                                // std::mutex, std::unique_lock, std::lock_guard

namespace utils::system { // namespace start
//----------------------------------------------------------------//
/* CLASS */

class Scheduler: private utils::security::observer::Observer<"Scheduler"> {
    private:
        utils::system::IdHandler<std::size_t> _idHandler;

        /* destruction */
        std::mutex _lock;
        std::vector<std::size_t> _finished;

        /* tasks */
        std::unordered_map<std::size_t, std::jthread> _tasks;

        // ---------- Pre-Function -------- //
        void clear(void); // clear finished task

        // ------------ Function ---------- //
        _cold inline void cancel_(std::size_t id) {this->_tasks.erase(id); this->_idHandler.free(id);};

    public:
        // ---------- Pre-Function -------- //
        void cancel(std::size_t id); // cancel given task

        // ------------ Function ---------- //
        _cold inline void cancel(void) {this->_tasks.clear(); this->_finished.clear(); this->_idHandler.free();}
        template<typename Fn>
        _hot std::size_t schedule(std::chrono::milliseconds delay, const Fn& fn) // schedule a new task
        {
            static_assert(std::is_invocable_r_v<void, Fn>, "Fn must be callable with no arguments (signature: void(void))");
            this->clear();

            // Get a new id
            std::size_t id = this->_idHandler.allocate();

            // Setup the new thread
            this->_tasks.emplace(id, std::jthread([this, id, delay, fn](std::stop_token stoken) {
                // Setup condition_variable_any
                std::mutex mutex;
                std::unique_lock<std::mutex> cvlock(mutex);
                std::condition_variable_any cv;

                // Wait until timeout or cancel trigger
                (void)cv.wait_for(cvlock, stoken, delay, []{return false;});

                if (!stoken.stop_requested()) _likely {fn();}

                // Signal the ending of the task
                std::lock_guard lock(this->_lock);
                this->_finished.push_back(id);
            }));

            return id;
        }

        // ------------ Operator ---------- //
        Scheduler& operator=(const Scheduler& other) = delete;
        Scheduler& operator=(Scheduler&& other) = delete;

        // ---------- Constructor --------- //
        Scheduler() = default;
        Scheduler(const Scheduler& other) = delete;
        Scheduler(Scheduler&& other) = delete;

        // ----------- Destructor --------- //
        ~Scheduler() = default;
};

} // namespace end
#endif /* SCHEDULER_H */
