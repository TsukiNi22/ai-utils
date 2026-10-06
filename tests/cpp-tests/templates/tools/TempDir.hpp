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
##  @file TempDir.hpp

File Description:
##  RAII temporary directory & environment override used by the tests
\**************************************************************/

#ifndef TEMPDIR_H
    #define TEMPDIR_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #include <filesystem>   // std::filesystem::path, std::filesystem::temp_directory_path
    #include <stdexcept>    // std::runtime_error
    #include <optional>     // std::optional
    #include <cstdlib>      // mkdtemp, setenv, unsetenv, getenv
    #include <string>       // std::string

namespace tests::tools { // namespace start
//----------------------------------------------------------------//
/* CLASS */

// Create a unique directory at construction and remove it at destruction
class TempDir {
    private:
        std::filesystem::path _path;

    public:
        const std::filesystem::path& path(void) const {return this->_path;};
        std::filesystem::path operator/(const std::string& name) const {return this->_path / name;};

        TempDir(void)
        {
            std::string pattern = (std::filesystem::temp_directory_path() / "utils-tests-XXXXXX").string();
            if (::mkdtemp(pattern.data()) == nullptr)
                throw std::runtime_error("mkdtemp failed");
            this->_path = pattern;
        };
        TempDir(const TempDir& other) = delete;
        TempDir& operator=(const TempDir& other) = delete;
        ~TempDir() {std::error_code ec; std::filesystem::remove_all(this->_path, ec);};
};

// Override an environment variable and restore it at destruction
class ScopedEnv {
    private:
        std::string _name;
        std::optional<std::string> _old;

    public:
        ScopedEnv(const std::string& name, const std::string& value): _name{name}
        {
            if (const char* old = std::getenv(name.c_str())) this->_old = old;
            ::setenv(name.c_str(), value.c_str(), 1);
        };
        ScopedEnv(const ScopedEnv& other) = delete;
        ScopedEnv& operator=(const ScopedEnv& other) = delete;
        ~ScopedEnv()
        {
            if (this->_old.has_value()) ::setenv(this->_name.c_str(), this->_old->c_str(), 1);
            else ::unsetenv(this->_name.c_str());
        };
};

} // namespace end
#endif /* TEMPDIR_H */
