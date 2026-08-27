#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include <glslang/Public/ShaderLang.h>

namespace
{
    // spock::compileShader (used both directly and via createGraphicsPipeline
    // in several test cases) requires glslang's process-wide state to be
    // initialised before any TShader is parsed, and finalised once at exit.
    class GlslangProcessListener : public Catch::EventListenerBase
    {
    public:
        using Catch::EventListenerBase::EventListenerBase;

        void testRunStarting(Catch::TestRunInfo const &) override
        {
            glslang::InitializeProcess();
        }

        void testRunEnded(Catch::TestRunStats const &) override
        {
            glslang::FinalizeProcess();
        }
    };
} // namespace

CATCH_REGISTER_LISTENER(GlslangProcessListener)
