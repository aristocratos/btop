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

#include "smc.hpp"
#include "intel_mac.hpp"

#include <cmath>
#include <cstring>

static constexpr size_t MaxIndexCount = sizeof("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ") - 1;
static constexpr const char *KeyIndexes = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static UInt32 _strtoul(char *str, int size, int base) {
	UInt32 total = 0;
	int i;

	for (i = 0; i < size; i++) {
		if (base == 16) {
			total += str[i] << (size - 1 - i) * 8;
		} else {
			total += (unsigned char)(str[i] << (size - 1 - i) * 8);
		}
	}
	return total;
}

static void _ultostr(char *str, UInt32 val) {
	str[0] = '\0';
	snprintf(str, 5, "%c%c%c%c",
			(unsigned int)val >> 24,
			(unsigned int)val >> 16,
			(unsigned int)val >> 8,
			(unsigned int)val);
}

namespace Cpu {

	static auto bytes_to_uint16(const SMCVal_t& val) -> uint16_t {
		return static_cast<uint16_t>(
			(static_cast<uint16_t>(static_cast<unsigned char>(val.bytes[0])) << 8)
			| static_cast<unsigned char>(val.bytes[1])
		);
	}

	static auto fixed_point_fraction_bits(const char type[5]) -> std::optional<int> {
		if ((type[0] != 's' and type[0] != 'f') or type[1] != 'p') return std::nullopt;
		const auto digit = type[3];
		if (digit >= '0' and digit <= '9') return digit - '0';
		if (digit >= 'a' and digit <= 'f') return digit - 'a' + 10;
		return std::nullopt;
	}

	SMCConnection::SMCConnection() {
		CFMutableDictionaryRef matchingDictionary = IOServiceMatching("AppleSMC");
		result = IOServiceGetMatchingServices(0, matchingDictionary, &iterator);
		if (result != kIOReturnSuccess) {
			throw std::runtime_error("failed to get AppleSMC");
		}

		device = IOIteratorNext(iterator);
		IOObjectRelease(iterator);
		if (device == 0) {
			throw std::runtime_error("failed to get SMC device");
		}

		result = IOServiceOpen(device, mach_task_self(), 0, &conn);
		IOObjectRelease(device);
		if (result != kIOReturnSuccess) {
			throw std::runtime_error("failed to get SMC connection");
		}
	}
	SMCConnection::~SMCConnection() {
		IOServiceClose(conn);
	}

	std::optional<double> SMCConnection::getValue(const char *key) {
		if (key == nullptr or strlen(key) != 4) return std::nullopt;

		UInt32Char_t mutable_key{};
		memcpy(mutable_key, key, 4);
		SMCVal_t val{};
		if (SMCReadKey(mutable_key, &val) != kIOReturnSuccess or val.dataSize == 0)
			return std::nullopt;

		double value = 0;
		if (strcmp(val.dataType, DATATYPE_FLT) == 0 and val.dataSize >= sizeof(float)) {
			float raw = 0;
			memcpy(&raw, val.bytes, sizeof(raw));
			value = raw;
		}
		else if (strcmp(val.dataType, DATATYPE_FPE2) == 0 and val.dataSize >= 2) {
			value = static_cast<double>(bytes_to_uint16(val)) / 4.0;
		}
		else if (const auto fraction_bits = fixed_point_fraction_bits(val.dataType);
		         fraction_bits.has_value() and val.dataSize >= 2) {
			const auto divisor = static_cast<double>(uint64_t{1} << fraction_bits.value());
			value = val.dataType[0] == 's'
				? static_cast<double>(static_cast<int16_t>(bytes_to_uint16(val))) / divisor
				: static_cast<double>(bytes_to_uint16(val)) / divisor;
		}
		else if (strcmp(val.dataType, DATATYPE_UINT8) == 0) {
			value = static_cast<unsigned char>(val.bytes[0]);
		}
		else if (strcmp(val.dataType, DATATYPE_UINT16) == 0 and val.dataSize >= 2) {
			value = bytes_to_uint16(val);
		}
		else if (strcmp(val.dataType, DATATYPE_UINT32) == 0 and val.dataSize >= 4) {
			value = static_cast<double>(
				(static_cast<uint32_t>(static_cast<unsigned char>(val.bytes[0])) << 24)
				| (static_cast<uint32_t>(static_cast<unsigned char>(val.bytes[1])) << 16)
				| (static_cast<uint32_t>(static_cast<unsigned char>(val.bytes[2])) << 8)
				| static_cast<unsigned char>(val.bytes[3])
			);
		}
		else {
			return std::nullopt;
		}

		return std::isfinite(value) ? std::optional<double>{value} : std::nullopt;
	}

