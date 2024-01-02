# Shaman DBA

```
 .d8888b.  888    888        d8888 888b     d888        d8888 888b    888      8888888b.  888888b.         d8888 
d88P  Y88b 888    888       d88888 8888b   d8888       d88888 8888b   888      888  "Y88b 888  "88b       d88888 
Y88b.      888    888      d88P888 88888b.d88888      d88P888 88888b  888      888    888 888  .88P      d88P888 
 "Y888b.   8888888888     d88P 888 888Y88888P888     d88P 888 888Y88b 888      888    888 8888888K.     d88P 888 
    "Y88b. 888    888    d88P  888 888 Y888P 888    d88P  888 888 Y88b888      888    888 888  "Y88b   d88P  888 
      "888 888    888   d88P   888 888  Y8P  888   d88P   888 888  Y88888      888    888 888    888  d88P   888 
Y88b  d88P 888    888  d8888888888 888   "   888  d8888888888 888   Y8888      888  .d88P 888   d88P d8888888888 
 "Y8888P"  888    888 d88P     888 888       888 d88P     888 888    Y888      8888888P"  8888888P" d88P     888 
```

Architecture Neutral DBA for Embedded Systems

## Why did I create this tool?

1. Security Assessment of Linux Based IoT Devices.
1. This tool is specifically targeted for old version of linux where it difficult to find any debugging or instrumentation tools.

## Programming Interface

1. **Breakpoint Handling** - give you programmable opportunity to when the breakpoint is hit. You will have access to program memory and register which you can manipulate when you hit the breakpoint.
1. **System-call handling** - You can get callback handler before entering and after the syscall is serviced by kernel. You can even cancel system and implement jailer. You will get access to program memory and register which you can manipulate at both points i.e. before and after system call.
1. **File Descriptor Trace** - this are more glorified syscall handler, basically you can get callback when there are operation done on File descriptor.
	1. Since there are different category of file descriptor like File, Socket and IPC you can inteface which you can register and manipulate the program memory/register.
	1. You can use it in trace only mode or even maniplate parameter on before and after system calls.
1. Function Hooking and Instrumentation - you can also replace the entire functionality of the function with your assembly code.
1. Basic Block hooking and instrumentation - same as above.

## Quick Usage Guide 

```shell
Shaman DBI Framework
Usage: ./build/bin/shaman [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  -l,--log TEXT               application debug logs
  -o,--trace TEXT             output of the tracee logs
  -p,--pid INT                PID of process to attach to
  -b,--brk TEXT ...           Address of the breakpoints
  -e,--exec TEXT ... REQUIRED program to execute
  -f,--follow                 follow the fork/clone/vfork syscalls
  -s,--syscall                trace system calls

```
## Build


```shell
cmake -S . -B build
cmake --build build --target shaman
```

## Use Case

1. This can be used to intercept socket operation and used to manipulate data to and from the target program, essentially use it as proxy program
1. Can be used as a tracer that logs specifed register, memory location. this can be dumped to file. This trace can later be loaded in Ghidra to analyze it even futher in more contextual environment.
1. Function hook very similar to system call trace but for functions.
1. Resource Tracing
  1. Kernel usually operates on system resource like File, 

## Dependencies

1. Kaitai - data structure formating and parsing framework
1. Capstone Engine - Disassembly Engine
1. Keystone Engine - Assembler Engine
1. lief - executable parsing framework
1. Linux OS - 2.5 an above

## Challenges

### Multi-threaded Breakpoint handling

The issue here is mulitple threads are sharing same data and code. And adding and remove breakpoints requires you to edit code which is shared between all threads.
The problem arises when a thread A hits a breakpoint to step-over is we have to remove the breakpoint and once the original instruction is executed it places the breakpoint back at that point. while it is doing that a thread B will be running the same instruction where the breakpoint was placed. So thread B misses the breakpoint event.

1. Case 1 - Two threads are operating on different section of the code. This is really not a problem out tool currently supports this feature.
1. Case 2 - Two threads running the same section of the code. This is little challenging to solve currently we are working on this.

## Adding New Architecture Support

There are three places where archiecture specific support is needed
1. Breakpoint Handling
1. System Call Tracing
1. Defining and using Register values

## Research Paper to Incorporate

1. [An In-Depth Analysis of Disassembly on Full-Scale x86/x64 Binaries](https://www.usenix.org/system/files/conference/usenixsecurity16/sec16_paper_andriesse.pdf)
1. ARMore: pushing Love Back into binaires
	1. This paper focues on static rewriting of ARM binaries
	1. It can do this for PIC and non-PIC code with/out symbols
1. [SaBRe: load-time selective binary rewriting](https://link.springer.com/content/pdf/10.1007/s10009-021-00644-w.pdf?pdf=button)
	1. SaBRe rewrites specific constructs—particularly system calls and functions—when the program is loaded into memory, and intercepts them using plugins through a simple API.
	1. We also discuss the theoretical underpinnings of disassembling and rewriting.
	1. We developed two backends—for x86_64 and RISC-V—which were used to implement three plugins: a fast system call tracer, a multi-version executor, and a fault injector.
	1. Our evaluation shows that SaBRe imposes little overhead, typically below 3%.
1. [Non-stop Multi-threaded Debugging in GDB](https://s3.amazonaws.com/arena-attachments/309033/6f46f21a0abfe4de8f56468953378dfb.pdf)
  1. This paper accurately describes the challenges of breakpoint management in muli-threading environment. we have to implement these feature as gdb has implemented it.

## Ref
1. [ARM and MIPS Ptrace Impl](https://github.com/aleden/ptracetricks/blob/main/ptracetricks.cpp)
1. [Writing Debugger in CPP](https://blog.tartanllama.xyz/writing-a-linux-debugger-source-signal/)
1. [Various Qemu Build Linux Machine to test the tool](https://people.debian.org/~aurel32/qemu/)
2. [TinyInst - lightweight DBI Framework by googleprojectzero](https://github.com/googleprojectzero/TinyInst/)
3. [Implementing GDB Stub Protocol](https://medium.com/swlh/implement-gdb-remote-debug-protocol-stub-from-scratch-1-a6ab2015bfc5)
1. [Python3 with ghidra - Mandiant Project](https://github.com/mandiant/Ghidrathon)
2. [bcov produces coverage information without recompiling a program by instrumenting it with breakpoints](https://bcov.sourceforge.net/)
