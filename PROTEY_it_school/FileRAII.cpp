#include "FileRAII.h"
#include <stdexcept>

FileRAII::FileRAII(const std::string& filename, std::ios_base::openmode mode)
    : filename_(filename) {
    file_.open(filename, mode);
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
}

FileRAII::~FileRAII() {
    if (file_.is_open()) {
        file_.close();
    }
}

std::string FileRAII::readLine() {
    std::string line;
    if (!std::getline(file_, line)) {
        if (file_.eof()) {
            throw std::runtime_error("End of file reached");
        }
        else {
            throw std::runtime_error("Error reading from file");
        }
    }
    return line;
}

void FileRAII::writeLine(const std::string& line) {
    file_ << line << '\n';
    if (file_.fail()) {
        throw std::runtime_error("Error writing to file");
    }
}
FileRAII::FileRAII(FileRAII&& other) noexcept
    : file_(std::move(other.file_)), filename_(std::move(other.filename_)) {}

FileRAII& FileRAII::operator=(FileRAII&& other) noexcept {
    if (this != &other) {
        if (file_.is_open()) {
            file_.close();
        }
        file_ = std::move(other.file_);
        filename_ = std::move(other.filename_);
    }
    return *this;
}