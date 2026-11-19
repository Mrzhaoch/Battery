#include "../src/hardware_model.h"
#include <cassert>
#include <cstdio>
using namespace hardware_model;

int main() {
    // Adapter tag: how "\GPU Engine(*)" instance names spell the LUID.
    const std::wstring tag = LuidTag(0, 0xD1E4);
    assert(tag == L"luid_0x00000000_0x0000d1e4");
    assert(MatchesAdapter(L"pid_42_luid_0x00000000_0x0000D1E4_phys_0_eng_0_engtype_3D", tag));  // case-insensitive
    assert(!MatchesAdapter(L"pid_42_luid_0x00000000_0x0000AAAA_phys_0_eng_0_engtype_3D", tag));
    assert(!MatchesAdapter(L"pid_42_luid_0x00000000_0x0000D1E4_phys_0_eng_0_engtype_3D", L""));

    // GPU usage: processes add up per engine, the busiest engine wins, other adapters are ignored.
    std::vector<CounterSample> engines = {
        { L"pid_1_luid_0x00000000_0x0000D1E4_phys_0_eng_0_engtype_3D", 30 },
        { L"pid_2_luid_0x00000000_0x0000D1E4_phys_0_eng_0_engtype_3D", 25 },
        { L"pid_1_luid_0x00000000_0x0000D1E4_phys_0_eng_3_engtype_VideoDecode", 40 },
        { L"pid_3_luid_0x00000000_0x0000AAAA_phys_0_eng_0_engtype_3D", 99 },  // the integrated GPU
    };
    assert(GpuUsage(engines, tag) == 55);
    const std::vector<CounterSample> busy = {
        { L"pid_1_luid_0x00000000_0x0000D1E4_phys_0_eng_0_engtype_3D", 70 },
        { L"pid_2_luid_0x00000000_0x0000D1E4_phys_0_eng_0_engtype_3D", 60 },
    };
    assert(GpuUsage(busy, tag) == 100);  // capped
    assert(GpuUsage(engines, LuidTag(0, 0x1234)) == -1);  // adapter not in the list
    assert(GpuUsage(std::vector<CounterSample>(), tag) == -1);
    // Two physical parts of one adapter stay separate engines.
    const std::vector<CounterSample> linked = {
        { L"pid_1_luid_0x00000000_0x0000D1E4_phys_0_eng_0_engtype_3D", 20 },
        { L"pid_1_luid_0x00000000_0x0000D1E4_phys_1_eng_0_engtype_3D", 35 },
    };
    assert(GpuUsage(linked, tag) == 35);

    // Video memory: bytes for the adapter, as a share of its dedicated memory.
    std::vector<CounterSample> memory = {
        { L"luid_0x00000000_0x0000D1E4_phys_0", 2.0e9 },
        { L"luid_0x00000000_0x0000AAAA_phys_0", 5.0e8 },
    };
    assert(AdapterBytes(memory, tag) == 2.0e9);
    assert(AdapterBytes(memory, LuidTag(0, 0x1234)) == -1);
    assert(Percent(AdapterBytes(memory, tag), 8.0e9) == 25);
    assert(Percent(-1, 8.0e9) == -1);
    assert(Percent(1, 0) == -1);

    // CPU: kernel time includes idle time.
    assert(CpuUsage(750, 900, 100) == 25);
    assert(CpuUsage(0, 0, 0) == -1);
    assert(CpuUsage(2000, 900, 100) == -1);  // inconsistent counters
    assert(CpuUsage(0, 600, 400) == 100);

    // Temperatures and their colours.
    assert(!PlausibleTemperature(0) && PlausibleTemperature(35) && !PlausibleTemperature(150) && !PlausibleTemperature(-5));
    assert(GpuHeat(79) == Heat::Normal && GpuHeat(80) == Heat::Warm && GpuHeat(90) == Heat::Hot);
    assert(SsdHeat(64) == Heat::Normal && SsdHeat(65) == Heat::Warm && SsdHeat(75) == Heat::Hot);

    puts("Hardware reading tests passed");
}
