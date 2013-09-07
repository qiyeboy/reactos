//
// System Headers
//
#include <ntifs.h>
#include <ntndk.h>
#define UNIMPLEMENTED
#define DPRINT1 DbgPrint

//
// Allow Microsoft Extensions
//
#ifdef _MSC_VER
#pragma warning(disable:4201)
#pragma warning(disable:4214)
#pragma warning(disable:4100)
#endif

//
// Node Type Codes for NPFS
//
#define NPFS_NTC_VCB            1
#define NPFS_NTC_ROOT_DCB       2
#define NPFS_NTC_FCB            4
#define NPFS_NTC_CCB            6
#define NPFS_NTC_NONPAGED_CCB   7
#define NPFS_NTC_ROOT_DCB_CCB   8
typedef USHORT NODE_TYPE_CODE, *PNODE_TYPE_CODE;

//
// Data Queue States
//
typedef enum _NP_DATA_QUEUE_STATE
{
    Empty = 2
} NP_DATA_QUEUE_STATE;

//
// An Input or Output Data Queue. Each CCB has two of these.
//
typedef struct _NP_DATA_QUEUE
{
    LIST_ENTRY List;
    ULONG QueueState;
    ULONG BytesInQueue;
    ULONG EntriesInQueue;
    ULONG QuotaUsed;
    ULONG ByteOffset;
    ULONG Quota;
} NP_DATA_QUEUE, *PNP_DATA_QUEUE;

//
// A Wait Queue. Only the VCB has one of these.
//
typedef struct _NP_WAIT_QUEUE
{
    LIST_ENTRY WaitList;
    KSPIN_LOCK WaitLock;
} NP_WAIT_QUEUE, *PNP_WAIT_QUEUE;

//
// The CCB for the Root DCB
//
typedef struct _NP_ROOT_DCB_CCB
{
    NODE_TYPE_CODE NodeType;
    PVOID Unknown;
    ULONG Unknown2;
} NP_ROOT_DCB_CCB, *PNP_ROOT_DCB_FCB;

//
// The header that both FCB and DCB share
//
typedef struct _NP_CB_HEADER
{
    NODE_TYPE_CODE NodeType;
    LIST_ENTRY DcbEntry;
    PVOID ParentDcb;
    ULONG CurrentInstances;
    ULONG OtherCount;
    PSECURITY_DESCRIPTOR SecurityDescriptor;
} NP_CB_HEADER, *PNP_CB_HEADER;

//
// The footer that both FCB and DCB share
//
typedef struct _NP_CB_FOOTER
{
    UNICODE_STRING FullName;
    UNICODE_STRING ShortName;
    UNICODE_PREFIX_TABLE_ENTRY PrefixTableEntry;
} NP_CB_FOOTER;

//
// A Directory Control Block (DCB)
//
typedef struct _NP_DCB
{
    //
    // Common Header
    //
    NP_CB_HEADER;

    //
    // DCB-specific data
    //
    LIST_ENTRY NotifyList;
    LIST_ENTRY NotifyList2;
    LIST_ENTRY FcbList;

    //
    // Common Footer
    //
    NP_CB_FOOTER;
} NP_DCB, *PNP_DCB;

//
// A File Control BLock (FCB)
//
typedef struct _NP_FCB
{
    //
    // Common Header
    //
    NP_CB_HEADER;

    //
    // FCB-specific fields
    //
    ULONG MaximumInstances;
    USHORT NamedPipeConfiguration;
    USHORT NamedPipeType;
    LARGE_INTEGER Timeout;
    LIST_ENTRY CcbList;

    //
    // Common Footer
    //
    NP_CB_FOOTER;
} NP_FCB, *PNP_FCB;

//
// The nonpaged portion of the CCB
//
typedef struct _NP_NONPAGED_CCB
{
    NODE_TYPE_CODE NodeType;
    PVOID EventBufferClient;
    PVOID EventBufferServer;
    ERESOURCE Lock;
} NP_NONPAGED_CCB, *PNP_NONPAGED_CCB;

//
// A Client Control Block (CCB)
//
typedef struct _NP_CCB
{
    NODE_TYPE_CODE NodeType;
    UCHAR NamedPipeState;
    UCHAR ClientReadMode:1;
    UCHAR ClientCompletionMode:1;
    UCHAR ServerReadMode:1;
    UCHAR ClientReservedFlags:6;
    UCHAR ServerCompletionMode:1;
    UCHAR ServerReservedFlags:6;
    SECURITY_QUALITY_OF_SERVICE ClientQos;
    LIST_ENTRY CcbEntry;
    PNP_FCB Fcb;
    PFILE_OBJECT ClientFileObject;
    PFILE_OBJECT ServerFileObject;
    PEPROCESS Process;
    PVOID ClientSession;
    PNP_NONPAGED_CCB NonPagedCcb;
    NP_DATA_QUEUE InQueue;
    NP_DATA_QUEUE OutQueue;
    PSECURITY_CLIENT_CONTEXT ClientContext;
    LIST_ENTRY IrpList;
} NP_CCB, *PNP_CCB;

