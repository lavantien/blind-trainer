#include <signal.h>
#include <stdlib.h>

#ifdef __LLVM_INSTR_PROFILE_GENERATE
int __llvm_profile_write_file(void);
#endif

static void probe_flush_on_abrt(int sig) {
#ifdef __LLVM_INSTR_PROFILE_GENERATE
    __llvm_profile_write_file();
#endif
    signal(sig, SIG_DFL);
}

int main(void) {
    signal(SIGABRT, probe_flush_on_abrt);
    abort();
}
