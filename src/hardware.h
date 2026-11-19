#pragma once
// Hardware readings shown after the batteries on the taskbar strip: CPU, GPU, RAM and video memory usage,
// GPU temperature and the system SSD's temperature. Everything comes from documented Windows interfaces
// that need no driver and no admin rights (performance counters, DXGI, D3DKMT, the storage temperature
// query); NVIDIA's own nvml.dll is tried for the GPU temperature when the graphics driver reports none.
// There is no CPU temperature: Windows only exposes it through a kernel driver.
// Call everything from the UI thread.

namespace hardware {

enum Metric : unsigned { Cpu, Gpu, GpuTemp, Vram, Ram, SsdTemp, MetricCount };
constexpr unsigned Bit(Metric m) { return 1u << m; }
constexpr unsigned kAllMetrics = (1u << MetricCount) - 1;

struct Readings {
    int value[MetricCount] = { -1, -1, -1, -1, -1, -1 };  // percent or degrees Celsius; -1 = not available
};

// Opens what the metrics in `mask` need and closes the rest; cheap when the mask is unchanged.
void Configure(unsigned mask);
// Reads every configured metric. Returns true when a rounded value changed since the previous call.
bool Sample(Readings& out);
// Closes everything (Configure(0)).
void Shutdown();

}  // namespace hardware
