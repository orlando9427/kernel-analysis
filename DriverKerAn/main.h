#include <ntddk.h>
#include <windef.h>

#ifndef _MAIN_H_
	#define _MAIN_H_

	typedef struct _MODULE_ENTRY {
		LIST_ENTRY Link; // FLink and BLink
		ULONG Unknown[4];
		ULONG ImageBase;
		ULONG EntryPoint;
		ULONG ImageSize;
		UNICODE_STRING DriverPath;
		UNICODE_STRING DriverName;
	} MODULE_ENTRY, *PMODULE_ENTRY;

	typedef struct _DRIVER_ENTRY {
		ULONG ImageBase;
		ULONG EntryPoint;
		ULONG ImageSize;
		WCHAR DriverPath[MAX_PATH];
		WCHAR DriverName[MAX_PATH];
	} DRIVER_ENTRY, *PDRIVER_ENTRY;

	#define DRIV_CODE(Function) CTL_CODE(FILE_DEVICE_UNKNOWN, Function, METHOD_NEITHER, FILE_READ_DATA | FILE_WRITE_DATA)

	#define APIProcess DRIV_CODE(0x00000001)
	#define KerProcess DRIV_CODE(0x00000002)
	#define APIModules DRIV_CODE(0x00000003)
	#define KerModules DRIV_CODE(0x00000004)
	/* #define APIProSize DRIV_CODE(0x00000005)
	#define KerProSize DRIV_CODE(0x00000006)
	#define APIModSize DRIV_CODE(0x00000007)
	#define KerModSize DRIV_CODE(0x00000008) */

	extern "C" {
		NTSYSAPI NTSTATUS NTAPI ZwQuerySystemInformation(IN ULONG SystemInformationClass, IN PVOID SystemInformation, IN ULONG SystemInformationLength, OUT PULONG ReturnLength);
		NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING theRegistryPath);
		VOID DriverUnload(IN PDRIVER_OBJECT DriverObject);
		NTSTATUS DriverIOControl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
		NTSTATUS DriverDispatch(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
	}
#endif