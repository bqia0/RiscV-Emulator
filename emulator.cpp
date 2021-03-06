#include "emulator.h"
#include "riscv.h"
#include <iostream>
#include <iomanip>
#include <string>

using namespace emulation;
using namespace std;

uint32_t getLSBMask(uint32_t numberOfBits) {
    if (numberOfBits < 32) {
        return ((uint32_t) 1 << numberOfBits) - 1;
    } else {
        return (uint32_t) 0xFFFFFFFF;
    }
}

// both inclusive
uint32_t bitMask(int start, int end){
    int length = end - start + 1;
    return (((uint32_t) 1 << length) - 1) << start;
}

int16_t BImm(uint32_t instruction){
    return ((instruction & bitMask(8, 11)) >> 7) | 
    ((instruction & bitMask(25, 30)) >> 20) | 
    ((instruction & bitMask(7, 7)) << 4) | 
    ((instruction & bitMask(31, 31)) >> 19);
}

Emulator::Emulator(char* program, uint32_t initialPC = 0) {
    this->program = (uint8_t*) program; // potential bug if char is not 8 bits?
    pc = initialPC;

    registers = vector<uint32_t>(REG_COUNT, 0);
    instructions_executed = 0;
}

void Emulator::executeIType(uint32_t instruction) {
    uint32_t funct3 = (instruction >> FUNCT3_OFFSET) & getLSBMask(3);
    int32_t immediate = ((int32_t) instruction >> I_IMM_OFFSET);
    uint32_t rs1 = (instruction >> RS1_OFFSET) & getLSBMask(REG_INDEX_BITS);
    uint32_t rd = (instruction >> RD_OFFSET) & getLSBMask(REG_INDEX_BITS);
    uint32_t funct7 = instruction >> 25;
    uint32_t shiftAmount = immediate & getLSBMask(5);
    switch (funct3) {
        case ADD_FUNCT3:
            // TODO: check for rd == 0, x0 is hardwired to 0 according to ISA spec.
            registers[rd] = registers[rs1] + immediate;
            break;
        case AND_FUNCT3:
            registers[rd] = registers[rs1] & immediate;
            break;
        case OR_FUNCT3:
            registers[rd] = registers[rs1] | immediate;
            break;
        case XOR_FUNCT3:
            registers[rd] = registers[rs1] ^ immediate;
            break;
        case SLT_FUNCT3:
            registers[rd] = (int32_t) registers[rs1] < immediate;
            break;
        case SLTU_FUNCT3:
            registers[rd] = registers[rs1] < (uint32_t) immediate;
            break;
        case SR_FUNCT3:
            if (funct7 == 0b0100000) { // SRAI
                registers[rd] = ((int32_t) registers[rs1]) >> shiftAmount;
            } else if (funct7 == 0b0000000) { //SRLI
                registers[rd] = registers[rs1] >> shiftAmount;
            }
            break;
        case SLL_FUNCT3:
            registers[rd] = registers[rs1] << shiftAmount;
            break;
        default:
            cout << "warning: unrecognized I-type instruction" << endl;
    }
    pc = pc + 4;
}

