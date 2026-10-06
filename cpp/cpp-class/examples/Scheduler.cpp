/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 26/08/2026 by @author Tsukini

File Name:
##  @file Scheduler.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#include "utils/attribute/Attribute.hpp"
#include "utils/exception/ExceptionDefine.hpp"
#include "utils/exception/basic/ErrorException.hpp"
#include "utils/system/Scheduler.hpp"
#include <cstddef>
#include <algorithm>
#include <thread>
#include <string>
#include <mutex>

_hot void utils::system::Scheduler::clear_(void)
{
    std::lock_guard lock(this->_lock);

    // Doesn't do anything if there is nothing to clear
    if (this->_finished.empty()) return;
    for (std::size_t id: this->_finished) this->cancel_(id);
    this->_finished.clear();
}

_cold void utils::system::Scheduler::cancel(std::size_t id)
{
    auto it = this->_tasks.find(id);
    if (it == this->_tasks.end()) _unlikely {
        throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, std::to_string(id));
    }
    if (it->second.get_id() == std::this_thread::get_id()) return; // called by the task itself: already running, nothing to cancel
    this->_tasks.erase(it); // jthread destruction: stop request + join

    // The task signal its end even when canceled, remove it to not free the id twice
    {
        std::lock_guard lock(this->_lock);
        std::erase(this->_finished, id);
    }
    this->_idHandler.free(id);
}

