#pragma once
#include <string>
#include <fstream>
#include <stdexcept>

class FileRAII {
public: 
    FileRAII(const std::string& filename, std::ios_base::openmode mode);
    ~FileRAII();
    std::string readLine();
    void writeLine(const std::string& line);

    FileRAII(const FileRAII&) = delete;
    FileRAII& operator=(const FileRAII&) = delete;

    FileRAII(FileRAII&&) noexcept;
    FileRAII& operator=(FileRAII&&) noexcept;

private:
    std::fstream file_;
    std::string filename_;
};