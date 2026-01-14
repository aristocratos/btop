# btop++ Comprehensive Changes Report

## Executive Summary

This report documents all modifications made to the btop++ codebase in the `gpu-apple-silicon-nosudo` branch. The changes span across **GPU monitoring**, **power panel**, **UI improvements**, **stability fixes**, and **performance optimizations**.

```mermaid
pie title Changes by Category
    "GPU Enhancements" : 35
    "Power Panel" : 20
    "UI/UX Fixes" : 15
    "Stability Fixes" : 20
    "Performance Optimizations" : 10
```

---

## Table of Contents

1. [GPU Monitoring Enhancements](#1-gpu-monitoring-enhancements)
2. [Power Panel Implementation](#2-power-panel-implementation)
3. [UI/UX Improvements](#3-uiux-improvements)
4. [Stability Improvements](#4-stability-improvements)
5. [Performance Optimizations](#5-performance-optimizations)
6. [Platform-Specific Fixes](#6-platform-specific-fixes)
7. [Architecture Overview](#7-architecture-overview)
8. [Files Modified Summary](#8-files-modified-summary)

---

## 1. GPU Monitoring Enhancements

### 1.1 Apple Silicon GPU Support (No Sudo Required)

**Commit**: `2961c7a` - *Add support for GPU stats on Apple Silicon*

Implemented comprehensive GPU monitoring for Apple Silicon Macs (M1/M2/M3/M4) without requiring sudo privileges, using the same approach as the `macmon` tool.

```mermaid
flowchart TB
    subgraph "IOReport Framework Integration"
        A["libIOReport.dylib"] --> B["Dynamic Loading"]
        B --> C["IOReportCreateSubscription"]
        C --> D["IOReportCreateSamples"]
        D --> E["IOReportCreateSamplesDelta"]
    end

    subgraph "Metrics Collected"
        E --> F["GPU Utilization %"]
        E --> G["GPU Frequency MHz"]
        E --> H["GPU Power Watts"]
        E --> I["GPU Temperature"]
        E --> J["ANE Power Watts"]
        E --> K["CPU Power Watts"]
    end

    subgraph "Display Integration"
        F --> L["GPU Panel"]
        G --> L
        H --> M["Pwr Panel"]
        I --> M
        J --> M
        K --> M
    end
```

#### Key Components

| Component | File | Description |
|-----------|------|-------------|
| `AppleSiliconGpu` class | `src/osx/apple_silicon_gpu.cpp` | Main GPU collector using IOReport |
| `AppleSiliconGpuMetrics` struct | `src/osx/apple_silicon_gpu.hpp` | Metrics container |
| IOReport function pointers | `src/osx/apple_silicon_gpu.hpp` | Dynamic library loading |

#### Metrics Structure

```cpp
struct AppleSiliconGpuMetrics {
    double gpu_usage_percent = 0.0;      // GPU utilization 0-100
    double gpu_freq_mhz = 0.0;           // Current GPU frequency in MHz
    double gpu_freq_max_mhz = 0.0;       // Maximum GPU frequency in MHz
    double gpu_power_watts = 0.0;        // GPU power consumption in watts
    double gpu_temp_celsius = 0.0;       // GPU temperature in Celsius
    double ane_power_watts = 0.0;        // ANE power consumption in watts
    double ane_activity_cmds = 0.0;      // ANE activity in commands/second
    double cpu_power_watts = 0.0;        // CPU power consumption in watts
};
```

### 1.2 Per-Process GPU Monitoring

**Commit**: `c0ed287` - *Comprehensive GPU process monitoring and fan RPM display*

Added GPU utilization tracking per-process in the process view.

```mermaid
flowchart LR
    subgraph "Process View Enhancements"
        A["Process List"] --> B["Gpu% Column"]
        B --> C["Braille Mini Graphs"]
        B --> D["Color Gradient"]
    end

    subgraph "Sorting Options"
        E["Sort by GPU Usage"]
        F["Quartile-based Scaling"]
    end

    subgraph "Detailed Panel"
        G["GPU Usage Graph"]
        H["Real-time Metrics"]
    end

    A --> E
    A --> F
    A --> G
    A --> H
```

#### Features

- **Gpu% Column**: Shows per-process GPU utilization percentage
- **Braille Mini Graphs**: Visual representation in process list
- **GPU-based Sorting**: Sort processes by GPU usage
- **Quartile Scaling**: Better visualization of GPU usage distribution
- **Color Gradient**: GPU utilization reflected in color intensity

### 1.3 GPU/ANE Panel Layout Improvements

**Commit**: `0805137` - *Improve GPU/ANE panel layout with conditional PWR display*

```mermaid
flowchart TD
    A{PWR Panel Visible?}

    A -->|Yes| B["GPU/ANE bars 8 chars wider"]
    B --> C["No inline power display"]

    A -->|No| D["Show power/temp inline"]
    D --> E["ANE power braille graph"]
    D --> F["Compact display mode"]

    G["Dynamic Height Adjustment"]
    A --> G
```

---

## 2. Power Panel Implementation

### 2.1 Comprehensive Power Monitoring

**Commit**: `7543bf3` - *Add comprehensive power monitoring panel for Apple Silicon*

Created a new dedicated Power (Pwr) panel for Apple Silicon Macs.

```mermaid
flowchart TB
    subgraph "Power Panel Components"
        A["CPU Power Graph"]
        B["GPU Power Graph"]
        C["ANE Power Graph"]
        D["Temperature Display"]
        E["Fan RPM Display"]
    end

    subgraph "Core Count Display"
        F["E-core Count"]
        G["P-core Count"]
        H["GPU Core Count"]
        I["ANE Core Count"]
    end

    subgraph "Power Metrics"
        J["Current Power (W)"]
        K["Average Power (W)"]
        L["Peak Power (W)"]
    end

    A --> J
    A --> K
    A --> L
    B --> J
    B --> K
    B --> L
    C --> J
    C --> K
    C --> L
```

### 2.2 Power Panel Data Structures

```cpp
namespace Pwr {
    extern string box;
    extern int x, y, width, height, min_width, min_height;
    extern bool shown, redraw;

    // Thread-safe mutex for power history
    extern std::mutex history_mutex;

    // Power history deques for braille graphs (in mW)
    extern deque<long long> cpu_pwr_history;
    extern deque<long long> gpu_pwr_history;
    extern deque<long long> ane_pwr_history;

    // Max observed power for auto-scaling (in mW)
    extern long long cpu_pwr_max, gpu_pwr_max, ane_pwr_max;

    // Thread-safe accessors
    deque<long long> get_cpu_history();
    deque<long long> get_gpu_history();
    deque<long long> get_ane_history();
    void update_history(long long cpu_mw, long long gpu_mw,
                        long long ane_mw, size_t max_size = 100);
}
```

### 2.3 Shared Power Variables

```cpp
namespace Shared {
    // E-core/P-core counts
    extern long eCoreCount, pCoreCount;

    // GPU and ANE core counts
    extern long gpuCoreCount, aneCoreCount;

    // Atomic power metrics for thread safety
    extern atomic<double> cpuPower, gpuPower, anePower;       // Current
    extern atomic<double> cpuPowerAvg, gpuPowerAvg, anePowerAvg;  // Average
    extern atomic<double> cpuPowerPeak, gpuPowerPeak, anePowerPeak; // Peak

    // ANE activity (commands/second)
    extern atomic<double> aneActivity;

    // Temperatures
    extern atomic<long long> cpuTemp, gpuTemp;

    // Fan metrics
    extern atomic<long long> fanRpm;
    extern atomic<int> fanCount;
}
```

### 2.4 Fan RPM Support

**Commit**: `c0ed287` - Added fan RPM display in power panel

```mermaid
flowchart LR
    subgraph "SMC Interface"
        A["SMCConnection"]
        B["getFanCount()"]
        C["getFanRPM(int fan)"]
    end

    subgraph "Data Types Supported"
        D["flt (float) - Apple Silicon"]
        E["fpe2 (fixed) - Intel Macs"]
    end

    subgraph "Display"
        F["Fan RPM after power"]
        G["Average when multiple fans"]
    end

    A --> B
    A --> C
    B --> F
    C --> F
    D --> C
    E --> C
```

#### SMC Methods Added

```cpp
class SMCConnection {
public:
    SMCConnection();
    ~SMCConnection();

    long long getTemp(int core);  // Existing
    int getFanCount();            // NEW: Returns number of fans
    int getFanRPM(int fan);       // NEW: Returns RPM for specified fan

private:
    kern_return_t SMCReadKey(const UInt32Char_t key, SMCVal_t* val);
    kern_return_t SMCCall(int index, SMCKeyData_t* input, SMCKeyData_t* output);
};
```

### 2.5 Split GPU/ANE Graph View

Toggle key `6` enables split view showing GPU and ANE activity separately:

```mermaid
flowchart TD
    subgraph "Combined View (Default)"
        A["GPU Usage Graph"]
    end

    subgraph "Split View (Key '6')"
        B["GPU Usage Graph (Upper)"]
        C["ANE Activity Graph (Lower)"]
    end

    D{Key '6' Pressed}
    D -->|Toggle| A
    D -->|Toggle| B
    D -->|Toggle| C
```

---

## 3. UI/UX Improvements

### 3.1 Disk Panel Enhancements

**Commit**: `5c9ec46` - *Improve disk panel space utilization and navigation*

```mermaid
flowchart TB
    subgraph "Space Calculation"
        A["Per-disk space check"]
        B["3 lines: no IO stats"]
        C["4 lines: with IO stats"]
        D["Disk images auto-skip IO"]
    end

    subgraph "Navigation"
        E["Tab: toggle selection"]
        F["Arrow keys: navigate"]
        G["Mouse scroll: scroll list"]
        H["Enter: detailed info"]
    end

    subgraph "Visual Improvements"
        I["Scroll indicators (up/down)"]
        J["Ghost content prevention"]
        K["Side-by-side indicators"]
    end

    A --> B
    A --> C
    A --> D
```

#### Disk Panel Features

| Feature | Description |
|---------|-------------|
| Smart space calculation | Each disk checked for actual space requirement |
| Disk image detection | DMG/ISO/IMG files skip IO stats automatically |
| Ghost content prevention | Clear unused rows after rendering |
| Scroll indicators | Positioned side by side on bottom border |

#### Navigation Enhancements

| Key | Action |
|-----|--------|
| `Tab` | Toggle disk selection mode |
| `Up/Down` | Navigate between disks |
| `Mouse Scroll` | Scroll through disk list |
| `Enter` | Show detailed disk info |

### 3.2 Memory Panel Improvements

- Consistent scroll indicators with disk panel
- Similar navigation behavior across panels
- Improved border rendering

### 3.3 Process Panel GPU Column

```mermaid
flowchart LR
    subgraph "Process Columns"
        A["PID"]
        B["Name"]
        C["CPU%"]
        D["Mem%"]
        E["Gpu% (NEW)"]
    end

    subgraph "GPU% Features"
        F["Percentage value"]
        G["Braille mini graph"]
        H["Color gradient"]
    end

    E --> F
    E --> G
    E --> H
```

---

## 4. Stability Improvements

### 4.1 Safe Utility Functions

**Commit**: `3b538cc` - *Comprehensive stability improvements with safe utilities*

```mermaid
flowchart TB
    subgraph "Input Sources"
        A["/proc filesystem"]
        B["sysctl interface"]
        C["IOKit/CoreFoundation"]
        D["User configuration"]
    end

    subgraph "Safe Conversion Layer"
        E["stoi_safe()"]
        F["stol_safe()"]
        G["stoll_safe()"]
        H["stoull_safe()"]
        I["stod_safe()"]
    end

    subgraph "Safe Operation Layer"
        J["safe_div()"]
        K["safe_mod()"]
        L["safe_at()"]
        M["safe_sub()"]
    end

    A --> E
    A --> F
    B --> G
    B --> H
    C --> I
```

#### Safe Conversion Functions

```cpp
// Safe string to integer conversions using std::from_chars
inline int stoi_safe(const std::string_view str, int fallback = 0) noexcept;
inline long stol_safe(const std::string_view str, long fallback = 0) noexcept;
inline long long stoll_safe(const std::string_view str, long long fallback = 0) noexcept;
inline unsigned long long stoull_safe(const std::string_view str,
                                       unsigned long long fallback = 0) noexcept;
inline double stod_safe(const std::string_view str, double fallback = 0.0) noexcept;
```

#### Safe Arithmetic Operations

```cpp
// Safe division - returns fallback on division by zero
template<typename T>
constexpr T safe_div(T numerator, T denominator, T fallback = T{}) noexcept;

// Safe modulo - returns fallback on division by zero
template<typename T>
constexpr T safe_mod(T numerator, T denominator, T fallback = T{}) noexcept;

// Safe vector access with bounds checking
template<typename T>
const T& safe_at(const vector<T>& vec, size_t index, const T& fallback) noexcept;

// Safe subtraction for unsigned types - prevents underflow
template<typename T>
constexpr T safe_sub(T a, T b) noexcept;
```

### 4.2 Thread Safety Improvements

**Commits**: `c53e033`, `38b7aa5` - *Thread safety and deadlock prevention*

```mermaid
flowchart TD
    subgraph "Before"
        A1["atomic_lock"] --> B1["No memory barriers"]
        B1 --> C1["Data races possible"]
    end

    subgraph "After"
        A2["atomic_lock"] --> B2["acquire/release semantics"]
        B2 --> C2["Proper synchronization"]
    end

    style C1 fill:#ff6b6b
    style C2 fill:#51cf66
```

#### Memory Ordering Fixes

```cpp
// Before: potential data race
while (not this->atom.compare_exchange_weak(this->not_true, true)) {
    this->not_true = false;
}

// After: proper memory ordering
while (not this->atom.compare_exchange_weak(this->not_true, true,
        std::memory_order_acquire, std::memory_order_relaxed)) {
    this->not_true = false;
    busy_wait();
}
```

#### Runner Thread Deadlock Prevention

**Commit**: `38b7aa5` - Fixed potential deadlock in runner thread restart mechanism

```cpp
namespace Runner {
    extern atomic<bool> should_terminate;  // Cooperative termination flag
    extern pthread_mutex_t mtx;            // Thread health monitoring mutex
    void thread_trigger();                 // Signal semaphore to wake runner
}
```

### 4.3 Signal Handler Safety

```mermaid
sequenceDiagram
    participant K as Kernel
    participant S as Signal Handler
    participant G as Global State
    participant I as Input System

    K->>S: SIGWINCH (terminal resize)
    S->>G: Set Global::resized = true (atomic)
    S->>I: Input::interrupt() (write to pipe)
    S-->>K: Return immediately

    Note over S: Only async-signal-safe operations!

    K->>S: SIGTERM (termination)
    S->>G: Set Global::should_quit = true (atomic)
    S->>G: Set Runner::stopping = true (atomic)
    S->>I: Input::interrupt()
    S-->>K: Return immediately
```

### 4.4 Resource Leak Fixes

**Commit**: `79d862d` - *Address multiple resource leaks and improve error handling*

| Resource | Issue | Fix |
|----------|-------|-----|
| IOHIDEventSystemClientRef | Not released on error paths | RAII wrapper / CFRelease on all paths |
| IORegistryEntry | Not released on all paths | Ensure IOObjectRelease on all exits |
| CoreFoundation objects | Early returns skip CFRelease | Scope guards for cleanup |

### 4.5 Error Handling Improvements

```mermaid
flowchart TD
    A["Parse system data"] --> B{Use safe conversion}
    B --> C["Valid result"]
    B --> D["Fallback value"]
    C --> E["Continue processing"]
    D --> F{Is fallback acceptable?}
    F -->|Yes| E
    F -->|No| G["Log warning"]
    G --> H["Use default/skip"]
```

---

## 5. Performance Optimizations

### 5.1 O(n) Lookup Optimization

**Commit**: `2403ed3` - *Comprehensive stability improvements for long-running sessions*

```mermaid
flowchart LR
    subgraph "Before O(n^2)"
        A1["For each graph"] --> B1["Linear search in process list"]
        B1 --> C1["30,000 comparisons per cleanup"]
    end

    subgraph "After O(n)"
        A2["Build PID set O(n)"] --> B2["Check membership O(1)"]
        B2 --> C2["Linear time cleanup"]
    end

    style C1 fill:#ff6b6b
    style C2 fill:#51cf66
```

#### Graph Cleanup Optimization

```cpp
// Before: O(n*m) complexity
std::erase_if(p_graphs, [&](const auto& pair) {
    return rng::find(plist, pair.first, &proc_info::pid) == plist.end();
});

// After: O(n+m) complexity using unordered_set
std::unordered_set<pid_t> live_pids;
for (const auto& p : plist) live_pids.insert(p.pid);
std::erase_if(p_graphs, [&](const auto& pair) {
    return not live_pids.contains(pair.first);
});
```

### 5.2 BSD Collector Optimization

Changed `vector<size_t> found` to `unordered_set<size_t>` in BSD collectors for O(1) lookup:

| Collector | File | Optimization |
|-----------|------|--------------|
| FreeBSD | `src/freebsd/btop_collect.cpp` | `unordered_set` for PID lookup |
| NetBSD | `src/netbsd/btop_collect.cpp` | `unordered_set` for PID lookup |
| OpenBSD | `src/openbsd/btop_collect.cpp` | `unordered_set` for PID lookup |

---

## 6. Platform-Specific Fixes

### 6.1 FreeBSD Fixes

- **strtok NULL checks**: Added null pointer checks before using strtok results
- **Safe parsing**: Converted all stoi/stol calls to safe variants

### 6.2 NetBSD/OpenBSD Fixes

- **malloc NULL checks**: Verify malloc return values
- **Division guards**: Prevent division by zero in battery percentage calculations
- **sysctl error handling**: Complete error handling for sysctl calls

### 6.3 macOS Fixes

- **SMC Apple Silicon support**: Handle 'flt ' data type for fan speed on Apple Silicon
- **Intel compatibility**: Maintain backward compatibility with fpe2 format
- **P-state parsing**: Safe parsing with validation

### 6.4 Linux Fixes

- **CPU core map parsing**: Standardized parsing with safe conversions
- **Intel GPU Top**: Fixed resource leaks in GPU monitoring

---

## 7. Architecture Overview

### 7.1 High-Level Architecture

```mermaid
flowchart TB
    subgraph "Data Collection Layer"
        A["CPU Collector"]
        B["Memory Collector"]
        C["GPU Collector (NEW)"]
        D["Network Collector"]
        E["Process Collector"]
        F["Power Collector (NEW)"]
    end

    subgraph "Shared Data Layer"
        G["Shared::cpuPower/gpuPower/anePower"]
        H["Shared::cpuTemp/gpuTemp"]
        I["Shared::fanRpm/fanCount"]
        J["Pwr::history_mutex"]
        K["Pwr::cpu/gpu/ane_pwr_history"]
    end

    subgraph "Drawing Layer"
        L["CPU Panel"]
        M["Memory Panel"]
        N["GPU Panel"]
        O["Network Panel"]
        P["Process Panel"]
        Q["Pwr Panel (NEW)"]
    end

    C --> G
    C --> H
    F --> G
    F --> I
    F --> K

    G --> L
    G --> Q
    H --> Q
    I --> Q
    K --> Q
```

### 7.2 Apple Silicon GPU Data Flow

```mermaid
sequenceDiagram
    participant IOReport as IOReport Library
    participant GPU as AppleSiliconGpu
    participant Shared as Shared Namespace
    participant Pwr as Pwr Panel
    participant Draw as Draw Thread

    loop Collection Cycle
        GPU->>IOReport: IOReportCreateSamples()
        IOReport-->>GPU: Current sample
        GPU->>IOReport: IOReportCreateSamplesDelta()
        IOReport-->>GPU: Delta metrics
        GPU->>GPU: parse_channels()
        GPU->>Shared: Update atomic values
        GPU->>Pwr: update_history() (with mutex)
    end

    loop Draw Cycle
        Draw->>Shared: Read atomic values
        Draw->>Pwr: get_*_history() (thread-safe copy)
        Draw->>Draw: Render panels
    end
```

### 7.3 Thread Safety Model

```mermaid
flowchart TB
    subgraph "Collector Thread"
        A["apple_silicon_gpu.collect()"]
        B["Update atomic values"]
        C["Pwr::update_history()"]
    end

    subgraph "Synchronization"
        D["std::atomic for scalars"]
        E["std::mutex for history deques"]
    end

    subgraph "Draw Thread"
        F["Read atomic values"]
        G["Pwr::get_*_history()"]
        H["Render panels"]
    end

    A --> B
    B --> D
    A --> C
    C --> E

    D --> F
    E --> G
    F --> H
    G --> H
```

---

## 8. Files Modified Summary

### 8.1 New Files Added

| File | Purpose |
|------|---------|
| `src/osx/apple_silicon_gpu.cpp` | Apple Silicon GPU collector implementation |
| `src/osx/apple_silicon_gpu.hpp` | GPU metrics and class declarations |
| `docs/safe-utilities.md` | Safe utilities documentation |
| `docs/stability-improvements.md` | Stability improvements documentation |
| `tests/ane_*.swift` | ANE test utilities |
| `.gitlab-ci.yml` | GitLab CI/CD pipeline |
| `CLAUDE.md` | Development guide |

### 8.2 Modified Files by Category

#### Core Files

| File | Changes |
|------|---------|
| `src/btop.cpp` | Signal handling, thread management, Apple GPU integration |
| `src/btop_config.cpp` | Disk selection config, safe conversions |
| `src/btop_config.hpp` | New config options |
| `src/btop_draw.cpp` | Pwr panel, GPU%, disk/mem panel improvements |
| `src/btop_input.cpp` | Disk navigation, keyboard handlers |
| `src/btop_menu.cpp` | Disk info menu |
| `src/btop_shared.cpp` | Shared GPU percent maps, power metrics |
| `src/btop_shared.hpp` | Pwr namespace, atomic variables, core counts |
| `src/btop_tools.cpp` | Safe conversions, UTF-8 handling |
| `src/btop_tools.hpp` | Safe utility functions |
| `src/btop_theme.cpp` | Safe RGB parsing |

#### macOS-Specific

| File | Changes |
|------|---------|
| `src/osx/btop_collect.cpp` | GPU integration, disk image detection, fan RPM |
| `src/osx/smc.cpp` | Fan count/RPM methods, Apple Silicon float support |
| `src/osx/smc.hpp` | SMCConnection class updates |
| `src/osx/sensors.cpp` | Temperature collection updates |

#### Platform Stubs

| File | Changes |
|------|---------|
| `src/freebsd/btop_collect.cpp` | Pwr stubs, safe conversions, strtok fixes |
| `src/netbsd/btop_collect.cpp` | Pwr stubs, safe conversions, malloc checks |
| `src/openbsd/btop_collect.cpp` | Pwr stubs, safe conversions, division guards |
| `src/linux/btop_collect.cpp` | Pwr stubs, safe conversions |

### 8.3 Lines of Code Summary

```mermaid
graph LR
    subgraph "Lines Added/Modified"
        A["apple_silicon_gpu: +1100"]
        B["btop_draw: +800"]
        C["btop_collect.cpp: +600"]
        D["btop_tools: +210"]
        E["btop_shared: +100"]
        F["smc: +80"]
    end
```

| Category | Lines Added | Lines Modified |
|----------|-------------|----------------|
| GPU Support | ~1100 | ~200 |
| Power Panel | ~400 | ~300 |
| UI Improvements | ~800 | ~400 |
| Safe Utilities | ~210 | ~100 |
| Platform Fixes | ~150 | ~300 |
| **Total** | **~2660** | **~1300** |

---

## Commit History

| Commit | Type | Description |
|--------|------|-------------|
| `c0ed287` | feat | GPU process monitoring and fan RPM display |
| `3fd4bcf` | fix | Critical stability improvements across all platforms |
| `3b538cc` | feat | Comprehensive stability improvements with safe utilities |
| `c53e033` | fix | Thread safety, performance, and best practices |
| `2403ed3` | fix | Stability improvements for long-running sessions |
| `79d862d` | fix | Resource leaks and error handling |
| `38b7aa5` | fix | Deadlock prevention in runner thread |
| `0805137` | feat | GPU/ANE panel layout with conditional PWR display |
| `5c9ec46` | feat | Disk panel space utilization and navigation |
| `7543bf3` | feat | Comprehensive power monitoring panel |
| `2961c7a` | feat | Apple Silicon GPU support (no sudo) |

---

## Testing Recommendations

### Functional Testing

- [ ] GPU metrics accuracy on M1/M2/M3/M4 chips
- [ ] Power panel graph rendering
- [ ] Fan RPM display accuracy
- [ ] Disk panel navigation with mouse/keyboard
- [ ] Process list GPU% column

### Stability Testing

- [ ] Long-running sessions (24+ hours)
- [ ] High process count scenarios (5000+ processes)
- [ ] Rapid terminal resize events
- [ ] Memory pressure testing

### Platform Testing

- [ ] macOS Apple Silicon (M1/M2/M3/M4)
- [ ] macOS Intel (backward compatibility)
- [ ] Linux x86_64
- [ ] FreeBSD
- [ ] NetBSD
- [ ] OpenBSD

---

*Report generated: January 2026*
*Branch: `gpu-apple-silicon-nosudo`*
*Based on commits: `2961c7a` to `c0ed287`*
