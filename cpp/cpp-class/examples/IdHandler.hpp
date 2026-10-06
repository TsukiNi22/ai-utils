/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 01/10/2026 by @author Tsukini

File Name:
##  @file IdHandler.hpp

File Description:
##  Handle id allocation
\**************************************************************/

#ifndef IDHANDLER_H
    #define IDHANDLER_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #include "../attribute/Attribute.hpp"               // _hot, _cold, _unlikely, _likely, _nodiscard
    #include "../exception/ExceptionDefine.hpp"         // utils::exception::InternalCode
    #include "../exception/basic/ErrorException.hpp"    // utils::exception::ErrorException
    #include "../exception/custom/FatalException.hpp"   // utils::exception::FatalException
    #include <limits>                                   // std::numeric_limits<T>
    #include <mutex>                                    // std::mutex
    #include <set>                                      // std::set

namespace utils::system { // namespace start
//----------------------------------------------------------------//
/* CLASS */

template<typename T>
class IdHandler {
    static_assert(std::is_integral_v<T>, "T must be an integral type");

    private:
        mutable std::mutex _lock; // Handling of multithreading
        // 0 is reserved for unallocated ones / default value
        T _id = std::numeric_limits<T>::min();
        std::set<T> _usedIds; // only for forced allocated ids
        std::set<T> _freeIds;

        // ------------ Function ---------- //
        _hot T allocate_(const bool safe_mode = true)
        {
            std::unique_lock<std::mutex> lock(this->_lock, std::defer_lock);
            if (safe_mode) lock.lock();
            else (void)lock.try_lock();

            T id;
            if (this->_freeIds.size() > 0) {
                auto it = this->_freeIds.begin();
                id = *it;
                this->_freeIds.erase(id);
            } else _likely {
                do { // skip 0 (reserved) & the forced ids, the overflow is checked at each step
                    if (this->_id == std::numeric_limits<T>::max()) _unlikely {
                        throw utils::exception::FatalException(utils::exception::InternalCode::IdOverflow);
                    }
                    id = ++this->_id;
                } while (id == 0 || this->_usedIds.contains(id));
            }
            return id;
        };
        _hot void free_(const T id, const bool safe_mode = true)
        {
            std::unique_lock<std::mutex> lock(this->_lock, std::defer_lock);
            if (safe_mode) lock.lock();
            else (void)lock.try_lock();

            // 0 is never distributed
            if (id == 0) _unlikely {
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidId, "The id 0 is reserved");
            }

            // Above the counter: only a forced id can be freed (the counter will give it later)
            if (id > this->_id) _unlikely {
                if (this->_usedIds.erase(id) == 0) _unlikely {
                    throw utils::exception::ErrorException(utils::exception::InternalCode::UnknownId, "This id was never distributed");
                }
                return;
            }

            // Only if the id wasn't already free
            if (!this->_freeIds.insert(id).second) _unlikely {
                throw utils::exception::ErrorException(utils::exception::InternalCode::DoubleFree);
            }

            // Remove it from the forced ones (if it was forced)
            this->_usedIds.erase(id);
        };

    public:
        // ------------ Function ---------- //
        _cold inline void use(T id, const bool safe_mode = true)
        {
            std::unique_lock<std::mutex> lock(this->_lock, std::defer_lock);
            if (safe_mode) lock.lock();
            else (void)lock.try_lock();

            // Check if the id is free (released or never distributed)
            if (id == 0) _unlikely {
                throw utils::exception::ErrorException(utils::exception::InternalCode::InvalidId, "The id 0 is reserved");
            } else if (this->_freeIds.contains(id)) _unlikely {
                this->_freeIds.erase(id);
            } else if (id <= this->_id) _unlikely { // already distributed and still in use
                throw utils::exception::ErrorException(utils::exception::InternalCode::DoubleUse);
            }

            // Store the forced id
            if (!this->_usedIds.insert(id).second) _unlikely {
                throw utils::exception::ErrorException(utils::exception::InternalCode::DoubleUse);
            }
        };
        _cold _nodiscard inline T id(void) const {return this->_id;};
        _cold _nodiscard inline T actual(const bool safe_mode = true) const
        {
            std::unique_lock<std::mutex> lock(this->_lock, std::defer_lock);
            if (safe_mode) lock.lock();
            else (void)lock.try_lock();

            if (this->_freeIds.size() > 0) _likely {return *this->_freeIds.begin();}
            else _unlikely {return this->_id;}
        };
        _cold _nodiscard T preview(const bool safe_mode = true) const
        {
            std::unique_lock<std::mutex> lock(this->_lock, std::defer_lock);
            if (safe_mode) lock.lock();
            else (void)lock.try_lock();

            if (this->_freeIds.size() > 0) {
                return *this->_freeIds.begin();
            }

            // Same as allocate (skip 0 & the forced ids) without changing anything
            T id = this->_id;
            do {
                if (id == std::numeric_limits<T>::max()) _unlikely {
                    throw utils::exception::ErrorException(utils::exception::InternalCode::IdOverflow);
                }
                ++id;
            } while (id == 0 || this->_usedIds.contains(id));
            return id;
        };
        _hot T allocate(T& id, const bool safe_mode = true)     {return (id = this->allocate_(safe_mode));};
        _hot _nodiscard T allocate(const bool safe_mode = true) {return this->allocate_(safe_mode);};
        _hot void free(T& id, const bool safe_mode = true)       {this->free_(id, safe_mode); id = 0;}
        _hot void free(const T& id, const bool safe_mode = true) {this->free_(id, safe_mode);};
        _cold void free(void) {this->clear(true);}; // free every id
        _cold void clear(const bool safe_mode = true) // free every id
        {
            std::unique_lock<std::mutex> lock(this->_lock, std::defer_lock);
            if (safe_mode) lock.lock();
            else (void)lock.try_lock();

            // Reset value (no id in circulation)
            this->_id = std::numeric_limits<T>::min();
            this->_freeIds.clear();
            this->_usedIds.clear();
        };

        // ------------ Operator ---------- //
        IdHandler& operator=(const IdHandler& other) = delete;
        IdHandler& operator=(IdHandler&& other) = delete;

        // ---------- Constructor --------- //
        IdHandler() = default;
        IdHandler(const IdHandler& other) = delete;
        IdHandler(IdHandler&& other) = delete;

        // ----------- Destructor --------- //
        ~IdHandler() = default;
};

} // namespace end
#endif /* IDHANDLER_H */
