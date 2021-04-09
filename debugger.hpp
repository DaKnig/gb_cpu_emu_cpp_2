#pragma once

#include "cpu.hpp"

void run_debugger(struct SM83& cpu);

int disassemble_instruction(uint8_t instr[4]);
