#include "jobwrap/ffi.h"

#include "common/bt_trace.h"
#include "jobwrap/lib.h"

unsigned long bt_jobwrap_create_ceiling_job(unsigned long long per_process_bytes, HANDLE *job) {
    BT_TRACE_ENTER("jobwrap.create_ceiling_job");
    *job = CreateJobObjectW(NULL, NULL);
    if (*job == NULL) {
        unsigned long code = GetLastError();
        BT_TRACE_FAIL("jobwrap.create_ceiling_job", "CreateJobObjectW error %lu", code);
        BT_TRACE_EXIT("jobwrap.create_ceiling_job");
        return code;
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
    ZeroMemory(&info, sizeof info);
    info.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_PROCESS_MEMORY | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    info.ProcessMemoryLimit = (SIZE_T)per_process_bytes;
    if (SetInformationJobObject(*job, JobObjectExtendedLimitInformation, &info, sizeof info) == 0) {
        unsigned long code = GetLastError();
        CloseHandle(*job);
        *job = NULL;
        BT_TRACE_FAIL("jobwrap.create_ceiling_job", "SetInformationJobObject error %lu", code);
        BT_TRACE_EXIT("jobwrap.create_ceiling_job");
        return code;
    }
    BT_TRACE_EXIT("jobwrap.create_ceiling_job");
    return 0;
}

unsigned long bt_jobwrap_spawn_suspended(const char *line, bt_jobwrap_spawned *out) {
    BT_TRACE_ENTER("jobwrap.spawn_suspended");
    wchar_t wide[BT_JOBWRAP_LINE_MAX];
    if (MultiByteToWideChar(CP_UTF8, 0, line, -1, wide, (int)(sizeof wide / sizeof wide[0])) == 0) {
        unsigned long code = GetLastError();
        BT_TRACE_FAIL("jobwrap.spawn_suspended", "widen failed error %lu", code);
        BT_TRACE_EXIT("jobwrap.spawn_suspended");
        return code;
    }
    STARTUPINFOW startup;
    ZeroMemory(&startup, sizeof startup);
    startup.cb = sizeof startup;
    PROCESS_INFORMATION info;
    ZeroMemory(&info, sizeof info);
    if (CreateProcessW(NULL, wide, NULL, NULL, TRUE, CREATE_SUSPENDED, NULL, NULL, &startup,
                       &info) == 0) {
        unsigned long code = GetLastError();
        BT_TRACE_FAIL("jobwrap.spawn_suspended", "CreateProcessW error %lu", code);
        BT_TRACE_EXIT("jobwrap.spawn_suspended");
        return code;
    }
    out->process = info.hProcess;
    out->thread = info.hThread;
    out->process_id = info.dwProcessId;
    BT_TRACE_EXIT("jobwrap.spawn_suspended");
    return 0;
}

unsigned long bt_jobwrap_assign_and_resume(HANDLE job, const bt_jobwrap_spawned *spawned) {
    BT_TRACE_ENTER("jobwrap.assign_and_resume");
    if (AssignProcessToJobObject(job, spawned->process) == 0) {
        unsigned long code = GetLastError();
        BT_TRACE_FAIL("jobwrap.assign_and_resume", "assign error %lu", code);
        BT_TRACE_EXIT("jobwrap.assign_and_resume");
        return code;
    }
    if (ResumeThread(spawned->thread) == 0xFFFFFFFFUL) {
        unsigned long code = GetLastError();
        BT_TRACE_FAIL("jobwrap.assign_and_resume", "resume error %lu", code);
        BT_TRACE_EXIT("jobwrap.assign_and_resume");
        return code;
    }
    BT_TRACE_EXIT("jobwrap.assign_and_resume");
    return 0;
}

unsigned long bt_jobwrap_wait_exit_code(HANDLE process, unsigned int *code) {
    BT_TRACE_ENTER("jobwrap.wait_exit_code");
    if (WaitForSingleObject(process, INFINITE) != WAIT_OBJECT_0) {
        unsigned long failure = GetLastError();
        BT_TRACE_FAIL("jobwrap.wait_exit_code", "wait error %lu", failure);
        BT_TRACE_EXIT("jobwrap.wait_exit_code");
        return failure;
    }
    DWORD raw = 0;
    if (GetExitCodeProcess(process, &raw) == 0) {
        unsigned long failure = GetLastError();
        BT_TRACE_FAIL("jobwrap.wait_exit_code", "exit code error %lu", failure);
        BT_TRACE_EXIT("jobwrap.wait_exit_code");
        return failure;
    }
    *code = (unsigned int)raw;
    BT_TRACE_EXIT("jobwrap.wait_exit_code");
    return 0;
}

unsigned long bt_jobwrap_dotnet_start_ticks(unsigned long long *ticks) {
    BT_TRACE_ENTER("jobwrap.dotnet_start_ticks");
    FILETIME creation;
    FILETIME exit_time;
    FILETIME kernel;
    FILETIME user;
    if (GetProcessTimes(GetCurrentProcess(), &creation, &exit_time, &kernel, &user) == 0) {
        unsigned long code = GetLastError();
        BT_TRACE_FAIL("jobwrap.dotnet_start_ticks", "GetProcessTimes error %lu", code);
        BT_TRACE_EXIT("jobwrap.dotnet_start_ticks");
        return code;
    }
    ULARGE_INTEGER raw;
    raw.LowPart = creation.dwLowDateTime;
    raw.HighPart = creation.dwHighDateTime;
    *ticks = bt_jobwrap_dotnet_ticks(raw.QuadPart);
    BT_TRACE_EXIT("jobwrap.dotnet_start_ticks");
    return 0;
}

void bt_jobwrap_close_handle(HANDLE handle) {
    if (handle != NULL) {
        CloseHandle(handle);
    }
}
