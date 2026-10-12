/**************************************************************\
Edition:
##  @date 11/10/2026 by @author Tsukini

File Name:
##  @file Worker.hpp

File Description:
##  One background thread running the jobs of an interface in
##  their order (send, /clear, restart...): the interface stays
##  responsive, the prompt is cleared at once
\**************************************************************/

#ifndef CLUSTER_WORKER_H
    #define CLUSTER_WORKER_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold
    #include <condition_variable>   // std::condition_variable
    #include <functional>           // std::function
    #include <thread>               // std::thread
    #include <deque>                // std::deque
    #include <mutex>                // std::mutex

namespace cluster { // namespace start
//----------------------------------------------------------------//
/* CLASS */

class Worker {
    private:
        std::mutex _mutex;
        std::condition_variable _wake;
        std::deque<std::function<void(void)>> _jobs;
        bool _stopping = false;
        std::thread _thread;

        // ---------- Pre-Function -------- //
        _cold void run_(void);

    public:
        // ---------- Pre-Function -------- //
        _cold void push(std::function<void(void)> job);   // run after the ones before it
        _cold void stop(void);                             // the running job ends, the others are dropped

        // ---------- Constructor -------- //
        Worker(void);

        // ----------- Destructor --------- //
        ~Worker();
};

} // namespace end

#endif /* CLUSTER_WORKER_H */
