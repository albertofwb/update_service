#include <windows.h> 
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>
#include "utils.h"
#include "xadl_utils.h"
#include "acl_control.h"

#define PIPE_TIMEOUT 5000
#define BUFSIZE 4096

typedef struct
{
    OVERLAPPED oOverlap;
    HANDLE hPipeInst;
    TCHAR chRequest[BUFSIZE];
    DWORD cbRead;
    TCHAR chReply[BUFSIZE];
    DWORD cbToWrite;
} PIPEINST, * LPPIPEINST;

VOID DisconnectAndClose(LPPIPEINST);
BOOL CreateAndConnectInstance(LPOVERLAPPED);
BOOL ConnectToNewClient(HANDLE, LPOVERLAPPED);
VOID GetAnswerToRequest(LPPIPEINST);

VOID WINAPI CompletedWriteRoutine(DWORD, DWORD, LPOVERLAPPED);
VOID WINAPI CompletedReadRoutine(DWORD, DWORD, LPOVERLAPPED);

HANDLE hPipe;

DWORD WINAPI StartNamePipeServer(LPVOID)
{
    HANDLE hConnectEvent;
    OVERLAPPED oConnect;
    LPPIPEINST lpPipeInst;
    DWORD dwWait, cbRet;
    BOOL fSuccess, fPendingIO;

    // Create one event object for the connect operation. 
    hConnectEvent = CreateEvent(
        NULL,    // default security attribute
        TRUE,    // manual reset event 
        TRUE,    // initial state = signaled 
        NULL);   // unnamed event object 

    if (hConnectEvent == NULL)
    {
        printf("CreateEvent failed with %d.\n", GetLastError());
        return 0;
    }

    oConnect.hEvent = hConnectEvent;

    fPendingIO = CreateAndConnectInstance(&oConnect);
    while (1)
    {
        // Wait for a client to connect, or for a read or write 
        // operation to be completed, which causes a completion 
        // routine to be queued for execution. 

        dwWait = WaitForSingleObjectEx(
            hConnectEvent,  // event object to wait for 
            INFINITE,       // waits indefinitely 
            TRUE);          // alertable wait enabled 

        switch (dwWait)
        {
            // The wait conditions are satisfied by a completed connect 
            // operation. 
        case 0:
            // If an operation is pending, get the result of the 
            // connect operation. 

            if (fPendingIO)
            {
                fSuccess = GetOverlappedResult(
                    hPipe,     // pipe handle 
                    &oConnect, // OVERLAPPED structure 
                    &cbRet,    // bytes transferred 
                    FALSE);    // does not wait 
                if (!fSuccess)
                {
                    printf("ConnectNamedPipe (%d)\n", GetLastError());
                    return 0;
                }
            }

            // Allocate storage for this instance. 
            lpPipeInst = (LPPIPEINST)GlobalAlloc(GPTR, sizeof(PIPEINST));
            if (lpPipeInst == NULL)
            {
                printf("GlobalAlloc failed (%d)\n", GetLastError());
                return 0;
            }

            lpPipeInst->hPipeInst = hPipe;

            lpPipeInst->cbToWrite = 0;
            CompletedWriteRoutine(0, 0, (LPOVERLAPPED)lpPipeInst);

            // Create new pipe instance for the next client. 

            fPendingIO = CreateAndConnectInstance(
                &oConnect);
            break;

        case WAIT_IO_COMPLETION:
            break;

            // An error occurred in the wait function. 

        default:
        {
            printf("WaitForSingleObjectEx (%d)\n", GetLastError());
            return 0;
        }
        }
    }
    return 0;
}


VOID WINAPI CompletedWriteRoutine(DWORD dwErr, DWORD cbWritten,
    LPOVERLAPPED lpOverLap)
{
    LPPIPEINST lpPipeInst;
    BOOL fRead = FALSE;

    // lpOverlap points to storage for this instance. 

    lpPipeInst = (LPPIPEINST)lpOverLap;

    ZeroMemory(lpPipeInst->chRequest, sizeof(lpPipeInst->chRequest));
    if ((dwErr == 0) && (cbWritten == lpPipeInst->cbToWrite))
        fRead = ReadFileEx(
            lpPipeInst->hPipeInst,
            lpPipeInst->chRequest,
            BUFSIZE * sizeof(TCHAR),
            (LPOVERLAPPED)lpPipeInst,
            (LPOVERLAPPED_COMPLETION_ROUTINE)CompletedReadRoutine);

    // Disconnect if an error occurred. 

    if (!fRead)
        DisconnectAndClose(lpPipeInst);
}

// CompletedReadRoutine(DWORD, DWORD, LPOVERLAPPED) 
// This routine is called as an I/O completion routine after reading 
// a request from the client. It gets data and writes it to the pipe. 

