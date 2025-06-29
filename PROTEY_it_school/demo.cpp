#include "FileRAII.h"
#include <iostream>
#include <string>

int main() {
    try {
        {
            std::cout << "Please type full rath to your file. Example: C:\\Users\\Public\\Documents\\shared.txt" <<std::endl;
            std::string path;
            std::getline(std::cin, path);

            std::cout << "Please enter a line you would like to add to your file" <<std::endl;
            std::string line;
            std::getline(std::cin, line);

            FileRAII writer(path, std::ios::out);
            writer.writeLine(line);
            
            FileRAII reader(path, std::ios::in);
           
            while (true) {
                try {
                    std::string line = reader.readLine();
                    std::cout << "Read: " << line << std::endl;
                }
                catch (const std::runtime_error& e) {
                    if (std::string(e.what()) == "End of file reached") {
                        break;
                    }
                    throw;
                }
            }
        }

        try {
            FileRAII fail_reader("nonexistent.txt", std::ios::in);
        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }

    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}