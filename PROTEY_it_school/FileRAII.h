#pragma once
#include <string>
#include <fstream>
#include <stdexcept>

class FileRAII {
public:
    // Конструктор открывает файл
    FileRAII(const std::string& filename, std::ios_base::openmode mode);

    // Деструктор закрывает файл
    ~FileRAII();

    // Чтение строки
    std::string readLine();

    // Запись строки
    void writeLine(const std::string& line);

    // Запрещаем копирование
    FileRAII(const FileRAII&) = delete;
    FileRAII& operator=(const FileRAII&) = delete;

    // Разрешаем перемещение
    FileRAII(FileRAII&&) noexcept;
    FileRAII& operator=(FileRAII&&) noexcept;

private:
    std::fstream file_;
    std::string filename_;
};