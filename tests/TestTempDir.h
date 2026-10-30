#pragma once

#include <filesystem>
#include <random>
#include <string>
#include <system_error>

namespace actest
{

// ctest runs test cases as concurrent processes, so each process writes its temp files into a
// directory of its own, removed when the process exits.
class TestTempDir
{
public:
    static const std::filesystem::path& Path()
    {
        static const TestTempDir dir;
        return dir.m_path;
    }

private:
    TestTempDir()
    {
        std::random_device entropy;
        do
        {
            const std::string name = "ac_tests_" + std::to_string(entropy());
            m_path = std::filesystem::temp_directory_path() / name;
        } while (!std::filesystem::create_directory(m_path));
    }

    ~TestTempDir()
    {
        std::error_code ignored;
        std::filesystem::remove_all(m_path, ignored);
    }

    std::filesystem::path m_path;
};

} // namespace actest
