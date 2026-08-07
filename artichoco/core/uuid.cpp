#include "uuid.h"

#include <array>
#include <charconv>
#include <random>
#include <system_error>

namespace arti::core {
namespace {

std::mt19937_64 createRandomEngine()
{
    std::random_device random_device;
    std::array<uint32_t, 8> seed_data{};
    for (uint32_t& seed : seed_data) {
        seed = random_device();
    }
    std::seed_seq seed{seed_data.begin(), seed_data.end()};
    return std::mt19937_64{seed};
}

std::mt19937_64& randomEngine()
{
    thread_local std::mt19937_64 engine = createRandomEngine();
    return engine;
}

} // namespace

UUID UUID::generate()
{
    Value value = 0;
    do {
        value = randomEngine()();
    } while (value == 0);
    return UUID{value};
}

std::optional<UUID> UUID::fromString(std::string_view value) noexcept
{
    constexpr size_t encoded_size = sizeof(Value) * 2;
    if (value.size() != encoded_size) {
        return std::nullopt;
    }

    Value parsed = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed, 16);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        return std::nullopt;
    }
    return UUID{parsed};
}

std::string UUID::toString() const
{
    constexpr size_t encoded_size = sizeof(Value) * 2;
    std::array<char, encoded_size> digits{};
    const auto result = std::to_chars(digits.data(), digits.data() + digits.size(), m_value, 16);
    const size_t digit_count = static_cast<size_t>(result.ptr - digits.data());

    std::string encoded(encoded_size - digit_count, '0');
    encoded.append(digits.data(), digit_count);
    return encoded;
}

} // namespace arti::core
