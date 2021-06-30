#pragma once

#include "cpu.hpp"
#include <stddef.h>

void run_debugger(struct SM83& cpu);

int disassemble_instruction(const uint8_t instr[4]);

void run_log(struct SM83& cpu, char* line, size_t argc);
/// run the cpu unitl an infiniteloop/breakpoint is encountered.
/// line may be of the form ".+\0log_filename\0".
/// the log filename is as specified by `line` if argc>1, or "log.txt" otherwise