	long long SMCConnection::getSMCTemp(const char *key) {
		const auto value = getValue(key);
		return value.has_value() ? static_cast<long long>(std::llround(value.value())) : -1;
	}

	// core means physical core in SMC, while in core map it's cpu threads :-/ Only an issue on hackintosh?
	// this means we can only get the T per physical core
	// another issue with the SMC API is that the key is always 4 chars -> what with systems with more than 9 physical cores?
	// no Mac models with more than 18 threads are released, so no problem so far
	// according to VirtualSMC docs (hackintosh fake SMC) the enumeration follows with alphabetic chars - not implemented yet here (nor in VirtualSMC)
	long long SMCConnection::getTemp(int core) {
		if (core < 0) {
			// TC0P is CPU proximity, not package temperature. Prefer the
			// available PECI/die value and fail closed if none is published.
			for (const auto key : ::IntelMac::cpu_package_temperature_keys) {
				const auto result = getSMCTemp(key.data());
				if (result != -1) return result;
			}
			return -1;
		}

		if ((size_t)core > MaxIndexCount) return -1;
		char key[5]{};
		snprintf(key, sizeof(key), "TC%1cc", KeyIndexes[core]);
		long long result = getSMCTemp(key);
		if (result == -1) {
			// try again with C
			snprintf(key, sizeof(key), "TC%1cC", KeyIndexes[core]);
			result = getSMCTemp(key);
		}
		return result;
	}

	kern_return_t SMCConnection::SMCReadKey(UInt32Char_t key, SMCVal_t *val) {
		kern_return_t result;
		SMCKeyData_t inputStructure;
		SMCKeyData_t outputStructure;

		memset(&inputStructure, 0, sizeof(SMCKeyData_t));
		memset(&outputStructure, 0, sizeof(SMCKeyData_t));
		memset(val, 0, sizeof(SMCVal_t));

		inputStructure.key = _strtoul(key, 4, 16);
		inputStructure.data8 = SMC_CMD_READ_KEYINFO;

		result = SMCCall(KERNEL_INDEX_SMC, &inputStructure, &outputStructure);
		if (result != kIOReturnSuccess)
			return result;

		val->dataSize = outputStructure.keyInfo.dataSize;
		_ultostr(val->dataType, outputStructure.keyInfo.dataType);
		inputStructure.keyInfo.dataSize = val->dataSize;
		inputStructure.data8 = SMC_CMD_READ_BYTES;

		result = SMCCall(KERNEL_INDEX_SMC, &inputStructure, &outputStructure);
		if (result != kIOReturnSuccess)
			return result;

		memcpy(val->bytes, outputStructure.bytes, sizeof(outputStructure.bytes));

		return kIOReturnSuccess;
	}

	kern_return_t SMCConnection::SMCCall(int index, SMCKeyData_t *inputStructure, SMCKeyData_t *outputStructure) {
		size_t structureInputSize;
		size_t structureOutputSize;

		structureInputSize = sizeof(SMCKeyData_t);
		structureOutputSize = sizeof(SMCKeyData_t);

		return IOConnectCallStructMethod(conn, index,
										 // inputStructure
										 inputStructure, structureInputSize,
										 // outputStructure
										 outputStructure, &structureOutputSize);
	}

}  // namespace Cpu
