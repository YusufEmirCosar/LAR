#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

std::uint64_t nextRandom(std::uint64_t &state) noexcept {
    state ^= state << 13U;
    state ^= state >> 7U;
    state ^= state << 17U;
    return state;
}

} // namespace

int main(int argc, char **argv) {
    std::size_t iterations = 5000U;
    if (argc == 2) {
        char *end = nullptr;
        const auto requested = std::strtoull(argv[1], &end, 10);
        if (end == argv[1] || *end != '\0' || requested == 0U || requested > 1'000'000U) {
            std::cerr << "Iteration count must be between 1 and 1000000.\n";
            return 2;
        }
        iterations = static_cast<std::size_t>(requested);
    } else if (argc > 2) {
        std::cerr << "Usage: parser-fuzz [iteration-count]\n";
        return 2;
    }

    std::uint64_t randomState = 0x4c415246555a5a31ULL;
    std::vector<std::uint8_t> bytes;
    for (std::size_t iteration = 0U; iteration < iterations; ++iteration) {
        const std::size_t size = static_cast<std::size_t>(nextRandom(randomState) % 8193U);
        bytes.resize(size);
        for (std::uint8_t &value : bytes) {
            value = static_cast<std::uint8_t>(nextRandom(randomState) & 0xffU);
        }
        if (!bytes.empty() && iteration % 4U == 0U) {
            bytes.front() = static_cast<std::uint8_t>('{');
        }
        if (bytes.size() > 1U && iteration % 7U == 0U) {
            bytes.back() = static_cast<std::uint8_t>('}');
        }
        (void)LLVMFuzzerTestOneInput(bytes.data(), bytes.size());
    }

    std::cout << "Completed " << iterations << " deterministic sanitizer-backed mutations.\n";
    return 0;
}
