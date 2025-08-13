#include "os_interface.h"

#include <iostream>
#include <fcntl.h>
#include <unistd.h>

OSInterface::OSInterface() {
    is_open_ = false;
}

OSInterface::~OSInterface() {
    if (is_open_) {
        file_.close();
    }
}

bool OSInterface::Open(const std::string& filename) {
    filename_ = filename;
    file_.open(filename_, std::ios::in | std::ios::out | std::ios::binary);

    if (!file_.is_open()) {
        file_.open(filename_, std::ios::out | std::ios::binary);
        file_.close();
        file_.open(filename_, std::ios::in | std::ios::out | std::ios::binary);
    }

    is_open_ = file_.is_open();
    return is_open_;
}

bool OSInterface::ReadPage(uint32_t page_num, std::span<std::byte> dest) {
    file_.seekg(static_cast<std::streamoff>(page_num) * dest.size_bytes(), std::ios::beg);
    file_.read(reinterpret_cast<char*>(dest.data()), dest.size_bytes());
    return file_.good();
}

bool OSInterface::WritePage(uint32_t page_num, std::span<const std::byte> src) {
    file_.seekp(static_cast<std::streamoff>(page_num) * src.size_bytes(), std::ios::beg);
    file_.write(reinterpret_cast<const char*>(src.data()), src.size_bytes());
    return file_.good();
}

bool OSInterface::Sync() {
    file_.flush();

    // Force OS to flush buffers to disk
    int fd = ::open(filename_.c_str(), O_RDWR);
    if (fd < 0) return false;
    bool success = (fsync(fd) == 0);
    ::close(fd);
    return success;
}

void OSInterface::Close() {
    file_.close();
    is_open_ = false;
}
