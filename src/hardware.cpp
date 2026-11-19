// Hardware readings; see hardware.h. Each source opens only while a metric that needs it is switched on,
// and every library is loaded from System32 by name at that point and unloaded again when the last
// metric that needs it is switched off, so a reading that is off costs no memory and no work.
// Rough cost while on: CPU and RAM nothing (two kernel calls); GPU temperature one driver call;
// SSD temperature one open handle; GPU and VRAM usage load pdh.dll and walk the GPU performance
// counters (the most expensive readings; switch both off to unload it).
#include <windows.h>
#include <winioctl.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <dxgi.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

#include "hardware.h"
#include "hardware_model.h"

namespace hardware {
namespace {

using hardware_model::CounterSample;

HMODULE SystemDll(const wchar_t* name) { return LoadLibraryExW(name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32); }

template <typename Fn>
bool Resolve(HMODULE m, const char* name, Fn& fn) {
    fn = m ? reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(m, name))) : nullptr;
    return fn != nullptr;
}

unsigned g_mask = 0;
Readings g_last;

// ---- CPU and RAM ---------------------------------------------------------------------------------

ULONGLONG g_idle = 0, g_kernel = 0, g_user = 0;  // previous GetSystemTimes; g_kernel == 0: none yet

ULONGLONG Ticks(const FILETIME& f) { return ((ULONGLONG)f.dwHighDateTime << 32) | f.dwLowDateTime; }

int ReadCpu() {
    FILETIME i, k, u;
    if (!GetSystemTimes(&i, &k, &u)) return -1;
    ULONGLONG idle = Ticks(i), kernel = Ticks(k), user = Ticks(u);
    int pct = g_kernel && kernel >= g_kernel && user >= g_user && idle >= g_idle
                  ? hardware_model::CpuUsage(idle - g_idle, kernel - g_kernel, user - g_user)
                  : -1;
    g_idle = idle;
    g_kernel = kernel;
    g_user = user;
    return pct;
}

int ReadRam() {
    MEMORYSTATUSEX m{};
    m.dwLength = sizeof(m);
    if (!GlobalMemoryStatusEx(&m) || !m.ullTotalPhys) return -1;
    return hardware_model::Percent((double)(m.ullTotalPhys - m.ullAvailPhys), (double)m.ullTotalPhys);
}

// ---- The graphics adapter ------------------------------------------------------------------------

struct Adapter {
    bool found = false;
    LUID luid{};
    UINT vendor = 0;
    double dedicatedBytes = 0;
    std::wstring tag;  // how the performance counters name it
};
Adapter g_adapter;
bool g_adapterTried = false;
int g_gpuMisses = 0;       // samples in a row with no GPU value; the adapter is looked up again after a while
int g_gpuRetryAfter = 30;  // misses before the next lookup; doubles while nothing turns up (at most an hour)

// The hardware adapter with the most dedicated memory: the discrete GPU when there is one.
void FindAdapter() {
    if (g_adapterTried) return;
    g_adapterTried = true;
    HMODULE dxgi = SystemDll(L"dxgi.dll");
    using CreateFactoryFn = HRESULT(WINAPI*)(REFIID, void**);
    CreateFactoryFn create = nullptr;
    IDXGIFactory1* factory = nullptr;
    if (Resolve(dxgi, "CreateDXGIFactory1", create) &&
        SUCCEEDED(create(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))) && factory) {
        IDXGIAdapter1* a = nullptr;
        for (UINT i = 0; SUCCEEDED(factory->EnumAdapters1(i, &a)) && a; i++) {
            DXGI_ADAPTER_DESC1 d{};
            bool physical = SUCCEEDED(a->GetDesc1(&d)) && !(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) &&
                            d.VendorId != 0x1414;  // 0x1414: Microsoft Basic Render Driver
            if (physical && (!g_adapter.found || (double)d.DedicatedVideoMemory > g_adapter.dedicatedBytes)) {
                g_adapter.found = true;
                g_adapter.luid = d.AdapterLuid;
                g_adapter.vendor = d.VendorId;
                g_adapter.dedicatedBytes = (double)d.DedicatedVideoMemory;
            }
            a->Release();
            a = nullptr;
        }
        factory->Release();
    }
    if (dxgi) FreeLibrary(dxgi);
    if (g_adapter.found) g_adapter.tag = hardware_model::LuidTag(g_adapter.luid.HighPart, g_adapter.luid.LowPart);
}

