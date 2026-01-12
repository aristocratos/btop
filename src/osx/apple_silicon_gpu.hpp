/* Copyright 2021 Aristocratos (jakob@qvantnet.com)
   Copyright 2025 btop contributors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

	   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

indent = tab
tab-size = 4
*/

#pragma once

#include <Availability.h>

// Only available on macOS 10.15.4+ (where Apple Silicon support exists)
#if __MAC_OS_X_VERSION_MIN_REQUIRED > 101504

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <cstdint>
#include <string>
#include <vector>

namespace Gpu {

	//? IOReport function pointer types
	using IOReportSubscriptionRef = CFTypeRef;

	//? IOReport function declarations (loaded dynamically from libIOReport.dylib)
	extern CFDictionaryRef (*IOReportCopyChannelsInGroup)(CFStringRef, CFStringRef, uint64_t, uint64_t, uint64_t);
	extern void (*IOReportMergeChannels)(CFDictionaryRef, CFDictionaryRef, CFTypeRef);
	extern IOReportSubscriptionRef (*IOReportCreateSubscription)(void*, CFMutableDictionaryRef, CFMutableDictionaryRef*, uint64_t, CFTypeRef);
	extern CFDictionaryRef (*IOReportCreateSamples)(IOReportSubscriptionRef, CFMutableDictionaryRef, CFTypeRef);
	extern CFDictionaryRef (*IOReportCreateSamplesDelta)(CFDictionaryRef, CFDictionaryRef, CFTypeRef);
	extern CFStringRef (*IOReportChannelGetGroup)(CFDictionaryRef);
	extern CFStringRef (*IOReportChannelGetSubGroup)(CFDictionaryRef);
	extern CFStringRef (*IOReportChannelGetChannelName)(CFDictionaryRef);
	extern CFStringRef (*IOReportChannelGetUnitLabel)(CFDictionaryRef);
	extern int32_t (*IOReportStateGetCount)(CFDictionaryRef);
	extern CFStringRef (*IOReportStateGetNameForIndex)(CFDictionaryRef, int32_t);
	extern int64_t (*IOReportStateGetResidency)(CFDictionaryRef, int32_t);
	extern int64_t (*IOReportSimpleGetIntegerValue)(CFDictionaryRef, int32_t);

	//? Apple Silicon GPU metrics container
	struct AppleSiliconGpuMetrics {
		double gpu_usage_percent = 0.0;      // GPU utilization 0-100
		double gpu_freq_mhz = 0.0;           // Current GPU frequency in MHz
		double gpu_freq_max_mhz = 0.0;       // Maximum GPU frequency in MHz
		double gpu_power_watts = 0.0;        // GPU power consumption in watts
		double gpu_temp_celsius = 0.0;       // GPU temperature in Celsius

		// ANE (Neural Engine) metrics
		double ane_power_watts = 0.0;        // ANE power consumption in watts
		double ane_activity_cmds = 0.0;      // ANE activity in commands/second

		// CPU power metrics
		double cpu_power_watts = 0.0;        // CPU power consumption in watts
	};

	//? Apple Silicon GPU collector class
	class AppleSiliconGpu {
	public:
		AppleSiliconGpu();
		~AppleSiliconGpu();

		//? Initialize the GPU monitoring (load IOReport library, create subscriptions)
		bool init();

		//? Shutdown and cleanup
		void shutdown();

		//? Check if Apple Silicon GPU is available
		bool is_available() const { return initialized and is_apple_silicon; }

		//? Collect GPU metrics (call periodically)
		AppleSiliconGpuMetrics collect();

		//? Get GPU name
		std::string get_name() const { return gpu_name; }

		//? Get maximum GPU frequency from DVFS table
		double get_max_freq_mhz() const { return max_gpu_freq_mhz; }

	private:
		bool initialized = false;
		bool is_apple_silicon = false;
		std::string gpu_name;
		double max_gpu_freq_mhz = 0.0;
		std::vector<double> gpu_freq_table;  // Available GPU frequencies from DVFS

		void* ioreport_lib_handle = nullptr;
		IOReportSubscriptionRef subscription = nullptr;
		CFMutableDictionaryRef channels = nullptr;
		CFDictionaryRef prev_sample = nullptr;
		uint64_t prev_sample_time = 0;

		// ANE tracking
		int64_t prev_ane_commands = 0;       // Previous ANECPU Commands Sent value

		// Power averaging
		static constexpr int POWER_AVG_SAMPLES = 60;  // Average over ~60 seconds
		std::vector<double> cpu_power_history;
		std::vector<double> gpu_power_history;
		std::vector<double> ane_power_history;
		int power_history_idx = 0;

		//? Load IOReport library and resolve function pointers
		bool load_ioreport_library();

		//? Setup IOReport subscriptions for GPU stats and energy model
		bool setup_subscriptions();

		//? Read GPU frequency table from IORegistry (voltage-states9)
		bool read_gpu_freq_table();

		//? Get GPU temperature via IOHIDSensors or SMC
		double get_gpu_temperature();

		//? Get CPU temperature via IOHIDSensors
		double get_cpu_temperature();

		//? Parse channel data from IOReport delta sample
		void parse_channels(CFDictionaryRef delta, double elapsed_seconds,
		                    double& out_freq_mhz, double& out_usage_percent,
		                    double& out_gpu_power_watts, double& out_temp_celsius,
		                    double& out_cpu_power_watts, double& out_ane_power_watts,
		                    double& out_ane_activity_cmds);
	};

	//? Global Apple Silicon GPU instance
	extern AppleSiliconGpu apple_silicon_gpu;

}  // namespace Gpu

#endif  // __MAC_OS_X_VERSION_MIN_REQUIRED > 101504
