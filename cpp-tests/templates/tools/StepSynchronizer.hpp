/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗ 
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 16/08/2026 by @author Tsukini

File Name:
##  @file StepSynchronizer.hpp

File Description:
##  Tools used to multithread synchronize task
\**************************************************************/

#ifndef STEPSYNCHRONIZER_H
    #define STEPSYNCHRONIZER_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #include <condition_variable>   // std::condition_variable
    #include <thread>               // std::thread
    #include <mutex>                // std::mutex, std::lock_guard, std::unique_lock

namespace tests::tools { // namespace start
//----------------------------------------------------------------//
/* CLASS */

class StepSynchronizer {
    private:
        std::mutex _mutex;
        std::condition_variable _cv;
        int _currentStep = -1;

    public:
        void waitForStep(int step) {
            std::unique_lock<std::mutex> lock(this->_mutex);
            this->_cv.wait(lock, [&]{return this->_currentStep == step;});
        }
        void advanceTo(int step) {
            {
                std::lock_guard<std::mutex> lock(this->_mutex);
                this->_currentStep = step;
            }
            this->_cv.notify_all();
        }
};

} // namespace end
#endif /* STEPSYNCHRONIZER_H */
