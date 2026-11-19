#pragma once
// Pure helpers behind the hardware readings (CPU / GPU / RAM / VRAM usage, GPU and SSD temperature), kept
// free of Windows calls so tests/hardware_model_test.cpp can check them. hardware.cpp does the reading.
#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <map>
#include <string>
#include <vector>

namespace hardware_model {

// One instance of a wildcard performance counter, e.g. "\GPU Engine(*)\Utilization Percentage".
struct CounterSample {
    std::wstring instance;  // "pid_1234_luid_0x00000000_0x0000D1E4_phys_0_eng_3_engtype_3D"
    double value = 0;
};

inline std::wstring Lower(std::wstring s) {
    for (auto& c : s) c = (wchar_t)std::towlower(c);
    return s;
}

// How the GPU performance counters name an adapter: "luid_0x<high>_0x<low>", compared case-insensitively.
inline std::wstring LuidTag(long high, unsigned long low) {
    wchar_t buf[40];
    std::swprintf(buf, 40, L"luid_0x%08lx_0x%08lx", (unsigned long)high, low);
    return buf;
}

inline bool MatchesAdapter(const std::wstring& instance, const std::wstring& luidTag) {
    return !luidTag.empty() && Lower(instance).find(Lower(luidTag)) != std::wstring::npos;
}

// GPU usage like Task Manager: per engine, add up every process's share; the busiest engine is the GPU's
// usage. -1 when no instance belongs to the adapter.
inline int GpuUsage(const std::vector<CounterSample>& engines, const std::wstring& luidTag) {
    std::map<std::wstring, double> perEngine;  // "phys_0_eng_3" -> summed percentage
    for (const auto& s : engines) {
        if (!MatchesAdapter(s.instance, luidTag)) continue;
        std::wstring name = Lower(s.instance);
        size_t phys = name.find(L"phys_");
        size_t type = name.find(L"_engtype", phys == std::wstring::npos ? 0 : phys);
        if (phys == std::wstring::npos || name.find(L"_eng_", phys) == std::wstring::npos) continue;
        perEngine[name.substr(phys, type == std::wstring::npos ? std::wstring::npos : type - phys)] += s.value;
    }
    if (perEngine.empty()) return -1;
    double busiest = 0;
    for (const auto& e : perEngine) busiest = std::max(busiest, e.second);
    return (int)std::clamp(busiest + 0.5, 0.0, 100.0);
}

// Bytes reported by "\GPU Adapter Memory(*)\Dedicated Usage" for the adapter; -1 when none match.
inline double AdapterBytes(const std::vector<CounterSample>& memory, const std::wstring& luidTag) {
    double sum = 0;
    bool found = false;
    for (const auto& s : memory)
        if (MatchesAdapter(s.instance, luidTag)) { sum += s.value; found = true; }
    return found ? sum : -1;
}

// Rounded share of `part` in `whole`, 0..100; -1 when either is unknown.
inline int Percent(double part, double whole) {
    if (part < 0 || whole <= 0) return -1;
    return (int)std::clamp(part * 100.0 / whole + 0.5, 0.0, 100.0);
}

// CPU usage from two GetSystemTimes calls: the kernel time includes the idle time.
inline int CpuUsage(unsigned long long idleDelta, unsigned long long kernelDelta, unsigned long long userDelta) {
    unsigned long long total = kernelDelta + userDelta;
    if (total == 0 || idleDelta > total) return -1;
    return Percent((double)(total - idleDelta), (double)total);
}

// Temperatures in whole degrees Celsius; anything outside 1..149 is a sensor that is absent or not ready.
inline bool PlausibleTemperature(int c) { return c > 0 && c < 150; }

enum class Heat { Normal, Warm, Hot };
// Orange from 80 C and red from 90 C for a GPU; 65 / 75 C for an SSD (NVMe drives throttle near 80).
inline Heat GpuHeat(int c) { return c >= 90 ? Heat::Hot : c >= 80 ? Heat::Warm : Heat::Normal; }
inline Heat SsdHeat(int c) { return c >= 75 ? Heat::Hot : c >= 65 ? Heat::Warm : Heat::Normal; }

}  // namespace hardware_model
