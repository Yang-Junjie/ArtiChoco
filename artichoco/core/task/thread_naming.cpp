#include "thread_naming.h"

#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace arti::core::detail {

void setCurrentThreadName(const char* name)
{
    if (name == nullptr) {
        return;
    }

#ifdef _WIN32
    // SetThreadDescription 吃宽字符（Win10 1607+，本项目的下限就是 Win10）。名字只有 ASCII，
    // 所以逐字节放大就够了 —— 为两行诊断代码拉一套真正的编码转换不值得。
    // 返回值是 HRESULT，刻意不看：起不上名不影响任何行为。
    const std::string narrow{name};
    const std::wstring wide{narrow.begin(), narrow.end()};
    SetThreadDescription(GetCurrentThread(), wide.c_str());
#else
    // Linux 的 pthread_setname_np 限 16 字节（含结尾的 \0），超了整个调用会失败而不是截断，
    // 所以这里自己先截。
    std::string truncated{name};
    if (truncated.size() > 15) {
        truncated.resize(15);
    }
    pthread_setname_np(pthread_self(), truncated.c_str());
#endif
}

} // namespace arti::core::detail
