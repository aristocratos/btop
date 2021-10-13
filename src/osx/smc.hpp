#pragma once

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <stdexcept>

#define VERSION "0.01"

#define KERNEL_INDEX_SMC 2

#define SMC_CMD_READ_BYTES 5
#define SMC_CMD_WRITE_BYTES 6
#define SMC_CMD_READ_INDEX 8
#define SMC_CMD_READ_KEYINFO 9
#define SMC_CMD_READ_PLIMIT 11
#define SMC_CMD_READ_VERS 12

#define DATATYPE_FPE2 "fpe2"
#define DATATYPE_UINT8 "ui8 "
#define DATATYPE_UINT16 "ui16"
#define DATATYPE_UINT32 "ui32"
#define DATATYPE_SP78 "sp78"

// key values
#define SMC_KEY_CPU_TEMP "TC0P"
#define SMC_KEY_CPU1_TEMP "TC1C"
#define SMC_KEY_CPU2_TEMP "TC2C" // etc
#define SMC_KEY_FAN0_RPM_CUR "F0Ac"

typedef struct {
    char major;
    char minor;
    char build;
    char reserved[1];
    UInt16 release;
} SMCKeyData_vers_t;

typedef struct {
    UInt16 version;
    UInt16 length;
    UInt32 cpuPLimit;
    UInt32 gpuPLimit;
    UInt32 memPLimit;
} SMCKeyData_pLimitData_t;

typedef struct {
    UInt32 dataSize;
    UInt32 dataType;
    char dataAttributes;
} SMCKeyData_keyInfo_t;

typedef char SMCBytes_t[32];

typedef struct {
    UInt32 key;
    SMCKeyData_vers_t vers;
    SMCKeyData_pLimitData_t pLimitData;
    SMCKeyData_keyInfo_t keyInfo;
    char result;
    char status;
    char data8;
    UInt32 data32;
    SMCBytes_t bytes;
} SMCKeyData_t;

typedef char UInt32Char_t[5];

typedef struct {
    UInt32Char_t key;
    UInt32 dataSize;
    UInt32Char_t dataType;
    SMCBytes_t bytes;
} SMCVal_t;

namespace Cpu {
    class SMCConnection {
		io_connect_t conn;
		kern_return_t result;
		mach_port_t masterPort;
		io_iterator_t iterator;
		io_object_t device;

	   public:
		SMCConnection() {
			IOMasterPort(MACH_PORT_NULL, &masterPort);

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
		// core means physical core in SMC, while in core map it's cpu threads :-/ Only an issue on hackintosh?
		// this means we can only get the T per physical core
		// another issue with the SMC API is that the key is always 4 chars -> what with systems with more than 9 physical cores?
		// no Mac models with more than 18 threads are released, so no problem so far
		// according to VirtualSMC docs (hackintosh fake SMC) the enumeration follows with alphabetic chars - not implemented yet here (nor in VirtualSMC)
		long long getTemp(int core) {
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
		virtual ~SMCConnection() {
			IOServiceClose(conn);
		}
	   private:
		UInt32 _strtoul(char *str, int size, int base) {
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
		void _ultostr(char *str, UInt32 val) {
			str[0] = '\0';
			sprintf(str, "%c%c%c%c",
			        (unsigned int)val >> 24,
			        (unsigned int)val >> 16,
			        (unsigned int)val >> 8,
			        (unsigned int)val);
		}

		kern_return_t SMCCall(int index, SMCKeyData_t *inputStructure, SMCKeyData_t *outputStructure) {
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

		kern_return_t SMCReadKey(UInt32Char_t key, SMCVal_t *val) {
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
	};
}