// ---- GPU and video memory usage: performance counters -----------------------------------------------

struct PdhApi {
    PDH_STATUS(WINAPI* open)(LPCWSTR, DWORD_PTR, PDH_HQUERY*) = nullptr;
    PDH_STATUS(WINAPI* add)(PDH_HQUERY, LPCWSTR, DWORD_PTR, PDH_HCOUNTER*) = nullptr;
    PDH_STATUS(WINAPI* collect)(PDH_HQUERY) = nullptr;
    PDH_STATUS(WINAPI* array)(PDH_HCOUNTER, DWORD, LPDWORD, LPDWORD, PPDH_FMT_COUNTERVALUE_ITEM_W) = nullptr;
    PDH_STATUS(WINAPI* close)(PDH_HQUERY) = nullptr;
};
PdhApi g_pdh;
HMODULE g_pdhDll = nullptr;
PDH_HQUERY g_query = nullptr;
PDH_HCOUNTER g_engines = nullptr, g_memory = nullptr;
std::vector<BYTE> g_buffer;

bool LoadPdh() {
    if (g_pdhDll) return true;
    g_pdhDll = SystemDll(L"pdh.dll");
    bool ok = Resolve(g_pdhDll, "PdhOpenQueryW", g_pdh.open) && Resolve(g_pdhDll, "PdhAddEnglishCounterW", g_pdh.add) &&
              Resolve(g_pdhDll, "PdhCollectQueryData", g_pdh.collect) &&
              Resolve(g_pdhDll, "PdhGetFormattedCounterArrayW", g_pdh.array) &&
              Resolve(g_pdhDll, "PdhCloseQuery", g_pdh.close);
    if (!ok) {
        if (g_pdhDll) FreeLibrary(g_pdhDll);
        g_pdhDll = nullptr;
        g_pdh = PdhApi{};
    }
    return ok;
}

void OpenQuery() {
    if (g_query || !LoadPdh()) return;
    if (g_pdh.open(nullptr, 0, &g_query) != ERROR_SUCCESS) { g_query = nullptr; return; }
    // English names work in every Windows language; the wildcards pick up processes that start later.
    if (g_pdh.add(g_query, L"\\GPU Engine(*)\\Utilization Percentage", 0, &g_engines) != ERROR_SUCCESS) g_engines = nullptr;
    if (g_pdh.add(g_query, L"\\GPU Adapter Memory(*)\\Dedicated Usage", 0, &g_memory) != ERROR_SUCCESS) g_memory = nullptr;
    g_pdh.collect(g_query);  // the utilization is a rate: it needs a first sample to compare with
}

// Closes the query and unloads pdh.dll, so GPU / VRAM switched off costs no memory at all.
void CloseQuery() {
    if (g_query && g_pdh.close) g_pdh.close(g_query);
    g_query = nullptr;
    g_engines = g_memory = nullptr;
    std::vector<BYTE>().swap(g_buffer);
    if (g_pdhDll) FreeLibrary(g_pdhDll);
    g_pdhDll = nullptr;
    g_pdh = PdhApi{};
}

std::vector<CounterSample> Instances(PDH_HCOUNTER counter) {
    std::vector<CounterSample> out;
    if (!counter) return out;
    for (int attempt = 0; attempt < 3; attempt++) {  // the instance list can grow between the size and the read
        DWORD bytes = (DWORD)g_buffer.size(), count = 0;
        auto* items = g_buffer.empty() ? nullptr : reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(g_buffer.data());
        PDH_STATUS s = g_pdh.array(counter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &bytes, &count, items);
        if ((DWORD)s == (DWORD)PDH_MORE_DATA) {
            g_buffer.resize((size_t)bytes + 4096);
            continue;
        }
        if (s != ERROR_SUCCESS || !items) return out;
        for (DWORD i = 0; i < count; i++) {
            const auto& it = items[i];
            if (it.szName && (it.FmtValue.CStatus == PDH_CSTATUS_VALID_DATA || it.FmtValue.CStatus == PDH_CSTATUS_NEW_DATA))
                out.push_back({ it.szName, it.FmtValue.doubleValue });
        }
        return out;
    }
    return out;
}

