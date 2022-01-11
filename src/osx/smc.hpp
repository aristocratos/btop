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
#define SMC_KEY_CPU_TEMP "TC0P" // proximity temp?
#define SMC_KEY_CPU_DIODE_TEMP "TC0D" // diode temp?
#define SMC_KEY_CPU_DIE_TEMP "TC0F" // die temp?
#define SMC_KEY_CPU1_TEMP "TC1C"
#define SMC_KEY_CPU2_TEMP "TC2C"  // etc
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
	   public:
		SMCConnection();
		virtual ~SMCConnection();

		long long getTemp(int core);

	   private:
        kern_return_t SMCReadKey(UInt32Char_t key, SMCVal_t *val);
		long long getSMCTemp(char *key);
		kern_return_t SMCCall(int index, SMCKeyData_t *inputStructure, SMCKeyData_t *outputStructure);

		io_connect_t conn;
		kern_return_t result;
		mach_port_t masterPort;
		io_iterator_t iterator;
		io_object_t device;
	};
}  // namespace Cpu
