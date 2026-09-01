#include "paths.h"

#include <stdexcept>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <string>
#elif defined(__linux__)
#include <system_error>
#else
#error "executableDir() needs a platform implementation (macOS: _NSGetExecutablePath)."
#endif

namespace arti::core {
namespace {

std::filesystem::path resolveExecutablePath()
{
#if defined(_WIN32)
    // GetModuleFileNameW 在缓冲不够时会截断，返回值恰好等于缓冲大小 —— 「刚好填满」和
    // 「装不下」在返回值上是同一个数，所以只能按「没有小于缓冲大小」当作不够，倍增重试。
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD written =
                GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (written == 0) {
            throw std::runtime_error("GetModuleFileNameW failed to resolve the executable path.");
        }
        if (written < buffer.size()) {
            buffer.resize(written);
            return std::filesystem::path{buffer};
        }
        // 32K 是 Windows 长路径的上限量级；到这里还不够就不是路径长的问题了。
        if (buffer.size() > 64 * 1024) {
            throw std::runtime_error("The executable path is unreasonably long.");
        }
        buffer.resize(buffer.size() * 2);
    }
#else
    std::error_code error;
    auto path = std::filesystem::read_symlink("/proc/self/exe", error);
    if (error) {
        throw std::runtime_error("Failed to read /proc/self/exe: " + error.message());
    }
    return path;
#endif
}

} // namespace

const std::filesystem::path& executableDir()
{
    static const std::filesystem::path directory = resolveExecutablePath().parent_path();
    return directory;
}

} // namespace arti::core
