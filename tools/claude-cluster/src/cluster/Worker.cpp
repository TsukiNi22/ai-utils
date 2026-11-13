/**************************************************************\
Edition:
##  @date 11/10/2026 by @author Tsukini

File Name:
##  @file Worker.cpp

File Description:
##  One background thread running jobs in their order
\**************************************************************/

/* include */
#define _Attribute
#include <utils/utils.hpp>
#include "cluster/Worker.hpp"
#include <exception>

/* constructor */
cluster::Worker::Worker(void)
    : _thread(&cluster::Worker::run_, this)
{
}

cluster::Worker::~Worker()
{
    this->stop();
}

/* jobs */
_cold void cluster::Worker::push(std::function<void(void)> job)
{
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (this->_stopping) return;
        this->_jobs.push_back(std::move(job));
    }
    this->_wake.notify_one();
}

_cold void cluster::Worker::stop(void)
{
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        this->_stopping = true;
        this->_jobs.clear();
    }
    this->_wake.notify_one();
    if (this->_thread.joinable()) this->_thread.join();
}

_cold void cluster::Worker::run_(void)
{
    while (true) {
        std::function<void(void)> job;
        {
            std::unique_lock<std::mutex> lock(this->_mutex);
            this->_wake.wait(lock, [this]() {return this->_stopping || !this->_jobs.empty();});
            if (this->_stopping) return;
            job = std::move(this->_jobs.front());
            this->_jobs.pop_front();
        }
        try {
            job();
        } catch (const std::exception&) {} // a job reports its own errors to its interface
    }
}
