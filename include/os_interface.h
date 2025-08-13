#ifndef OS_INTERFACE_H
#define OS_INTERFACE_H

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <span>
#include <string>

class OSInterface {
    public:
        OSInterface();
        ~OSInterface();

        // Opens the file.
        bool Open(const std::string& filename);
        // Reads the data from page_num into the dest span. It is assumed that dest is the size of exactly 1 page.
        bool ReadPage(uint32_t page_num, std::span<std::byte> dest);
        // Writes the data from page_num into the src span. It is assumed that src is the size of exactly 1 page.
        bool WritePage(uint32_t page_num, std::span<const std::byte> src);
        // Flushes any existing changes to the file such that the file is updated.
        bool Sync();
        // Closes the file.
        void Close();

    private:
        std::fstream file_;
        std::string filename_;
        bool is_open_;
};


#endif
