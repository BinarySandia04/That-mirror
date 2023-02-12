#pragma once

#include <cstdint>
#include <vector>
#include <map>

#include "internal.h"

#define UINT32_SIZE 4

namespace That {
    class VM {
        
        public:
            VM(char filename[]);
            ~VM();

            enum Instruction {
                PUSH, // abx
                MOVE, // A, B

                CALL, // A, B, C // A(B+1, B+2, ..., B+C-1)
                ICL, // A, B, C // A(B+1,B+2,...,B+C-1) // Internal call
                RET, // Return something from reg A

                ADD, // A = B + C
                SUB, // A = B - C
                MUL, // A = B * C
                DIV, // A = B / C
            };

            static std::map<That::Internal::InternalFunctions, That::reg_t (*)(That::reg_t*, That::reg_t*)> FunctionMap;
            
        private:
            
            std::vector<reg_t*> regs;

            void MemDump(uint8_t *data, int size);

            int Process(uint8_t ins[], reg_t* cons[], int offset, bool *returnFlag, int *returnVal);
            uint8_t ReadA(uint8_t ins[]);
            uint8_t ReadB(uint8_t ins[]);
            uint8_t ReadC(uint8_t ins[]);
            uint32_t ReadBx(uint8_t ins[]);
            uint32_t ReadAbx(uint8_t ins[]);
            uint32_t ReadBytes(uint8_t *dir);
            reg_t Call(uint8_t a, uint8_t b, uint8_t c);
    };
}