void Emulator::executeRType(uint32_t instruction) {
    uint32_t funct3 = (instruction >> FUNCT3_OFFSET) & getLSBMask(3);
    uint32_t rs2 = (instruction >> RS2_OFFSET) & getLSBMask(REG_INDEX_BITS);
    uint32_t rs1 = (instruction >> RS1_OFFSET) & getLSBMask(REG_INDEX_BITS);
    uint32_t rd = (instruction >> RD_OFFSET) & getLSBMask(REG_INDEX_BITS);
    uint32_t funct7 = instruction >> 25;

    uint32_t shiftAmount = registers[rs2] & getLSBMask(SHIFT_AMOUNT_SIZE);
    switch (funct3) {
        case ADD_FUNCT3:
            if (funct7 == 0b0100000) { // SUB
                registers[rd] = registers[rs1] - registers[rs2];
            } else if (funct7 == 0b0000000) { // ADD
                registers[rd] = registers[rs1] + registers[rs2];
            }
            break;
        case AND_FUNCT3:
            registers[rd] = registers[rs1] & registers[rs2];
            break;
        case OR_FUNCT3:
            registers[rd] = registers[rs1] | registers[rs2];
            break;
        case XOR_FUNCT3:
            registers[rd] = registers[rs1] ^ registers[rs2];
            break;
        case SLT_FUNCT3:
            registers[rd] = (int32_t) registers[rs1] < (int32_t) registers[rs2];
            break;
        case SLTU_FUNCT3:
            registers[rd] = registers[rs1] < registers[rs2];
            break;
        case SLL_FUNCT3:
            registers[rd] = registers[rs1] << shiftAmount;
            break;
        case SR_FUNCT3:
            if (funct7 == 0b0100000) { // SRA
                registers[rd] = (int32_t) registers[rs1] >> shiftAmount;
            } else { // SRL
                registers[rd] = registers[rs1] >> shiftAmount;
            }
            break;
        default:
            cout << "warning: unrecognized I-type instruction" << endl;
    }
    pc = pc + 4;
}

void Emulator::executeLUI(uint32_t instruction) {
    uint32_t rd = (instruction >> RD_OFFSET) & getLSBMask(REG_INDEX_BITS);
    registers[rd] = (instruction >> U_IMM_OFFSET) << U_IMM_OFFSET;
    pc = pc + 4;
}

void Emulator::executeAUIPC(uint32_t instruction) {
    uint32_t rd = (instruction >> RD_OFFSET) & getLSBMask(REG_INDEX_BITS);
    uint32_t immediate = (instruction >> U_IMM_OFFSET) << U_IMM_OFFSET;
    registers[rd] = pc + immediate;
    pc = pc + 4;
}

void Emulator::executeJALR(uint32_t instruction){
    uint8_t rd = (instruction >> RD_OFFSET) & getLSBMask(REG_INDEX_BITS);
    uint8_t rs1 = (instruction >> RS1_OFFSET) & getLSBMask(REG_INDEX_BITS);
    int32_t imm = ((int32_t)instruction >> I_IMM_OFFSET);
    registers[rd] = pc + 4;
    pc = (registers[rs1] + imm) & 0xFFFFFFFE;
}

void Emulator::executeBranch(uint32_t instruction){
    uint8_t rs1 = (instruction >> RS1_OFFSET) & getLSBMask(REG_INDEX_BITS);
    uint8_t rs2 = (instruction >> RS2_OFFSET) & getLSBMask(REG_INDEX_BITS);
    int32_t imm = BImm(instruction);
    uint8_t funct3 = (instruction >> FUNCT3_OFFSET) & getLSBMask(3);
    bool branch = false;
    switch(funct3){
        case BEQ_FUNCT3:
            branch = registers[rs1] == registers[rs2];
            break;
        case BNE_FUNCT3:
            branch = registers[rs1] != registers[rs2];
            break;
        case BLT_FUNCT3:
            branch = (int32_t) registers[rs1] < (int32_t) registers[rs2];
            break;
        case BGE_FUNCT3:
            branch =  (int32_t) registers[rs1] >= (int32_t) registers[rs2];
            break;
        case BLTU_FUNCT3:
            branch = registers[rs1] < registers[rs2];
            break;
        case BGEU_FUNCT3:
            branch =  registers[rs1] >= registers[rs2];
            break;
        default:
            cout << "Branch instruction has bad FUNCT3: " << funct3 << endl;
            break;
    }
    if(branch){
        pc = pc + imm;
    }else{
        pc = pc + 4;
    }
}

