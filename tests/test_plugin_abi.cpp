#include <Windows.h>

#include <cstdint>
#include <iostream>
#include <string>

namespace {

struct NVSEInterfaceCompat {
    std::uint32_t nvse_version{};
    std::uint32_t runtime_version{};
    std::uint32_t editor_version{};
    std::uint32_t is_editor{};
};

struct PluginInfoCompat {
    std::uint32_t info_version{};
    const char* name{};
    std::uint32_t version{};
};

using QueryFunction = bool (*)(const NVSEInterfaceCompat*, PluginInfoCompat*);

bool Require(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    HMODULE module = LoadLibraryA(argv[1]);
    if (!Require(module != nullptr, "could not load plugin DLL")) {
        return 1;
    }
    const auto query = reinterpret_cast<QueryFunction>(GetProcAddress(module, "NVSEPlugin_Query"));
    if (!Require(query != nullptr, "NVSEPlugin_Query export is missing")) {
        return 1;
    }
    if (!Require(GetProcAddress(module, "NVSEPlugin_Load") != nullptr, "NVSEPlugin_Load export is missing")) {
        return 1;
    }

    NVSEInterfaceCompat nvse{};
    nvse.nvse_version = 0x06040050u;
    nvse.runtime_version = 0x040020D0u;
    PluginInfoCompat info{};
    if (!Require(query(&nvse, &info), "supported runtime was rejected")) {
        return 1;
    }
    if (!Require(info.info_version == 1u && info.version == 1u &&
                 std::string(info.name) == "Pip-Boy Video Player", "plugin metadata is invalid")) {
        return 1;
    }

    nvse.runtime_version = 0x040020C0u;
    if (!Require(!query(&nvse, &info), "unsupported runtime was accepted")) {
        return 1;
    }
    nvse.runtime_version = 0x040020D0u;
    nvse.nvse_version = 0u;
    if (!Require(!query(&nvse, &info), "outdated xNVSE was accepted")) {
        return 1;
    }
    nvse.nvse_version = 0x06040050u;
    nvse.is_editor = 1u;
    if (!Require(!query(&nvse, &info), "editor process was accepted")) {
        return 1;
    }
    FreeLibrary(module);
    std::cout << "plugin ABI and rejection checks passed\n";
    return 0;
}