//
// A Volume Control Block (VCB)
//
typedef struct _NP_VCB
{
    NODE_TYPE_CODE NodeType;
    ULONG ReferenceCount;
    PNP_DCB RootDcb;
    UNICODE_PREFIX_TABLE PrefixTable;
    ERESOURCE Lock;
    RTL_GENERIC_TABLE EventTable;
    NP_WAIT_QUEUE WaitQueue;
} NP_VCB, *PNP_VCB;

extern PNP_VCB NpVcb;

VOID
NTAPI
NpInitializeWaitQueue(IN PNP_WAIT_QUEUE WaitQueue);


NTSTATUS
NTAPI
NpUninitializeDataQueue(IN PNP_DATA_QUEUE DataQueue);

NTSTATUS
NTAPI
NpInitializeDataQueue(IN PNP_DATA_QUEUE DataQueue,
                      IN ULONG Quota);

NTSTATUS
NTAPI
NpCreateCcb(IN PNP_FCB Fcb,
            IN PFILE_OBJECT FileObject,
            IN UCHAR State,
            IN UCHAR ReadMode,
            IN UCHAR CompletionMode,
            IN ULONG InQuota,
            IN ULONG OutQuota,
            OUT PNP_CCB* NewCcb);

NTSTATUS
NTAPI
NpCreateFcb(IN PNP_DCB Dcb,
            IN PUNICODE_STRING PipeName,
            IN ULONG MaximumInstances,
            IN LARGE_INTEGER Timeout,
            IN USHORT NamedPipeConfiguration,
            IN USHORT NamedPipeType,
            OUT PNP_FCB *NewFcb);

NTSTATUS
NTAPI
NpCreateRootDcb(VOID);

NTSTATUS
NTAPI
NpCreateRootDcbCcb(IN PNP_ROOT_DCB_FCB* NewRootCcb);

VOID
NTAPI
NpInitializeVcb(VOID);

VOID
NTAPI
NpDeleteCcb(IN PNP_CCB Ccb,
            IN PLIST_ENTRY ListEntry);

VOID
NTAPI
NpDeleteFcb(IN PNP_FCB Fcb,
            IN PLIST_ENTRY ListEntry);

NTSTATUS
NTAPI
NpFsdCreateNamedPipe(IN PDEVICE_OBJECT DeviceObject,
                     IN PIRP Irp);

NTSTATUS
NTAPI
NpFsdCreate(IN PDEVICE_OBJECT DeviceObject,
            IN PIRP Irp);

NTSTATUS
NTAPI
NpSetConnectedPipeState(IN PNP_CCB Ccb,
                        IN PFILE_OBJECT FileObject,
                        IN PLIST_ENTRY List);

VOID
NTAPI
NpUninitializeSecurity(IN PNP_CCB Ccb);

NTSTATUS
NTAPI
NpInitializeSecurity(IN PNP_CCB Ccb,
                     IN PSECURITY_QUALITY_OF_SERVICE SecurityQos,
                     IN PETHREAD Thread);

VOID
NTAPI
NpSetFileObject(IN PFILE_OBJECT FileObject,
                IN PVOID PrimaryContext,
                IN PVOID Ccb,
                IN BOOLEAN ServerSide);

NODE_TYPE_CODE
NTAPI
NpDecodeFileObject(IN PFILE_OBJECT FileObject,
                   OUT PVOID* PrimaryContext OPTIONAL,
                   OUT PNP_CCB* Ccb,
                   OUT PBOOLEAN ServerSide OPTIONAL);

PNP_FCB
NTAPI
NpFindPrefix(IN PUNICODE_STRING Name,
             IN ULONG CaseInsensitiveIndex,
             IN PUNICODE_STRING Prefix);

NTSTATUS
NTAPI
NpFindRelativePrefix(IN PNP_DCB Dcb,
                     IN PUNICODE_STRING Name,
                     IN ULONG CaseInsensitiveIndex,
                     IN PUNICODE_STRING Prefix,
                     OUT PNP_FCB *FoundFcb);

VOID
NTAPI
NpCheckForNotify(IN PNP_DCB Dcb,
                 IN BOOLEAN SecondList,
                 IN PLIST_ENTRY List);

NTSTATUS
NTAPI
NpCancelWaiter(IN PNP_WAIT_QUEUE WaitQueue,
               IN PUNICODE_STRING PipeName,
               IN NTSTATUS Status,
               IN PLIST_ENTRY ListEntry);