void Emulator::step(bool inDebugMode) { 
    uint32_t instruction = ((0x000000FF & program[pc]) | 
               (0x000000FF & program[pc + 1]) << 8 | 
               (0x000000FF & program[pc + 2]) << 16 | 
               (0x000000FF & program[pc + 3]) << 24);

    if (inDebugMode) {
        cout << setw(8) << setfill('0') << hex << pc;
        cout << ": " << setw(8) << setfill('0') << hex << instruction << " " << endl;
    }

    uint32_t opcode = instruction & getLSBMask(OPCODE_WIDTH); // bitmask will not work if OPCODE_WIDTH = 32;

    switch (opcode) {
        case OP_IMM:
            executeIType(instruction);
            break;
        case OP_REG:
            executeRType(instruction);
            break;
        case OP_LUI:
            executeLUI(instruction);
            break;
        case OP_AUIPC:
            executeAUIPC(instruction);
            break;
        case OP_JALR:
            executeJALR(instruction);
            break;
        default:
            cout << "Invalid instruction detected; execution terminated" << endl;
            return;
            break;
    }

    instructions_executed++;
}

void Emulator::stepMultiple(int steps, bool inDebugMode) {
    if (steps < 0) {
        throw invalid_argument("# of steps cannot be negative.");
    }
    for(int i = 0; i < steps; i++) {
        step(inDebugMode);
    }
}

bool Emulator::areConditionsMet(vector<CONDITION>& conditions) {
    for (int i = 0; i < conditions.size(); i++) {
        CONDITION condition = conditions[i];
        if (condition.isPC) {
            if (pc != condition.targetValue) return false;
        }
        if (condition.isRegister) {
            if (registers[condition.registerNumber] != condition.targetValue) return false;
        }
    }
    return true;
}

void Emulator::stepUntilConditionsMet(vector<CONDITION>& conditions, bool inDebugMode) {
    while (!areConditionsMet(conditions)) step(inDebugMode);
}

void Emulator::printInstructionsExecuted() {
    cout << dec << instructions_executed << dec << endl;
}

void Emulator::printPC() {
    cout << "0x" << setw(8) << setfill('0') << hex << pc << endl;
}

void Emulator::printRegisters(bool useABINames, bool useDecimal) {
    for (int i = 0; i < REG_COUNT; i++) {
        if (useABINames) {
            cout << setw(5) << setfill(' ') << right << abi_register_names[i];
        } else {
            cout << setw(5) << setfill(' ') << right << ("x" + to_string(i));
        }
        if (useDecimal) {
            cout << ": " << setw(10) << setfill(' ') << dec << registers[i];
        } else { // TODO: Decide if we need to handle unsigned/signed
            cout << ": " << "0x" << setw(8) << setfill('0') << hex << registers[i];
        }
        if ((i + 1) % REG_PRINT_COL_WIDTH == 0) {
            cout << endl;
        }
    }
}

void Emulator::printRegister(string registerName, bool useDecimal) {
    try {
        uint32_t registerNumber = registerNameToRegisterIndex(registerName);
        if (useDecimal) {
            cout << registers[registerNumber] << endl;
        } else {
            cout << "0x" << setw(8) << setfill('0') << registers[registerNumber] << endl;
        }
    } catch (const invalid_argument& e) {
        throw invalid_argument("Specified register does not exist");
    }
}

uint32_t registerNameToRegisterIndex(string registerName) {
    try {
        int registerNumber = -1;
        if (registerName[0] == 'x') {
            registerNumber = stoi(registerName.substr(1, registerName.size()));
        } else {
            for (int i = 0; i < REG_COUNT; i++) {
                if (registerName == abi_register_names[i]) registerNumber = i;
            }
            if (registerName == "fp" || registerName == "s0") registerNumber = 8;
        }
        if (registerNumber < 0 || registerNumber > REG_COUNT - 1) {
            throw invalid_argument("Specified register does not exist");
        }
        return registerNumber;
    } catch (const invalid_argument& e) {
        throw invalid_argument("Specified register does not exist");
    }
}