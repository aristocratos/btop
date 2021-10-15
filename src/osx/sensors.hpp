#include <map>

namespace Cpu {
	class ThermalSensors {
	   public:
		std::map<int, double> getSensors();
	};
}  // namespace Cpu
