#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define MODE_CHILD "--child"
#define CAP_BYTES 100663296ULL
#define ALLOC_BYTES 1048576ULL
#define TOUCH_STRIDE 4096ULL
#define BLOCKS 640ULL

static int child_mode(void) {
    fprintf(stderr, "child: allocating\n");
    fflush(stderr);
    for (unsigned long long at = 0; at < BLOCKS; at++) {
        unsigned char *block = VirtualAlloc(NULL, (SIZE_T)ALLOC_BYTES, MEM_RESERVE | MEM_COMMIT,
                                            PAGE_READWRITE);
        if (block == NULL) {
            fprintf(stderr, "child: VirtualAlloc failed at block %llu error %lu\n", at,
                    GetLastError());
            fflush(stderr);
            return 1;
        }
        for (unsigned long long off = 0; off < ALLOC_BYTES; off += TOUCH_STRIDE) {
            block[off] = 1;
        }
    }
    fprintf(stderr, "child: allocated and touched %llu MiB\n", BLOCKS);
    fflush(stderr);
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], MODE_CHILD) == 0) {
        return child_mode();
    }
    HANDLE job = CreateJobObjectW(NULL, NULL);
    if (job == NULL) {
        fprintf(stderr, "job create failed %lu\n", GetLastError());
        return 1;
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
    ZeroMemory(&info, sizeof info);
    info.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_PROCESS_MEMORY | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    info.ProcessMemoryLimit = (SIZE_T)CAP_BYTES;
    if (SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof info) == 0) {
        fprintf(stderr, "set info failed %lu\n", GetLastError());
        return 1;
    }
    fprintf(stderr, "job limit set to %llu bytes\n", CAP_BYTES);
    char command[1024];
    GetModuleFileNameA(NULL, command, sizeof command);
    char wide_tail[128];
    snprintf(wide_tail, sizeof wide_tail, " %s", MODE_CHILD);
    strcat(command, wide_tail);
    STARTUPINFOA startup;
    ZeroMemory(&startup, sizeof startup);
    startup.cb = sizeof startup;
    PROCESS_INFORMATION proc;
    ZeroMemory(&proc, sizeof proc);
    if (CreateProcessA(NULL, command, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &startup,
                       &proc) == 0) {
        fprintf(stderr, "spawn failed %lu\n", GetLastError());
        return 1;
    }
    if (AssignProcessToJobObject(job, proc.hProcess) == 0) {
        fprintf(stderr, "assign failed %lu\n", GetLastError());
        return 1;
    }
    if (ResumeThread(proc.hThread) == 0xFFFFFFFFUL) {
        fprintf(stderr, "resume failed %lu\n", GetLastError());
        return 1;
    }
    CloseHandle(proc.hThread);
    fprintf(stderr, "child pid %lu resumed\n", proc.dwProcessId);
    WaitForSingleObject(proc.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(proc.hProcess, &code);
    fprintf(stderr, "child exit %lu\n", code);
    CloseHandle(proc.hProcess);
    CloseHandle(job);
    return 0;
}
