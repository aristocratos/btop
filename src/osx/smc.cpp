#include "smc.hpp"

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
	sprintf(str, "%c%c%c%c",
	        (unsigned int)val >> 24,
	        (unsigned int)val >> 16,
	        (unsigned int)val >> 8,
	        (unsigned int)val);
}

namespace Cpu {

	SMCConnection::SMCConnection() {
		IOMasterPort(kIOMasterPortDefault, &masterPort);

		CFMutableDictionaryRef matchingDictionary = IOServiceMatching("AppleSMC");
		result = IOServiceGetMatchingServices(masterPort, matchingDictionary, &iterator);
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

	// core means physical core in SMC, while in core map it's cpu threads :-/ Only an issue on hackintosh?
	// this means we can only get the T per physical core
	// another issue with the SMC API is that the key is always 4 chars -> what with systems with more than 9 physical cores?
	// no Mac models with more than 18 threads are released, so no problem so far
	// according to VirtualSMC docs (hackintosh fake SMC) the enumeration follows with alphabetic chars - not implemented yet here (nor in VirtualSMC)
	long long SMCConnection::getTemp(int core) {
		SMCVal_t val;
		kern_return_t result;
		char key[] = SMC_KEY_CPU_TEMP;
		if (core >= 0) {
			snprintf(key, 5, "TC%1dc", core);
		}
		result = SMCReadKey(key, &val);
		if (result == kIOReturnSuccess) {
			if (strcmp(val.dataType, DATATYPE_SP78) == 0) {
				// convert sp78 value to temperature
				int intValue = val.bytes[0] * 256 + (unsigned char)val.bytes[1];
				return static_cast<long long>(intValue / 256.0);
			}
		}
		return -1;
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
		                                 // ouputStructure
		                                 outputStructure, &structureOutputSize);
	}

}  // namespace Cpu
