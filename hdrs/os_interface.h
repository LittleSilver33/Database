#ifndef OS_INTERFACE_H
#define OS_INTERFACE_H

#include <string>
#include <cstdint>
#include <fstream>

class OSInterface {
    public:
        OSInterface();
        ~OSInterface();

        bool Open(const std::string& filename);
        bool ReadPage(uint32_t page_num, void* dest, size_t page_size);
        bool WritePage(uint32_t page_num, void* dest, size_t page_size);
        bool Sync();
        void Close();

    private:
        std::fstream file_;
        std::string filename_;
        bool is_open_;
};


#endif
