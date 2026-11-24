from pathlib import Path
import subprocess
import sys

def patch_tree(base: Path) -> None:
    c = base / "cont.c"
    s = c.read_text()
    s = s.replace("#include <sys/mman.h>", "#ifndef __SWITCH__\n#include <sys/mman.h>\n#endif", 1)
    s = s.replace(
        "        else {\n            return base;\n        }\n#else\n        errno = 0;\n        void * base = mmap(",
        "        else {\n            return base;\n        }\n#elif defined(__SWITCH__)\n        void * base = calloc(*count, stride);\n        if (!base) {\n            *count = (*count) >> 1;\n        } else {\n            return base;\n        }\n#else\n        errno = 0;\n        void * base = mmap(",
        1,
    )
    s = s.replace(
        "            VirtualFree(allocation->base, 0, MEM_RELEASE);\n            rb_raise(rb_eFiberError, \"can't set a guard page: %s\", ERRNOMSG);\n        }\n#else",
        "            VirtualFree(allocation->base, 0, MEM_RELEASE);\n            rb_raise(rb_eFiberError, \"can't set a guard page: %s\", ERRNOMSG);\n        }\n#elif defined(__SWITCH__)\n        (void)page;\n#else",
        1,
    )
    s = s.replace(
        "#ifdef _WIN32\n    VirtualFree(allocation->base, 0, MEM_RELEASE);\n#else",
        "#ifdef _WIN32\n    VirtualFree(allocation->base, 0, MEM_RELEASE);\n#elif defined(__SWITCH__)\n    free(allocation->base);\n#else",
        1,
    )
    s = s.replace(
        "#ifdef _WIN32\n    SYSTEM_INFO info;\n    GetSystemInfo(&info);\n    pagesize = info.dwPageSize;\n#else /* not WIN32 */",
        "#ifdef _WIN32\n    SYSTEM_INFO info;\n    GetSystemInfo(&info);\n    pagesize = info.dwPageSize;\n#elif defined(__SWITCH__)\n    pagesize = 4096;\n#else /* not WIN32 */",
        1,
    )
    c.write_text(s)

    io = base / "io_buffer.c"
    t = io.read_text()
    t = t.replace("#include <sys/mman.h>", "#ifndef __SWITCH__\n#include <sys/mman.h>\n#endif", 1)
    t = t.replace(
        "    if (!base) {\n        rb_sys_fail(\"io_buffer_map_memory:VirtualAlloc\");\n    }\n#else",
        "    if (!base) {\n        rb_sys_fail(\"io_buffer_map_memory:VirtualAlloc\");\n    }\n#elif defined(__SWITCH__)\n    void * base = calloc(1, size);\n    if (!base) {\n        rb_sys_fail(\"io_buffer_map_memory:calloc\");\n    }\n#else",
        1,
    )
    t = t.replace(
        "    data->mapping = mapping;\n#else",
        "    data->mapping = mapping;\n#elif defined(__SWITCH__)\n    (void)descriptor; (void)offset; (void)size; (void)flags;\n    void *base = NULL;\n    rb_raise(rb_eNotImpError, \"IO::Buffer.map(file) is unsupported on Switch\");\n#else",
        1,
    )
    t = t.replace(
        "#ifdef _WIN32\n    VirtualFree(base, 0, MEM_RELEASE);\n#else",
        "#ifdef _WIN32\n    VirtualFree(base, 0, MEM_RELEASE);\n#elif defined(__SWITCH__)\n    (void)size;\n    free(base);\n#else",
        1,
    )
    t = t.replace(
        "    SYSTEM_INFO info;\n    GetSystemInfo(&info);\n    RUBY_IO_BUFFER_PAGE_SIZE = info.dwPageSize;\n#else /* not WIN32 */",
        "    SYSTEM_INFO info;\n    GetSystemInfo(&info);\n    RUBY_IO_BUFFER_PAGE_SIZE = info.dwPageSize;\n#elif defined(__SWITCH__)\n    RUBY_IO_BUFFER_PAGE_SIZE = 4096;\n#else /* not WIN32 */",
        1,
    )
    io.write_text(t)

    pr = base / "process.c"
    ps = pr.read_text()
    ps = ps.replace(
        "#elif defined HAVE_WAIT4\n    return wait4(pid, st, flags, NULL);\n#else",
        "#elif defined HAVE_WAIT4\n    return wait4(pid, st, flags, NULL);\n#elif defined(__SWITCH__)\n    (void)pid; (void)st; (void)flags;\n    errno = ECHILD;\n    return -1;\n#else",
        1,
    )
    pr.write_text(ps)

    tp = base / "thread_pthread.c"
    ts = tp.read_text()
    ts = ts.replace(
        "#include <time.h>\n#include <signal.h>",
        "#include <time.h>\n#include <signal.h>\n#ifdef __SWITCH__\n#include <poll.h>\n#endif",
        1,
    )
    ts = ts.replace(
        "#if defined(SIGVTALRM) && !defined(__CYGWIN__) && !defined(__EMSCRIPTEN__)",
        "#if defined(SIGVTALRM) && !defined(__CYGWIN__) && !defined(__EMSCRIPTEN__) && !defined(__SWITCH__)",
        1,
    )
    ts = ts.replace(
        "    posix_signal(SIGVTALRM, null_func);",
        "#ifndef __SWITCH__\n    posix_signal(SIGVTALRM, null_func);\n#endif",
        1,
    )
    tp.write_text(ts)

    gc = base / "gc.c"
    gs = gc.read_text()
    gs = gs.replace("#include <sys/mman.h>", "#ifndef __SWITCH__\n#include <sys/mman.h>\n#endif", 1)
    gs = gs.replace(
        "#if defined(_WIN32)\n    DWORD old_protect;\n\n    if (!VirtualProtect(body, HEAP_PAGE_SIZE, PAGE_NOACCESS, &old_protect)) {\n#else",
        "#if defined(_WIN32)\n    DWORD old_protect;\n\n    if (!VirtualProtect(body, HEAP_PAGE_SIZE, PAGE_NOACCESS, &old_protect)) {\n#elif defined(__SWITCH__)\n    (void)objspace; (void)body;\n    if (0) {\n#else",
        1,
    )
    gs = gs.replace(
        "#if defined(_WIN32)\n    DWORD old_protect;\n\n    if (!VirtualProtect(body, HEAP_PAGE_SIZE, PAGE_READWRITE, &old_protect)) {\n#else",
        "#if defined(_WIN32)\n    DWORD old_protect;\n\n    if (!VirtualProtect(body, HEAP_PAGE_SIZE, PAGE_READWRITE, &old_protect)) {\n#elif defined(__SWITCH__)\n    (void)objspace; (void)body;\n    if (0) {\n#else",
        1,
    )
    gs = gs.replace(
        "static struct sigaction old_sigbus_handler;\nstatic struct sigaction old_sigsegv_handler;",
        "#if defined(__SWITCH__)\nstatic void uninstall_handlers(void) {}\nstatic void install_handlers(void) {}\n#else\nstatic struct sigaction old_sigbus_handler;\nstatic struct sigaction old_sigsegv_handler;",
        1,
    )
    gs = gs.replace(
        "    sigaction(SIGBUS, &action, &old_sigbus_handler);\n    sigaction(SIGSEGV, &action, &old_sigsegv_handler);\n}\n#endif",
        "    sigaction(SIGBUS, &action, &old_sigbus_handler);\n    sigaction(SIGSEGV, &action, &old_sigsegv_handler);\n}\n#endif\n#endif",
        1,
    )
    gs = gs.replace(
        "    if (USE_MMAP_ALIGNED_ALLOC) {\n        GC_ASSERT(alignment % sysconf(_SC_PAGE_SIZE) == 0);",
        "#ifndef __SWITCH__\n    if (USE_MMAP_ALIGNED_ALLOC) {\n        GC_ASSERT(alignment % sysconf(_SC_PAGE_SIZE) == 0);",
        1,
    )
    gs = gs.replace(
        "        res = (void *)aligned;\n    }\n    else {",
        "        res = (void *)aligned;\n    }\n    else {\n#endif",
        1,
    )
    gs = gs.replace(
        "        ((void**)aligned)[-1] = res;\n        res = (void*)aligned;\n# endif\n    }\n#endif",
        "        ((void**)aligned)[-1] = res;\n        res = (void*)aligned;\n# endif\n#ifndef __SWITCH__\n    }\n#endif\n#endif",
        1,
    )
    gs = gs.replace(
        "    if (USE_MMAP_ALIGNED_ALLOC) {\n        GC_ASSERT(size % sysconf(_SC_PAGE_SIZE) == 0);",
        "#ifndef __SWITCH__\n    if (USE_MMAP_ALIGNED_ALLOC) {\n        GC_ASSERT(size % sysconf(_SC_PAGE_SIZE) == 0);",
        1,
    )
    gs = gs.replace(
        "            rb_bug(\"rb_aligned_free: munmap failed\");\n        }\n    }\n    else {",
        "            rb_bug(\"rb_aligned_free: munmap failed\");\n        }\n    }\n    else {\n#endif",
        1,
    )
    gs = gs.replace(
        "        free(((void**)ptr)[-1]);\n# endif\n    }\n#endif",
        "        free(((void**)ptr)[-1]);\n# endif\n#ifndef __SWITCH__\n    }\n#endif\n#endif",
        1,
    )
    gc.write_text(gs)

    io_c = base / "io.c"
    ios = io_c.read_text()
    ios = ios.replace(
        "#include \"internal/error.h\"",
        "#include \"internal/error.h\"\n#if defined(__SWITCH__) && defined(O_BINARY)\nstatic inline int setmode(int fd, int mode) { (void)fd; (void)mode; return O_BINARY; }\n#endif",
        1,
    )
    io_c.write_text(ios)

if __name__ == "__main__":
    patch_tree(Path(sys.argv[1]))