VOID WINAPI CompletedReadRoutine(DWORD dwErr, DWORD cbBytesRead,
    LPOVERLAPPED lpOverLap)
{
    LPPIPEINST lpPipeInst;
    BOOL fWrite = FALSE;

    // lpOverlap points to storage for this instance. 

    lpPipeInst = (LPPIPEINST)lpOverLap;

    // The read operation has finished, so write a response (if no 
    // error occurred). 

    if ((dwErr == 0) && (cbBytesRead != 0))
    {
        GetAnswerToRequest(lpPipeInst);

        fWrite = WriteFileEx(
            lpPipeInst->hPipeInst,
            lpPipeInst->chReply,
            lpPipeInst->cbToWrite,
            (LPOVERLAPPED)lpPipeInst,
            (LPOVERLAPPED_COMPLETION_ROUTINE)CompletedWriteRoutine);
    }

    // Disconnect if an error occurred. 

    if (!fWrite)
        DisconnectAndClose(lpPipeInst);
}

// DisconnectAndClose(LPPIPEINST) 
// This routine is called when an error occurs or the client closes 
// its handle to the pipe. 

VOID DisconnectAndClose(LPPIPEINST lpPipeInst)
{
    // Disconnect the pipe instance. 

    if (!DisconnectNamedPipe(lpPipeInst->hPipeInst))
    {
        printf("DisconnectNamedPipe failed with %d.\n", GetLastError());
    }

    // Close the handle to the pipe instance. 

    CloseHandle(lpPipeInst->hPipeInst);

    // Release the storage for the pipe instance. 

    if (lpPipeInst != NULL)
        GlobalFree(lpPipeInst);
}



BOOL CreateAndConnectInstance(LPOVERLAPPED lpoOverlap)
{
    LPCTSTR lpszPipename = TEXT("\\\\.\\pipe\\audit_service");

    SECURITY_ATTRIBUTES     sa;
    BuildSecurityAttributes(sa);

    hPipe = CreateNamedPipe(
        lpszPipename,             // pipe name 
        PIPE_ACCESS_DUPLEX |      // read/write access 
        FILE_FLAG_OVERLAPPED,     // overlapped mode 
        PIPE_TYPE_MESSAGE |       // message-type pipe 
        PIPE_READMODE_MESSAGE |   // message read mode 
        PIPE_WAIT,                // blocking mode 
        PIPE_UNLIMITED_INSTANCES, // unlimited instances 
        BUFSIZE * sizeof(TCHAR),    // output buffer size 
        BUFSIZE * sizeof(TCHAR),    // input buffer size 
        PIPE_TIMEOUT,             // client time-out 
        &sa);                    // default security attributes
    if (hPipe == INVALID_HANDLE_VALUE)
    {
        printf("CreateNamedPipe failed with %d.\n", GetLastError());
        return 0;
    }

    return ConnectToNewClient(hPipe, lpoOverlap);
}

BOOL ConnectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo)
{
    BOOL fConnected, fPendingIO = FALSE;

    // Start an overlapped connection for this pipe instance. 
    fConnected = ConnectNamedPipe(hPipe, lpo);

    // Overlapped ConnectNamedPipe should return zero. 
    if (fConnected)
    {
        printf("ConnectNamedPipe failed with %d.\n", GetLastError());
        return 0;
    }

    switch (GetLastError())
    {
        // The overlapped connection in progress. 
    case ERROR_IO_PENDING:
        fPendingIO = TRUE;
        break;

        // Client is already connected, so signal an event. 

    case ERROR_PIPE_CONNECTED:
        if (SetEvent(lpo->hEvent))
            break;

        // If an error occurs during the connect operation... 
    default:
    {
        printf("ConnectNamedPipe failed with %d.\n", GetLastError());
        return 0;
    }
    }
    return fPendingIO;
}


VOID GetAnswerToRequest(LPPIPEINST pipe)
{
    LPTSTR auth = pipe->chRequest;
    LPCTSTR FLAG = _T("XADL_AUDIT");
    CONST DWORD FLAG_SIZE = _tcslen(FLAG);
    _tprintf_s(TEXT("row input %s\n"), auth);

    if (_tcsncmp(auth, FLAG, FLAG_SIZE)) {
        _tprintf_s(TEXT("get invalid %s\n"), auth);
        StringCchCopy(pipe->chReply, BUFSIZE, _T("KO"));
    }
    else {
        TCHAR* cmd = auth + FLAG_SIZE;
        if (_tcslen(cmd) == 0) {
            _tprintf_s(_T("skip create process as cmd size is 0\n"));
        }
        else if (!_tcscmp(cmd, _T("uninstall client"))) {
            UninstallClient();
            StringCchCopy(pipe->chReply, BUFSIZE, _T("OK"));
        }
        else {
            _tprintf_s(TEXT("invoke process %s\n"), cmd);
            RunNewProcess(cmd);
            StringCchCopy(pipe->chReply, BUFSIZE, _T("OK"));
        }
    }
    pipe->cbToWrite = (lstrlen(pipe->chReply) + 1) * sizeof(TCHAR);
}