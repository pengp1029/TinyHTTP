#include <windows.h>
#include<stdio.h>

int main() {
    // Path to the CGI script (color.cgi) and any necessary parameters
    char* scriptPath = "E:/c++/tinyhttpd/src/htdocs/color.cgi";  // Update with actual path
    char* parameters = "color=red";  // Update with desired parameters

    // Construct the command line with the script path and parameters
    char commandLine[1024];
    sprintf(commandLine, "%s?%s", scriptPath, parameters);

    // CreateProcess variables
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    // Initialize the STARTUPINFO structure
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    // Create the process
    if (CreateProcess(
        NULL,           // No module name (use command line)
        scriptPath,    // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        0,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory
        &si,            // Pointer to STARTUPINFO structure
        &pi)            // Pointer to PROCESS_INFORMATION structure
    )
    {
        // Wait until child process exits.
        WaitForSingleObject(pi.hProcess, INFINITE);
        printf("yes!");
        // Close process and thread handles.
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else
    {
        // Handle error
        DWORD error = GetLastError();
        printf("no!");
        // ...
    }

    return 0;
}