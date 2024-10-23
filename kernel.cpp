#include <ntddk.h>
#include <wdf.h>

#define BUFFER_SIZE 1024

class ReadWriteDriver {
private:
    char* buffer;
    int buffer_size;

public:
    ReadWriteDriver() {
        buffer_size = BUFFER_SIZE;
        buffer = (char*)ExAllocatePoolWithTag(NonPagedPool, buffer_size, 'rwd');
        if (buffer) {
            RtlZeroMemory(buffer, buffer_size);
        }
    }

    ~ReadWriteDriver() {
        if (buffer) {
            ExFreePool(buffer);
        }
    }

    NTSTATUS Read(PUCHAR user_buffer, size_t len, LARGE_INTEGER* offset, size_t* bytes_read) {
        if (offset->QuadPart >= buffer_size)
            return STATUS_END_OF_FILE;

        if (offset->QuadPart + len > buffer_size)
            len = buffer_size - offset->QuadPart;

        RtlCopyMemory(user_buffer, buffer + offset->QuadPart, len);
        *bytes_read = len;
        offset->QuadPart += len;

        KdPrint(("Read %llu bytes from offset %llu\n", *bytes_read, offset->QuadPart));
        return STATUS_SUCCESS;
    }

    NTSTATUS Write(PUCHAR user_buffer, size_t len, LARGE_INTEGER* offset, size_t* bytes_written) {
        if (offset->QuadPart >= buffer_size)
            return STATUS_DISK_FULL;

        if (offset->QuadPart + len > buffer_size)
            len = buffer_size - offset->QuadPart;

        RtlCopyMemory(buffer + offset->QuadPart, user_buffer, len);
        *bytes_written = len;
        offset->QuadPart += len;

        KdPrint(("Wrote %llu bytes to offset %llu\n", *bytes_written, offset->QuadPart));
        return STATUS_SUCCESS;
    }
};

// Global pointer to driver instance
static ReadWriteDriver* g_rwDriver = nullptr;

NTSTATUS EvtDeviceIoControl(WDFQUEUE Queue, WDFREQUEST Request, size_t OutputBufferLength, size_t InputBufferLength, ULONG IoControlCode) {
    NTSTATUS status = STATUS_SUCCESS;
    size_t bytes_read_or_written = 0;
    LARGE_INTEGER offset = { 0 };

    // Buffer management
    PUCHAR buffer;
    WDFMEMORY inputMemory;
    WDFMEMORY outputMemory;

    // Example of handling a read request
    switch (IoControlCode) {
        case IOCTL_READ:
            // Get user buffer for reading
            status = WdfRequestRetrieveOutputMemory(Request, &outputMemory);
            if (!NT_SUCCESS(status)) {
                KdPrint(("Failed to retrieve output memory for read operation\n"));
                break;
            }

            buffer = (PUCHAR)WdfMemoryGetBuffer(outputMemory, NULL);
            status = g_rwDriver->Read(buffer, OutputBufferLength, &offset, &bytes_read_or_written);
            break;

        case IOCTL_WRITE:
            // Get user buffer for writing
            status = WdfRequestRetrieveInputMemory(Request, &inputMemory);
            if (!NT_SUCCESS(status)) {
                KdPrint(("Failed to retrieve input memory for write operation\n"));
                break;
            }

            buffer = (PUCHAR)WdfMemoryGetBuffer(inputMemory, NULL);
            status = g_rwDriver->Write(buffer, InputBufferLength, &offset, &bytes_read_or_written);
            break;

        default:
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }

    WdfRequestCompleteWithInformation(Request, status, bytes_read_or_written);
    return status;
}

// Driver entry point
extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    NTSTATUS status;
    WDF_DRIVER_CONFIG config;

    WDF_DRIVER_CONFIG_INIT(&config, WDF_NO_EVENT_CALLBACK);

    // Initialize framework driver object
    status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) {
        KdPrint(("Failed to create WDF driver\n"));
        return status;
    }

    // Create instance of driver
    g_rwDriver = new (NonPagedPool, 'rwd') ReadWriteDriver();
    if (!g_rwDriver) {
        KdPrint(("Failed to create ReadWriteDriver instance\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    KdPrint(("Driver successfully loaded\n"));
    return STATUS_SUCCESS;
}