// ---- GPU temperature: the graphics driver (D3DKMT), NVIDIA's nvml.dll as a fallback --------------------

// D3DKMT structures as declared in the Windows SDK's d3dkmthk.h (not every MinGW header set has them).
struct KmtOpenAdapterFromLuid { LUID AdapterLuid; UINT hAdapter; };
struct KmtQueryAdapterInfo { UINT hAdapter; UINT Type; void* pPrivateDriverData; UINT PrivateDriverDataSize; };
struct KmtCloseAdapter { UINT hAdapter; };
struct KmtAdapterPerfData {
    UINT32 PhysicalAdapterIndex;
    ULONGLONG MemoryFrequency, MaxMemoryFrequency, MaxMemoryFrequencyOC, MemoryBandwidth, PCIEBandwidth;
    ULONG FanRPM, Power, Temperature;  // Temperature in tenths of a degree Celsius
    UCHAR PowerStateOverride;
};
constexpr UINT kAdapterPerfData = 62;  // KMTQAITYPE_ADAPTERPERFDATA, WDDM 2.4 (Windows 10 1803) and later
using KmtCall = LONG(APIENTRY*)(void*);
KmtCall g_kmtQuery = nullptr, g_kmtClose = nullptr;
UINT g_kmtAdapter = 0;

using NvmlInitFn = int (*)();
using NvmlHandleFn = int (*)(unsigned, void**);
using NvmlTemperatureFn = int (*)(void*, int, unsigned*);
using NvmlShutdownFn = int (*)();
HMODULE g_nvml = nullptr;
void* g_nvmlDevice = nullptr;
NvmlTemperatureFn g_nvmlTemperature = nullptr;
NvmlShutdownFn g_nvmlShutdown = nullptr;

int KmtTemperature() {
    if (!g_kmtAdapter || !g_kmtQuery) return -1;
    KmtAdapterPerfData p{};
    KmtQueryAdapterInfo q{ g_kmtAdapter, kAdapterPerfData, &p, sizeof(p) };
    if (g_kmtQuery(&q) != 0) return -1;
    int c = (int)((p.Temperature + 5) / 10);
    return hardware_model::PlausibleTemperature(c) ? c : -1;
}

int NvmlTemperature() {
    unsigned c = 0;
    if (!g_nvmlDevice || !g_nvmlTemperature || g_nvmlTemperature(g_nvmlDevice, 0 /* NVML_TEMPERATURE_GPU */, &c) != 0)
        return -1;
    return hardware_model::PlausibleTemperature((int)c) ? (int)c : -1;
}

// `initialized`: nvmlInit_v2 succeeded, so nvmlShutdown is owed.
void CloseNvml(bool initialized) {
    if (g_nvml) {
        if (initialized && g_nvmlShutdown) g_nvmlShutdown();
        FreeLibrary(g_nvml);
    }
    g_nvml = nullptr;
    g_nvmlDevice = nullptr;
    g_nvmlTemperature = nullptr;
    g_nvmlShutdown = nullptr;
}

void CloseGpuTemperature() {
    if (g_kmtAdapter && g_kmtClose) {
        KmtCloseAdapter c{ g_kmtAdapter };
        g_kmtClose(&c);
    }
    g_kmtAdapter = 0;
    CloseNvml(g_nvmlDevice != nullptr);
}

