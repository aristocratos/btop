/* Copyright 2021 Aristocratos (jakob@qvantnet.com)

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

#include <cstdint>
#include <utility>

namespace IOReport {

	//* Initialize IOReport subscription for CPU stats
	bool init();

	//* Cleanup IOReport resources
	void cleanup();

	//* Get current E-cluster and P-cluster frequencies in MHz
	//* Returns {e_freq_mhz, p_freq_mhz}
	std::pair<uint32_t, uint32_t> get_cpu_frequencies();

	//* Check if IOReport is available and initialized
	bool is_available();

}
