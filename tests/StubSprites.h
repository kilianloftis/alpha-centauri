#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <system_error>

namespace actest
{

// Minimal 1x1 PNG so CI without extract_terrain.py can still exercise sprite draws.
inline constexpr std::uint8_t k_StubPng[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48,
    0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00,
    0x00, 0x1F, 0x15, 0xC4, 0x89, 0x00, 0x00, 0x00, 0x0A, 0x49, 0x44, 0x41, 0x54, 0x78,
    0x9C, 0x63, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00, 0x01, 0x0D, 0x0A, 0x2D, 0xB4, 0x00,
    0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};

// Test processes run in parallel and share these files, so a stub is written once, through a
// rename, and never truncated under another process's load.
inline void WriteStubPng(const std::string& path)
{
    std::error_code error;
    if (std::filesystem::file_size(path, error) == sizeof(k_StubPng))
    {
        return;
    }
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    const std::string tempPath = path + "." + std::to_string(std::random_device{}()) + ".tmp";
    {
        std::ofstream out(tempPath, std::ios::binary);
        out.write(reinterpret_cast<const char*>(k_StubPng), sizeof(k_StubPng));
    }
    std::filesystem::rename(tempPath, path);
}

} // namespace actest
