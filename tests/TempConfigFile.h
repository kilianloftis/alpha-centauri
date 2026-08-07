#pragma once

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace actest
{

// A config file written for one test and removed with it. Names are made unique per instance,
// so concurrently running test binaries cannot read each other's files.
class TempConfigFile
{
public:
    TempConfigFile(const char* name, const std::string& rContents)
    {
        static std::atomic<unsigned> counter{0};
        m_path = std::filesystem::temp_directory_path()
                 / (std::to_string(counter++) + "_" + name);

        std::ofstream out(m_path);
        if (!out.is_open())
        {
            throw std::runtime_error("could not write test config " + m_path.string());
        }
        out << rContents;
        out.close();
        REQUIRE(std::filesystem::exists(m_path));
    }

    ~TempConfigFile()
    {
        std::error_code ignored;
        std::filesystem::remove(m_path, ignored);
    }

    TempConfigFile(const TempConfigFile&) = delete;
    TempConfigFile& operator=(const TempConfigFile&) = delete;

    std::string Path() const { return m_path.string(); }

private:
    std::filesystem::path m_path;
};

} // namespace actest
