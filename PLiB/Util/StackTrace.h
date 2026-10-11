//
// Created by Fredrik Robertsen on 5/1/26.
//

#ifndef PHELL_STACKTRACE_H
#define PHELL_STACKTRACE_H

#ifdef ENABLE_UNWIND

#define UNW_LOCAL_ONLY
#include <libunwind.h>
#include <cxxabi.h>
#include <cstdint>
#include <string>
#include <iostream>
#include <memory>
#include <algorithm>
#include <stdint.h>

inline void printStacktraceUnwind(std::ostream& out = std::cerr) {
    unw_context_t context;
    unw_cursor_t cursor;
    unw_getcontext(&context);
    unw_init_local(&cursor, &context);

    out << "Stack trace:\n";
    int frame = 0;
    while (unw_step(&cursor) > 0) {
        unw_word_t pc, offset;
        if (unw_get_reg(&cursor, UNW_REG_IP, &pc) < 0 || pc == 0)
            break;

        char raw_name[512];
        std::string function = "<unknown>";
        if (unw_get_proc_name(&cursor, raw_name, sizeof(raw_name), &offset) == 0) {
            int status = 0;
            std::unique_ptr<char, void(*)(void*)> demangled(
                abi::__cxa_demangle(raw_name, nullptr, nullptr, &status),
                std::free
            );
            function = (status == 0 && demangled) ? demangled.get() : raw_name;
        }

        out << "  [" << frame++ << "] " << function
            << " + 0x" << std::hex << offset
            << " (pc=0x" << static_cast<std::uintptr_t>(pc) << ")\n";
    }
}


#endif

#endif //PHELL_STACKTRACE_H