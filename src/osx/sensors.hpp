#include <robin_hood.h>

using robin_hood::unordered_flat_map;

namespace Cpu {
	class ThermalSensors {
	   public:
		unordered_flat_map<int, double> getSensors();
	};
}  // namespace Cpu
