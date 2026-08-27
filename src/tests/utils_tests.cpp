#include "spock/utils.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <limits>
#include <sstream>

#if defined(__linux__)
#include <unistd.h>
#define SPOCK_TEST_CAN_CAPTURE_STDERR 1
#endif

TEST_CASE("checked_cast passes through in-range values", "[utils]")
{
    CHECK(spock::checked_cast<uint32_t>(uint64_t{0}) == 0u);
    CHECK(spock::checked_cast<uint32_t>(uint64_t{42}) == 42u);
    CHECK(spock::checked_cast<uint16_t>(uint32_t{1234}) == uint16_t{1234});
    CHECK(spock::checked_cast<uint8_t>(uint32_t{200}) == uint8_t{200});
}

TEST_CASE("checked_cast preserves the maximum representable value of the target type", "[utils]")
{
    uint64_t maxUint32AsUint64 = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max());
    CHECK(spock::checked_cast<uint32_t>(maxUint32AsUint64) == std::numeric_limits<uint32_t>::max());

    uint32_t maxUint16AsUint32 = static_cast<uint32_t>(std::numeric_limits<uint16_t>::max());
    CHECK(spock::checked_cast<uint16_t>(maxUint16AsUint32) == std::numeric_limits<uint16_t>::max());
}

#if defined(SPOCK_TEST_CAN_CAPTURE_STDERR)
namespace
{
    // Temporarily redirects stderr to a file so writeLog's output can be
    // inspected, then restores the original stderr before returning.
    std::string captureStderrDuring(std::function<void()> const &action)
    {
        std::string path = "/tmp/spock_test_writelog_" + std::to_string(::getpid()) + ".log";

        fflush(stderr);
        int savedStderrFd = dup(fileno(stderr));
        FILE *redirected = freopen(path.c_str(), "w", stderr);
        REQUIRE(redirected != nullptr);

        action();

        fflush(stderr);
        dup2(savedStderrFd, fileno(stderr));
        close(savedStderrFd);
        clearerr(stderr);

        std::ifstream captured(path);
        std::stringstream buffer;
        buffer << captured.rdbuf();
        std::remove(path.c_str());

        return buffer.str();
    }
} // namespace

TEST_CASE("writeLog emits the given message on stderr", "[utils]")
{
    std::string message = "spock writeLog test message";

    std::string output = captureStderrDuring([&] { spock::writeLog(message); });

    CHECK(output.find(message) != std::string::npos);
}
#else
TEST_CASE("writeLog does not throw or crash", "[utils]")
{
    CHECK_NOTHROW(spock::writeLog("spock writeLog smoke test"));
}
#endif
