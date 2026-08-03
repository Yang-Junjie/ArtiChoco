
#include "uuid.h"

#include <random>

namespace arti::core {

// 全局单例随机数生成器
static std::mt19937_64& getRng()
{
    static std::random_device rd;
    static std::mt19937_64 rng(rd());
    return rng;
}

UUID::UUID()
{
    auto& rng = getRng();
    do {
        m_uuid = rng();
    } while (m_uuid == 0);
}

UUID::UUID(uint64_t uuid)
    : m_uuid(uuid)
{}

} // namespace arti::core
