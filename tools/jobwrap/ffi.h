#ifndef BT_JOBWRAP_FFI_H
#define BT_JOBWRAP_FFI_H

#include <windows.h>

typedef struct {
    HANDLE process;
    HANDLE thread;
    DWORD process_id;
} bt_jobwrap_spawned;

unsigned long bt_jobwrap_create_ceiling_job(unsigned long long per_process_bytes, HANDLE *job);
unsigned long bt_jobwrap_spawn_suspended(const wchar_t *line, bt_jobwrap_spawned *out);
unsigned long bt_jobwrap_assign_and_resume(HANDLE job, const bt_jobwrap_spawned *spawned);
unsigned long bt_jobwrap_wait_exit_code(HANDLE process, unsigned int *code);
unsigned long bt_jobwrap_dotnet_start_ticks(unsigned long long *ticks);
void bt_jobwrap_close_handle(HANDLE handle);

#endif