void OpenGpuTemperature() {
    if (!g_adapter.found || g_kmtAdapter || g_nvml) return;
    HMODULE gdi = GetModuleHandleW(L"gdi32.dll");
    KmtCall open = nullptr;
    if (Resolve(gdi, "D3DKMTOpenAdapterFromLuid", open) && Resolve(gdi, "D3DKMTQueryAdapterInfo", g_kmtQuery) &&
        Resolve(gdi, "D3DKMTCloseAdapter", g_kmtClose)) {
        KmtOpenAdapterFromLuid o{ g_adapter.luid, 0 };
        if (open(&o) == 0) g_kmtAdapter = o.hAdapter;
    }
    if (KmtTemperature() >= 0 || g_adapter.vendor != 0x10DE) return;
    // NVIDIA: the driver installs nvml.dll in System32. Only the first NVIDIA GPU is read.
    g_nvml = SystemDll(L"nvml.dll");
    NvmlInitFn init = nullptr;
    NvmlHandleFn handle = nullptr;
    if (!Resolve(g_nvml, "nvmlInit_v2", init) || !Resolve(g_nvml, "nvmlDeviceGetHandleByIndex_v2", handle) ||
        !Resolve(g_nvml, "nvmlDeviceGetTemperature", g_nvmlTemperature) ||
        !Resolve(g_nvml, "nvmlShutdown", g_nvmlShutdown) || init() != 0) {
        CloseNvml(false);
        return;
    }
    void* device = nullptr;
    if (handle(0, &device) == 0 && device) g_nvmlDevice = device;
    else CloseNvml(true);
}

int ReadGpuTemperature() {
    int c = KmtTemperature();
    return c >= 0 ? c : NvmlTemperature();
}

// ---- SSD temperature: the drive that holds Windows ------------------------------------------------

HANDLE g_disk = INVALID_HANDLE_VALUE;

