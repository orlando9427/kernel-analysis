#include <ntddk.h>
#include <main.h>

const WCHAR Device[] = L"\\device\\HidedDriver";
const WCHAR Link[] = L"\\??\\HidedDriver";

UNICODE_STRING Dev, Lnk;

NTSTATUS DriverDispatch(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	Irp->IoStatus.Status = STATUS_SUCCESS;   
    Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath)
{
	NTSTATUS Status;

	RtlInitUnicodeString(&Dev, Device);
	RtlInitUnicodeString(&Lnk, Link);

	DriverObject->DriverUnload = DriverUnload;
	DriverObject->MajorFunction[IRP_MJ_SHUTDOWN]        =   
	DriverObject->MajorFunction[IRP_MJ_CREATE]          =   
	DriverObject->MajorFunction[IRP_MJ_CLOSE]           = DriverDispatch;  
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]  = DriverIOControl;

	Status = IoCreateDevice(DriverObject, 0, &Dev, FILE_DEVICE_UNKNOWN, 0, 0, &DriverObject->DeviceObject);

	if (NT_SUCCESS(Status)) {
		Status = IoCreateSymbolicLink(&Lnk, &Dev);
		DbgPrint("Dispositivo creado.");
		if (!NT_SUCCESS(Status)){
			IoDeleteDevice(DriverObject->DeviceObject);
			DbgPrint("No se pudo crear el link simbolico.");
			//DbgPrint("Error 0x
		} else
			DbgPrint("Conexion simbolica creada.");
	} else {
		DbgPrint("No se pudo crear el dispositivo.");
		return STATUS_UNSUCCESSFUL;
	}

	DbgPrint("Driver cargado.");
	return Status;
}

NTSTATUS DriverIOControl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	PIO_STACK_LOCATION Stack;
	KPROCESSOR_MODE accessMode;
	NTSTATUS Status = STATUS_SUCCESS;
	PMODULE_ENTRY Module;
	PVOID inputBuffer;
	PVOID outputBuffer;
	ULONG inputLength;
	ULONG IoControlCode;
	ULONG outputLength;
	ULONG Ret = 0;

	DbgPrint("Leyendo informacion para recibir datos.");
	Irp->IoStatus.Status = STATUS_SUCCESS;   
    Irp->IoStatus.Information = 0; 
	Stack = IoGetCurrentIrpStackLocation(Irp);
	inputLength = Stack->Parameters.DeviceIoControl.InputBufferLength;
	IoControlCode = Stack->Parameters.DeviceIoControl.IoControlCode;
	outputLength = Stack->Parameters.DeviceIoControl.OutputBufferLength;
	accessMode = Irp->RequestorMode;
	inputBuffer = Stack->Parameters.DeviceIoControl.Type3InputBuffer;
	outputBuffer = Irp->UserBuffer;

	if (outputLength == 0){
		DbgPrint("No se recibio buffer");
		return STATUS_INVALID_BUFFER_SIZE;
	}
	DbgPrint("Entrando con IOCode: %8X", IoControlCode);
	switch(IoControlCode){
		/*case APIProSize:
			{
				Status = ZwQuerySystemInformation(5, NULL, 0, (PULONG)outputBuffer);
			}*/

		case APIProcess:
			{
				//Se llama la API en MK para tener todos los privilegios
				Status = ZwQuerySystemInformation(5, outputBuffer, outputLength, &Ret);
				break;
			}

		case APIModules:
			{
				//Se llama desde MK listar los drivers para tener privilegios completos
				Status = ZwQuerySystemInformation(11, outputBuffer, outputLength, &Ret);
				break;
			}

		case KerModules:
			{
				//Por medio de ASM se obtiene la PsLoadedModulesList para recorrer la lista de
				//forma manual y así detectar saltos innesperados --Proximamente--
				DbgPrint("Listando Modulos...");
				__asm
				{
					mov eax, dword ptr fs:[0x1C]	 // KPCR
					mov eax,[eax+0x34]	 //KPCR -> KdVersionBlock
					mov eax,[eax+0x70]	 //KDDEBUGGER_DATA32 -> PsLoadedModulesList
					mov Module,eax
				}
				PVOID First = 0;//Primer modulo
				ULONG Drivers = 0;//Numero de Drivers "cargados"
				First = (PVOID)Module;//Guardamos el primero para no hacer un bucle
				//infinito
				Module = (PMODULE_ENTRY)Module->Link.Flink;
				while (((ULONG)Module->Link.Flink != 0) && (Module->Link.Flink != First)){
					Drivers++; //Simplemente se cuentan los drivers
					DbgPrint("Driver name %ws, ImageBase: %08X, ImageSize: %08X, Memory: %08X", Module->DriverName.Buffer, Module->ImageBase, Module->ImageSize, Module);
					Module = (PMODULE_ENTRY)Module->Link.Flink; //Siguiente driver!
				}
				Ret = Drivers * sizeof(DRIVER_ENTRY);
				if (Ret > outputLength){ //Si el buffer es muy pequeño, piden solo el tamaño
					Status = STATUS_INFO_LENGTH_MISMATCH; //Pedimos un buffer mas grande
				} else { //Si se tiene un buffer de buen tamaño para la petición
					PDRIVER_ENTRY Driver = (PDRIVER_ENTRY)outputBuffer; //Hacemos una array con el
					//con nuestra propia estructura con la info de los drivers
					DbgPrint("Enviando estructuras.");
					Module = (PMODULE_ENTRY)First;
					Module = (PMODULE_ENTRY)Module->Link.Flink;
					Drivers = 0; //Volvemos el contador del array a cero y recorremos la lista otra vez
					while (((ULONG)Module->Link.Flink != 0) && (Module->Link.Flink != First)){
						Driver[Drivers].ImageSize = Module->ImageSize;
						Driver[Drivers].ImageBase = Module->ImageBase;
						Driver[Drivers].EntryPoint = Module->EntryPoint;
						RtlCopyMemory(Driver[Drivers].DriverName, Module->DriverName.Buffer, Module->DriverName.Length);
						RtlCopyMemory(Driver[Drivers].DriverPath, Module->DriverPath.Buffer, Module->DriverPath.Length);
						//Se copia la info a la estructura
						Drivers++;
						//DbgPrint("Driver name %ws, ImageBase: %08x, Memory: %08X", Module->DriverName.Buffer, Module->ImageBase, Module);
						Module = (PMODULE_ENTRY)Module->Link.Flink;
					}
				}
				//break;
			}
	}

	if (Status == STATUS_INFO_LENGTH_MISMATCH){ //Este caso se trato para los errores de APIs y nuestros
		//por eso usamos ese status.
		DbgPrint("Tamaño ocupado: %d", Ret);
		*(PULONG)outputBuffer = Ret; //Retornamos el buffer necesario
		DbgPrint("Variable escrita: %d", *(PULONG)outputBuffer);
		Status = STATUS_SUCCESS; //Y el status de buffer pequeño
	}
	IoCompleteRequest(Irp, IO_NO_INCREMENT); //Lo de ley, marcar como leido el IRP y el Status
	DbgPrint("Regresando pedido.");
	return Status;
}

VOID DriverUnload(IN PDRIVER_OBJECT DriverObject)
{
	IoDeleteDevice(DriverObject->DeviceObject);
	DbgPrint("Eliminando dispositivo.");
	IoDeleteSymbolicLink(&Lnk);
	DbgPrint("Eliminando conexion simbolica.");

	DbgPrint("Driver descargado.");
	return;
}