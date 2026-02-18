#include "FileRAII.h"
#include <iostream>
#include <string>

int main() {
    try {
        {
            std::cout << "Please type full rath to your file. Example: C:\\Users\\Public\\Documents\\shared.txt" <<std::endl;
            std::string path;
            std::getline(std::cin, path);

            std::cout << "Enter 'write' if you would likw to add line to file and 'read' if you would like to read the whole file" << std::endl;
            std::string command;
            std::getline(std::cin, command);


            if (command == "write") {
                std::cout << "Please enter a line you would like to add to your file" << std::endl;
                std::string new_line;
                std::getline(std::cin, new_line);

                FileRAII writer(path, std::ios::out | std::ios::app);
                writer.writeLine(new_line);
            }
            
            if (command == "read") {
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
        }

    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}