void OpenDisk() {
    if (g_disk != INVALID_HANDLE_VALUE) return;
    wchar_t windows[MAX_PATH];
    UINT n = GetWindowsDirectoryW(windows, MAX_PATH);
    if (n < 2 || n >= MAX_PATH || windows[1] != L':') return;
    wchar_t volume[] = L"\\\\.\\C:";
    volume[4] = windows[0];
    // Access 0: enough for the queries below, and no admin rights needed.
    HANDLE v = CreateFileW(volume, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (v == INVALID_HANDLE_VALUE) return;
    alignas(8) BYTE buf[sizeof(VOLUME_DISK_EXTENTS) + 8 * sizeof(DISK_EXTENT)];
    DWORD got = 0;
    BOOL ok = DeviceIoControl(v, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, nullptr, 0, buf, sizeof(buf), &got, nullptr);
    CloseHandle(v);
    const auto* extents = reinterpret_cast<const VOLUME_DISK_EXTENTS*>(buf);
    if (!ok || got < sizeof(VOLUME_DISK_EXTENTS) || extents->NumberOfDiskExtents < 1) return;
    wchar_t disk[40];
    std::swprintf(disk, 40, L"\\\\.\\PhysicalDrive%lu", (unsigned long)extents->Extents[0].DiskNumber);
    g_disk = CreateFileW(disk, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
}

void CloseDisk() {
    if (g_disk != INVALID_HANDLE_VALUE) CloseHandle(g_disk);
    g_disk = INVALID_HANDLE_VALUE;
}

// STORAGE_TEMPERATURE_INFO / STORAGE_TEMPERATURE_DATA_DESCRIPTOR as declared in the Windows SDK's
// winioctl.h (Windows 10); declared here because older MinGW headers lack them or hide them behind NTDDI.
struct SsdTemperatureInfo {
    WORD Index;
    SHORT Temperature;  // degrees Celsius
    SHORT OverThreshold, UnderThreshold;
    BOOLEAN OverThresholdChangable, UnderThresholdChangable, EventGenerated;
    BYTE Reserved0;
    DWORD Reserved1;
};
struct SsdTemperatureDescriptor {
    DWORD Version, Size;
    SHORT CriticalTemperature, WarningTemperature;
    WORD InfoCount;
    BYTE Reserved0[2];
    DWORD Reserved1[2];
    SsdTemperatureInfo TemperatureInfo[1];
};
static_assert(sizeof(SsdTemperatureInfo) == 16, "STORAGE_TEMPERATURE_INFO layout");
static_assert(offsetof(SsdTemperatureDescriptor, TemperatureInfo) == 24, "STORAGE_TEMPERATURE_DATA_DESCRIPTOR layout");
constexpr int kStorageDeviceTemperatureProperty = 52;  // STORAGE_PROPERTY_ID StorageDeviceTemperatureProperty

// The drive's own temperature (the NVMe "composite" sensor comes first); -1 when it reports none.
int ReadSsdTemperature() {
    if (g_disk == INVALID_HANDLE_VALUE) return -1;
    STORAGE_PROPERTY_QUERY q{};
    q.PropertyId = static_cast<STORAGE_PROPERTY_ID>(kStorageDeviceTemperatureProperty);
    q.QueryType = PropertyStandardQuery;
    alignas(8) BYTE buf[512];
    DWORD got = 0;
    if (!DeviceIoControl(g_disk, IOCTL_STORAGE_QUERY_PROPERTY, &q, sizeof(q), buf, sizeof(buf), &got, nullptr)) return -1;
    const size_t head = offsetof(SsdTemperatureDescriptor, TemperatureInfo);
    if (got < head) return -1;
    const auto* d = reinterpret_cast<const SsdTemperatureDescriptor*>(buf);
    const SsdTemperatureInfo* info = d->TemperatureInfo;
    for (WORD i = 0; i < d->InfoCount && head + (size_t)(i + 1) * sizeof(SsdTemperatureInfo) <= got; i++)
        if (hardware_model::PlausibleTemperature(info[i].Temperature)) return info[i].Temperature;
    return -1;
}

constexpr unsigned kCounterMetrics = Bit(Gpu) | Bit(Vram);
constexpr unsigned kAdapterMetrics = kCounterMetrics | Bit(GpuTemp);

// Opens the GPU sources the mask needs (looking the adapter up first) and closes the others.
void ApplyGpu(unsigned mask) {
    if (mask & kAdapterMetrics) FindAdapter();
    if ((mask & kCounterMetrics) && g_adapter.found) OpenQuery();
    else CloseQuery();
    if ((mask & Bit(GpuTemp)) && g_adapter.found) OpenGpuTemperature();
    else CloseGpuTemperature();
    if (!(mask & kAdapterMetrics)) {
        g_adapter = Adapter{};
        g_adapterTried = false;
    }
}

}  // namespace

void Configure(unsigned mask) {
    mask &= kAllMetrics;
    if (mask == g_mask) return;
    ApplyGpu(mask);
    if (mask & Bit(SsdTemp)) OpenDisk();
    else CloseDisk();
    if (!(mask & Bit(Cpu))) g_kernel = 0;
    else if (!g_kernel) ReadCpu();  // the baseline, so the first Sample already has a CPU value
    g_gpuMisses = 0;
    g_gpuRetryAfter = 30;
    g_mask = mask;
    g_last = Readings{};
}

bool Sample(Readings& out) {
    Readings r;
    if ((g_mask & kCounterMetrics) && g_query) g_pdh.collect(g_query);
    if (g_mask & Bit(Cpu)) r.value[Cpu] = ReadCpu();
    if (g_mask & Bit(Ram)) r.value[Ram] = ReadRam();
    if (g_mask & Bit(Gpu)) r.value[Gpu] = hardware_model::GpuUsage(Instances(g_engines), g_adapter.tag);
    if (g_mask & Bit(Vram))
        r.value[Vram] = hardware_model::Percent(hardware_model::AdapterBytes(Instances(g_memory), g_adapter.tag),
                                                g_adapter.dedicatedBytes);
    if (g_mask & Bit(GpuTemp)) r.value[GpuTemp] = ReadGpuTemperature();
    if (g_mask & Bit(SsdTemp)) r.value[SsdTemp] = ReadSsdTemperature();

    // A driver update or a GPU reset gives the adapter a new LUID: after a while with nothing, look again,
    // less and less often while the PC keeps reporting nothing (a GPU without these counters or sensors).
    if (g_mask & kAdapterMetrics) {
        bool any = r.value[Gpu] >= 0 || r.value[Vram] >= 0 || r.value[GpuTemp] >= 0;
        if (any) {
            g_gpuMisses = 0;
            g_gpuRetryAfter = 30;
        } else if (++g_gpuMisses >= g_gpuRetryAfter) {
            g_gpuMisses = 0;
            g_gpuRetryAfter = std::min(g_gpuRetryAfter * 2, 1800);
            ApplyGpu(0);
            ApplyGpu(g_mask);
        }
    }

    bool changed = std::memcmp(r.value, g_last.value, sizeof(r.value)) != 0;
    g_last = r;
    out = r;
    return changed;
}

void Shutdown() { Configure(0); }

}  // namespace hardware
