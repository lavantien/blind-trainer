use std::ffi::c_void;

type Handle = *mut c_void;

const NULL_HANDLE: Handle = std::ptr::null_mut();
const JOB_CLASS_EXTENDED_LIMITS: i32 = 9;
const CREATE_SUSPENDED: u32 = 4;
const LIMIT_PROCESS_MEMORY: u32 = 0x100;
const LIMIT_KILL_ON_JOB_CLOSE: u32 = 0x2000;

#[repr(C)]
struct JobBasicLimitInformation {
    per_process_user_time_limit: i64,
    per_job_user_time_limit: i64,
    limit_flags: u32,
    minimum_working_set_size: usize,
    maximum_working_set_size: usize,
    active_process_limit: u32,
    affinity: usize,
    priority_class: u32,
    scheduling_class: u32,
}

#[repr(C)]
struct IoCounters {
    read_operation_count: u64,
    write_operation_count: u64,
    other_operation_count: u64,
    read_transfer_count: u64,
    write_transfer_count: u64,
    other_transfer_count: u64,
}

#[repr(C)]
struct JobExtendedLimitInformation {
    basic: JobBasicLimitInformation,
    io: IoCounters,
    process_memory_limit: usize,
    job_memory_limit: usize,
    peak_process_memory_used: usize,
    peak_job_memory_used: usize,
}

#[repr(C)]
struct StartupInfoW {
    cb: u32,
    reserved: *mut u16,
    desktop: *mut u16,
    title: *mut u16,
    x: u32,
    y: u32,
    x_size: u32,
    y_size: u32,
    x_count_chars: u32,
    y_count_chars: u32,
    fill_attribute: u32,
    flags: u32,
    show_window: u16,
    reserved2: u16,
    reserved2_bytes: *mut u8,
    standard_input: *mut c_void,
    standard_output: *mut c_void,
    standard_error: *mut c_void,
}

#[repr(C)]
struct ProcessInformation {
    process: Handle,
    thread: Handle,
    process_id: u32,
    thread_id: u32,
}

const _: () = assert!(std::mem::size_of::<JobBasicLimitInformation>() == 64);
const _: () = assert!(std::mem::size_of::<JobExtendedLimitInformation>() == 144);

#[link(name = "kernel32")]
unsafe extern "system" {
    fn CreateJobObjectW(attributes: *mut c_void, name: *const u16) -> Handle;
    fn SetInformationJobObject(
        job: Handle,
        info_class: i32,
        info: *mut c_void,
        info_size: u32,
    ) -> i32;
    fn AssignProcessToJobObject(job: Handle, process: Handle) -> i32;
    fn CreateProcessW(
        application: *const u16,
        command_line: *mut u16,
        process_attributes: *mut c_void,
        thread_attributes: *mut c_void,
        inherit_handles: i32,
        creation_flags: u32,
        environment: *mut c_void,
        current_directory: *const u16,
        startup_info: *mut StartupInfoW,
        process_information: *mut ProcessInformation,
    ) -> i32;
    fn ResumeThread(thread: Handle) -> u32;
    fn WaitForSingleObject(handle: Handle, milliseconds: u32) -> u32;
    fn GetExitCodeProcess(process: Handle, exit_code: *mut u32) -> i32;
    fn GetProcessTimes(
        process: Handle,
        creation: *mut u64,
        exit_time: *mut u64,
        kernel: *mut u64,
        user: *mut u64,
    ) -> i32;
    fn GetCurrentProcess() -> Handle;
    fn CloseHandle(handle: Handle) -> i32;
    fn GetLastError() -> u32;
}

pub struct SpawnedProcess {
    pub process: Handle,
    pub thread: Handle,
    pub process_id: u32,
}

pub fn to_wide(raw: &str) -> Vec<u16> {
    raw.encode_utf16().chain(std::iter::once(0)).collect()
}

fn last_error() -> u32 {
    unsafe { GetLastError() }
}

pub fn create_ceiling_job(per_process_bytes: usize) -> Result<Handle, u32> {
    let job = unsafe { CreateJobObjectW(std::ptr::null_mut(), std::ptr::null()) };
    if job == NULL_HANDLE {
        return Err(last_error());
    }
    let mut info = unsafe { std::mem::zeroed::<JobExtendedLimitInformation>() };
    info.basic.limit_flags = LIMIT_PROCESS_MEMORY | LIMIT_KILL_ON_JOB_CLOSE;
    info.process_memory_limit = per_process_bytes;
    let set = unsafe {
        SetInformationJobObject(
            job,
            JOB_CLASS_EXTENDED_LIMITS,
            &mut info as *mut JobExtendedLimitInformation as *mut c_void,
            std::mem::size_of::<JobExtendedLimitInformation>() as u32,
        )
    };
    if set == 0 {
        let code = last_error();
        close(job);
        return Err(code);
    }
    Ok(job)
}

pub fn spawn_suspended(line: &str) -> Result<SpawnedProcess, u32> {
    let mut wide = to_wide(line);
    let mut startup = unsafe { std::mem::zeroed::<StartupInfoW>() };
    startup.cb = std::mem::size_of::<StartupInfoW>() as u32;
    let mut info = unsafe { std::mem::zeroed::<ProcessInformation>() };
    let created = unsafe {
        CreateProcessW(
            std::ptr::null(),
            wide.as_mut_ptr(),
            std::ptr::null_mut(),
            std::ptr::null_mut(),
            1,
            CREATE_SUSPENDED,
            std::ptr::null_mut(),
            std::ptr::null(),
            &mut startup,
            &mut info,
        )
    };
    if created == 0 {
        return Err(last_error());
    }
    Ok(SpawnedProcess {
        process: info.process,
        thread: info.thread,
        process_id: info.process_id,
    })
}

pub fn assign_and_resume(job: Handle, spawned: &SpawnedProcess) -> Result<(), u32> {
    let assigned = unsafe { AssignProcessToJobObject(job, spawned.process) };
    if assigned == 0 {
        return Err(last_error());
    }
    let resumed = unsafe { ResumeThread(spawned.thread) };
    if resumed == u32::MAX {
        return Err(last_error());
    }
    Ok(())
}

pub fn wait_exit_code(process: Handle) -> Result<u32, u32> {
    let waited = unsafe { WaitForSingleObject(process, u32::MAX) };
    if waited != 0 {
        return Err(last_error());
    }
    let mut code: u32 = 0;
    let fetched = unsafe { GetExitCodeProcess(process, &mut code) };
    if fetched == 0 {
        return Err(last_error());
    }
    Ok(code)
}

pub fn dotnet_start_ticks() -> Result<u64, u32> {
    let mut creation = 0u64;
    let mut exit_time = 0u64;
    let mut kernel = 0u64;
    let mut user = 0u64;
    let fetched = unsafe {
        GetProcessTimes(
            GetCurrentProcess(),
            &mut creation,
            &mut exit_time,
            &mut kernel,
            &mut user,
        )
    };
    if fetched == 0 {
        return Err(last_error());
    }
    Ok(crate::dotnet_ticks(creation))
}

pub fn close(handle: Handle) {
    if handle != NULL_HANDLE {
        unsafe { CloseHandle(handle) };
    }
}
