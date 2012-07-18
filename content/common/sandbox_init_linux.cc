// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/sandbox_init.h"

#if defined(__i386__) || defined(__x86_64__)

// This is an assert for GYP
#if !defined(OS_LINUX)
  #error "Linux specific file compiled on non Linux OS!"
#endif

#include <asm/unistd.h>
#include <errno.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <signal.h>
#include <string.h>
#include <sys/prctl.h>
#include <ucontext.h>
#include <unistd.h>

#include <vector>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "content/public/common/content_switches.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"

// These are fairly new and not defined in all headers yet.
#if defined(__x86_64__)

#ifndef __NR_process_vm_readv
  #define __NR_process_vm_readv 310
#endif

#ifndef __NR_process_vm_writev
  #define __NR_process_vm_writev 311
#endif

#elif defined(__i386__)

#ifndef __NR_process_vm_readv
  #define __NR_process_vm_readv 347
#endif

#ifndef __NR_process_vm_writev
  #define __NR_process_vm_writev 348
#endif

#endif

namespace {

bool IsSingleThreaded() {
  // Possibly racy, but it's ok because this is more of a debug check to catch
  // new threaded situations arising during development.
  int num_threads =
    file_util::CountFilesCreatedAfter(FilePath("/proc/self/task"),
                                      base::Time::UnixEpoch());

  // We pass the test if we don't know ( == 0), because the setuid sandbox
  // will prevent /proc access in some contexts.
  return num_threads == 1 || num_threads == 0;
}

inline bool IsChromeOS() {
#if defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

intptr_t CrashSIGSYS_Handler(const struct arch_seccomp_data& args, void* aux) {
  int syscall = args.nr;
  if (syscall >= 1024)
    syscall = 0;
  // Encode 8-bits of the 1st two arguments too, so we can discern which socket
  // type, which fcntl, ... etc., without being likely to hit a mapped
  // address.
  // Do not encode more bits here without thinking about increasing the
  // likelihood of collision with mapped pages.
  syscall |= ((args.args[0] & 0xffUL) << 12);
  syscall |= ((args.args[1] & 0xffUL) << 20);
  // Purposefully dereference the syscall as an address so it'll show up very
  // clearly and easily in crash dumps.
  volatile char* addr = reinterpret_cast<volatile char*>(syscall);
  *addr = '\0';
  // In case we hit a mapped address, hit the null page with just the syscall,
  // for paranoia.
  syscall &= 0xfffUL;
  addr = reinterpret_cast<volatile char*>(syscall);
  *addr = '\0';
  for (;;)
    _exit(1);
}

// TODO(jln) we need to restrict the first parameter!
bool IsKillSyscall(int sysno) {
  switch (sysno) {
    case __NR_kill:
    case __NR_tkill:
    case __NR_tgkill:
      return true;
    default:
      return false;
  }
}

bool IsGettimeSyscall(int sysno) {
  switch (sysno) {
    case __NR_clock_gettime:
    case __NR_gettimeofday:
    case __NR_time:
      return true;
    default:
      return false;
  }
}

bool IsFileSystemSyscall(int sysno) {
  switch (sysno) {
    case __NR_open:
    case __NR_openat:
    case __NR_execve:
    case __NR_access:
    case __NR_mkdir:
    case __NR_mkdirat:
    case __NR_readlink:
    case __NR_readlinkat:
    case __NR_stat:
    case __NR_lstat:
    case __NR_chdir:
    case __NR_mknod:
    case __NR_mknodat:
      return true;
    default:
      return false;
  }
}

#if defined(__x86_64__)
// x86_64 only because it references system calls that are multiplexed on IA32.
playground2::Sandbox::ErrorCode GpuProcessPolicy_x86_64(int sysno) {
  switch(sysno) {
    case __NR_read:
    case __NR_ioctl:
    case __NR_poll:
    case __NR_epoll_wait:
    case __NR_recvfrom:
    case __NR_write:
    case __NR_writev:
    case __NR_gettid:
    case __NR_sched_yield:  // Nvidia binary driver.

    case __NR_futex:
    case __NR_madvise:
    case __NR_sendmsg:
    case __NR_recvmsg:
    case __NR_eventfd2:
    case __NR_pipe:
    case __NR_mmap:
    case __NR_mprotect:
    case __NR_clone:  // TODO(jln) restrict flags.
    case __NR_set_robust_list:
    case __NR_getuid:
    case __NR_geteuid:
    case __NR_getgid:
    case __NR_getegid:
    case __NR_epoll_create:
    case __NR_fcntl:
    case __NR_socketpair:
    case __NR_epoll_ctl:
    case __NR_prctl:
    case __NR_fstat:
    case __NR_close:
    case __NR_restart_syscall:
    case __NR_rt_sigreturn:
    case __NR_brk:
    case __NR_rt_sigprocmask:
    case __NR_munmap:
    case __NR_dup:
    case __NR_mlock:
    case __NR_munlock:
    case __NR_exit:
    case __NR_exit_group:
    case __NR_getpid:  // Nvidia binary driver.
    case __NR_getppid:  // ATI binary driver.
    case __NR_lseek:  // Nvidia binary driver.
    case __NR_shutdown:  // Virtual driver.
    case __NR_rt_sigaction:  // Breakpad signal handler.
      return playground2::Sandbox::SB_ALLOWED;
    case __NR_socket:
      return EACCES;  // Nvidia binary driver.
    case __NR_fchmod:
      return EPERM;  // ATI binary driver.
    default:
      if (IsGettimeSyscall(sysno) ||
          IsKillSyscall(sysno)) { // GPU watchdog.
        return playground2::Sandbox::SB_ALLOWED;
      }
      // Generally, filename-based syscalls will fail with ENOENT to behave
      // similarly to a possible future setuid sandbox.
      if (IsFileSystemSyscall(sysno)) {
        return ENOENT;
      }
      // In any other case crash the program with our SIGSYS handler
      return playground2::Sandbox::ErrorCode(CrashSIGSYS_Handler, NULL);
  }
}

// x86_64 only because it references system calls that are multiplexed on IA32.
playground2::Sandbox::ErrorCode FlashProcessPolicy_x86_64(int sysno) {
  switch (sysno) {
    case __NR_futex:
    case __NR_write:
    case __NR_epoll_wait:
    case __NR_read:
    case __NR_times:
    case __NR_clone:  // TODO(jln): restrict flags.
    case __NR_set_robust_list:
    case __NR_getuid:
    case __NR_geteuid:
    case __NR_getgid:
    case __NR_getegid:
    case __NR_epoll_create:
    case __NR_fcntl:
    case __NR_socketpair:
    case __NR_pipe:
    case __NR_epoll_ctl:
    case __NR_gettid:
    case __NR_prctl:
    case __NR_fstat:
    case __NR_sendmsg:
    case __NR_mmap:
    case __NR_munmap:
    case __NR_mprotect:
    case __NR_madvise:
    case __NR_rt_sigaction:
    case __NR_rt_sigprocmask:
    case __NR_wait4:
    case __NR_exit_group:
    case __NR_exit:
    case __NR_rt_sigreturn:
    case __NR_restart_syscall:
    case __NR_close:
    case __NR_recvmsg:
    case __NR_lseek:
    case __NR_brk:
    case __NR_sched_yield:
    case __NR_shutdown:
    case __NR_sched_getaffinity:
    case __NR_sched_setscheduler:
    case __NR_dup:  // Flash Access.
    // These are under investigation, and hopefully not here for the long term.
    case __NR_shmctl:
    case __NR_shmat:
    case __NR_shmdt:
      return playground2::Sandbox::SB_ALLOWED;
    case __NR_ioctl:
      return ENOTTY;  // Flash Access.
    case __NR_socket:
      return EACCES;

    default:
      if (IsGettimeSyscall(sysno) ||
          IsKillSyscall(sysno)) {
        return playground2::Sandbox::SB_ALLOWED;
      }
      if (IsFileSystemSyscall(sysno)) {
        return ENOENT;
      }
      // In any other case crash the program with our SIGSYS handler.
      return playground2::Sandbox::ErrorCode(CrashSIGSYS_Handler, NULL);
  }
}
#endif

playground2::Sandbox::ErrorCode BlacklistPtracePolicy(int sysno) {
  if (sysno < static_cast<int>(MIN_SYSCALL) ||
      sysno > static_cast<int>(MAX_SYSCALL)) {
    // TODO(jln) we should not have to do that in a trivial policy.
    return ENOSYS;
  }
  switch (sysno) {
    case __NR_ptrace:
    case __NR_process_vm_readv:
    case __NR_process_vm_writev:
    case __NR_migrate_pages:
    case __NR_move_pages:
      return playground2::Sandbox::ErrorCode(CrashSIGSYS_Handler, NULL);
    default:
      return playground2::Sandbox::SB_ALLOWED;
  }
}

// Allow all syscalls.
// This will still deny x32 or IA32 calls in 64 bits mode or
// 64 bits system calls in compatibility mode.
playground2::Sandbox::ErrorCode AllowAllPolicy(int sysno) {
  if (sysno < static_cast<int>(MIN_SYSCALL) ||
      sysno > static_cast<int>(MAX_SYSCALL)) {
    // TODO(jln) we should not have to do that in a trivial policy.
    return ENOSYS;
  } else {
    return playground2::Sandbox::SB_ALLOWED;
  }
}

// Is the sandbox fully disabled for this process?
bool ShouldDisableSandbox(const CommandLine& command_line,
                          const std::string& process_type) {
  if (command_line.HasSwitch(switches::kNoSandbox) ||
      command_line.HasSwitch(switches::kDisableSeccompFilterSandbox)) {
    return true;
  }

  if (!IsChromeOS()) {
    // On non ChromeOS we never enable the sandbox AT ALL unless
    // CHROME_ENABLE_SECCOMP is in the environment.
    // TODO(jorgelo): remove this when seccomp BPF is included
    // in an upstream release Linux kernel.
    static const char kEnableSeccomp[] = "CHROME_ENABLE_SECCOMP";
    scoped_ptr<base::Environment> env(base::Environment::Create());

    if (!env->HasVar(kEnableSeccomp))
      return true;
  }

  if (process_type == switches::kGpuProcess) {
    // The GPU sandbox is disabled by default in ChromeOS, enabled by default on
    // generic Linux.
    // TODO(jorgelo): when we feel comfortable, make this a policy decision
    // instead. (i.e. move this to GetProcessSyscallPolicy) and return an
    // AllowAllPolicy for lack of "--enable-gpu-sandbox".
    bool should_disable;
    if (IsChromeOS()) {
      should_disable = true;
    } else {
      should_disable = false;
    }

    if (command_line.HasSwitch(switches::kEnableGpuSandbox))
      should_disable = false;
    if (command_line.HasSwitch(switches::kDisableGpuSandbox))
      should_disable = true;
    return should_disable;
  }

  return false;
}

playground2::Sandbox::EvaluateSyscall GetProcessSyscallPolicy(
    const CommandLine& command_line,
    const std::string& process_type) {
#if defined(__x86_64__)
  if (process_type == switches::kGpuProcess) {
    return GpuProcessPolicy_x86_64;
  }

  if (process_type == switches::kPpapiPluginProcess) {
    // TODO(jln): figure out what to do with non-Flash PPAPI
    // out-of-process plug-ins.
    return FlashProcessPolicy_x86_64;
  }

  if (process_type == switches::kRendererProcess ||
      process_type == switches::kWorkerProcess) {
    return BlacklistPtracePolicy;
  }
  NOTREACHED();
  // This will be our default if we need one.
  return AllowAllPolicy;
#else
  // On IA32, we only have a small blacklist at the moment.
  (void) process_type;
  return BlacklistPtracePolicy;
#endif  // __x86_64__
}

void InitializeSandbox_x86() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  const std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  if (ShouldDisableSandbox(command_line, process_type))
    return;

  // No matter what, InitializeSandbox() should always be called before threads
  // are started.
  // Note: IsSingleThreaded() will be true if /proc is not accessible!
  if (!IsSingleThreaded()) {
    std::string error_message = "InitializeSandbox() called with multiple "
                                "threads in process " + process_type;
    // TODO(jln): change this into a CHECK() once we are more comfortable it
    // does not trigger.
    // On non-DEBUG build, we still log an error
    LOG(ERROR) << error_message;
    return;
  }

  // We should not enable seccomp mode 2 if seccomp mode 1 is activated. There
  // is no easy way to know if seccomp mode 1 is activated, this is a hack.
  //
  // SeccompSandboxEnabled() (which really is "may we try to enable seccomp
  // inside the Zygote") is buggy because it relies on the --enable/disable
  // seccomp-sandbox flags which unfortunately doesn't get passed to every
  // process type.
  // TODO(markus): Provide a way to detect if seccomp mode 1 is enabled or let
  // us disable it by default on Debug builds.
  if (prctl(PR_GET_SECCOMP, 0, 0, 0, 0) != 0) {
    VLOG(1) << "Seccomp BPF disabled in "
            << process_type
            <<  " because seccomp mode 1 is enabled.";
    return;
  }

  // TODO(jln): find a way for the Zygote processes under the setuid sandbox to
  // have a /proc fd and pass it here.
  // Passing -1 as the /proc fd since we have no special way to have it for
  // now.
  if (playground2::Sandbox::supportsSeccompSandbox(-1) !=
      playground2::Sandbox::STATUS_AVAILABLE) {
    return;
  }

  playground2::Sandbox::EvaluateSyscall SyscallPolicy =
      GetProcessSyscallPolicy(command_line, process_type);

  playground2::Sandbox::setSandboxPolicy(SyscallPolicy, NULL);
  playground2::Sandbox::startSandbox();

  // TODO(jorgelo): remove this once we surface
  // seccomp filter sandbox status in about:sandbox.
  const std::string ActivatedSeccomp =
      "Activated seccomp filter sandbox for process type: " +
      process_type + ".";
  if (IsChromeOS())
    LOG(WARNING) << ActivatedSeccomp;
  else
    VLOG(1) << ActivatedSeccomp;
}

}  // anonymous namespace

#endif  // defined(__i386__) || defined(__x86_64__)

namespace content {

void InitializeSandbox() {
#if defined(__i386__) || defined(__x86_64__)
  InitializeSandbox_x86();
#endif
}

}  // namespace content
