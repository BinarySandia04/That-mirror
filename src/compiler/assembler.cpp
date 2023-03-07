#include "assembler.h"
#include "../flags/flags.h"
#include "../headers/termcolor.hpp"
#include "../headers/debug.hpp"

#include <map>
#include <iostream>
#include <string>
#include <vector>

using namespace That;

Assembler::Assembler(Nodes::Node* ast, Flag::Flags flags){
    try {
        AssembleCode(ast, &instructions);
    } catch(std::string r){
        Debug::LogError(r);
    }

    if(CHECK_BIT(flags, 1)){
        std::cout << termcolor::red << termcolor::bold << "ASM:" << termcolor::reset << std::endl;
        // Ara doncs fem debug de les instruccions
        for(int i = 0; i < instructions.size(); i++){
            std::cout << instructions[i].type << " ";
            if(instructions[i].GetA() != INT32_MIN) std::cout << instructions[i].GetA() << " ";
            if(instructions[i].GetB() != INT32_MIN) std::cout << instructions[i].GetB() << " ";
            if(instructions[i].GetC() != INT32_MIN) std::cout << instructions[i].GetC() << " ";
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }
    
}

MachineCode Assembler::GetAssembly(){
    MachineCode machine;

    machine.instructions = instructions;
    machine.constants = constants;

    return machine;
}

void Assembler::AssembleCode(Nodes::Node* node, std::vector<Instruction> *to){
    
    // Mirem primer quines funcions hi ha al entorn i les identifiquem
    for(int i = 0; i < node->children.size(); i++){
        Nodes::NodeType t = node->children[i]->type;
        if(t == Nodes::FUNCTION){
            AppendReference(node->children[i]->children[0]);
        }
    }

    // Després les definim
    for(int i = 0; i < node->children.size(); i++){
        Nodes::NodeType t = node->children[i]->type;
        if(t == Nodes::FUNCTION){
            AssembleFunction(node->children[i], to);
        }
    }

    // Ara executem    
    for(int i = 0; i < node->children.size(); i++){
        Nodes::Node* n = node->children[i];
        Nodes::NodeType t = n->type;
        
        try {
            if(IsExpression(t)) AssembleExpression(n, to);
            else if(t == Nodes::DEF_FUNCTION) AssembleDef(n, to);
            else if(t == Nodes::DECLARATION) AssembleDeclaration(n, to);
            else if(t == Nodes::ASSIGNATION) AssembleAssignation(n, to);
            else if(t == Nodes::RETURN) AssembleReturn(n, to);
            else if(t == Nodes::IF) AssembleConditional(n, to);
            else if(t == Nodes::WHILE) AssembleWhile(n, to);
            else if(t == Nodes::FOR) AssembleFor(n, to);
            else if(t == Nodes::BREAK) AssembleTempBreak(n, to);
            else if(t == Nodes::SKIP) AssembleTempSkip(n, to);
        } catch(std::string r){
            throw(r);
        }
        
    }
    
}

void Assembler::AssembleFunction(Nodes::Node* func, std::vector<Instruction> *to){
    // Tenim aqui que els fills son en ordre:
    // <nom>, <param> x nd x <param>, <codi>
    // Suposant que al stack tenim les anteriors i al registre tenim els paràmetres fem la funció
    // Hem de saber ara doncs quins paràmetres tenim penjats al stack.
    // Com que aixo ho fem al cridar la funció, doncs estaràn enrere suposo. Anem a
    // afegir-los ara virtualment segons els paràmetres que tenim ara a la funció.
    
    int stack = StartContext();
    // ref - name
    // type - return tipus
    // code
    // params, 2 en 2
    Instruction top(VM::TO, ParamType::E);
    PushInstruction(top, to);
    

    for(int i = 3; i < func->children.size(); i++){
        // i + 2 -> tipus, i + 3 -> ref
        // O get reference id
        AppendReference(func->children[i]);
    }

    AssembleCode(func->children[2], to);

    // Vale ara que ja tenim la funció anem a treure tot el que haviem suposadament
    // ficat al stack
    
    EndContext(stack, to);

    Instruction end(VM::END, ParamType::E);
    PushInstruction(end, to);
}


// TODO: Acabar de fer els ifs
void Assembler::AssembleConditional(Nodes::Node* cond, std::vector<Instruction> *to){
    // If
    // Conditional
    // Code
    // |
    // Conditional
    // Code
    // Code

    int a = 0;
    std::vector<std::vector<Instruction>> conditions, codes;
    for(int i = 0; i < cond->children.size(); i++){
        
        std::cout << i << std::endl;
        cond->children[i]->Debug();
        std::cout << std::endl << std::endl;

        std::vector<Instruction> condit, code;

        if(i % 2 == 0){ // Ultimo codigo o condicion
            if(cond->children.size() % 2 == 1 && i == cond->children.size() - 1){
                // Aislar
                int stack = StartContext();

                AssembleCode(cond->children[i], &code);
                
                EndContext(stack, &code);
                
                codes.push_back(code);
                a += code.size();
            } else {
                AssembleExpression(cond->children[i], &condit);
                conditions.push_back(condit);

                a += condit.size();
            }
            
        } else { // Codigo
            // Aislar
            int stack = StartContext();

            AssembleCode(cond->children[i], &code);

            EndContext(stack, &code);

            codes.push_back(code);
            a += code.size() + 1;
            // if(cond->children.size() % 2 == 1) a++;
            a++;
        }
    }

    // Val si es parell la idea es saltar fins al final
    for(int i = 0; i < cond->children.size(); i++){
        
        std::cout << i << std::endl;
        if(cond->children.size() % 2 == 1 && i == cond->children.size() - 1){
            PushInstructions(&codes[(i) / 2], to);
            break;
        }

        if(i % 2 == 0){
            a -= conditions[i / 2].size();
            PushInstructions(&conditions[i / 2], to);

            a--;
            Instruction test(VM::TEST, ParamType::AB);

            test.SetA(regPointer);
            
            test.SetB(codes[(i / 2)].size() + 1);
            
            PushInstruction(test, to);

            
        } else {
            PushInstructions(&codes[(i - 1) / 2], to);
            a -= codes[(i - 1) / 2].size();

            a--;
            Instruction jmp(VM::JUMP, ParamType::A);
            jmp.SetA(a);
            PushInstruction(jmp, to);
        }
    }
    
}

void Assembler::AssembleFor(Nodes::Node* para, std::vector<Instruction> *to){
    // Assemblem primer doncs una declaració, per això, aillem
    int stack = StartContext();

    std::vector<Instruction> exp, inc, code, total;
    // Ponemos inicializacion y tal
    AssembleCode(para->children[0], &total);
    int decSize = total.size();

    int a = 0;
    AssembleExpression(para->children[1], &exp);
    a += exp.size() + 1;

    AssembleCode(para->children[2], &inc);
    a += inc.size() + 1;

    AssembleCode(para->children[3], &code);
    a += code.size();

    Instruction jump(VM::JUMP, ParamType::A), test(VM::TEST, ParamType::AB);
    test.SetA(regPointer);
    test.SetB(a - exp.size());

    PushInstructions(&exp, &total);
    PushInstruction(test, &total);
    PushInstructions(&code, &total);
    PushInstructions(&inc, &total);

    jump.SetA(-a + 1);
    PushInstruction(jump, &total);

    EndContext(stack, &total);

    // Val ok ara fem
    int p = total.size();
    for(int i = 0; i < total.size(); i++){
        if(total[i].type == VM::JUMP && total[i].temp == 1){
            std::cout << "Hola break" << std::endl;
            // Val eh hem de posar el nombre de salts fin al final ja que això és un break
            total[i].temp = 0;
            total[i].SetA(total.size() - i);
        }

        if(total[i].type == VM::JUMP && total[i].temp == 2){
            // Val eh ara aixo es un continue
            total[i].temp = 0;
            total[i].SetA(-i + decSize);
        }
    }

    PushInstructions(&total, to);
}

// TODO: Falta aillar contexto
void Assembler::AssembleWhile(Nodes::Node* whil, std::vector<Instruction> *to){
    std::vector<Instruction> exp, code, total;

    
    AssembleExpression(whil->children[0], &exp);
    PushInstructions(&exp, &total);
    
    // Aislar
    int stack = StartContext();

    AssembleCode(whil->children[1], &code);

    EndContext(stack, &code);

    int n = exp.size() + code.size() + 1;

    Instruction test(VM::TEST, ParamType::AB);
    test.SetA(regPointer);
    test.SetB(code.size() + 1);

    PushInstruction(test, &total);
    PushInstructions(&code, &total);

    Instruction jump(VM::JUMP, ParamType::A);
    jump.SetA(-n - 1);

    PushInstruction(jump, &total);

    int p = total.size();
    for(int i = 0; i < total.size(); i++){
        if(total[i].type == VM::JUMP && total[i].temp == 1){
            // Val eh hem de posar el nombre de salts fin al final ja que això és un break
            total[i].temp = 0;
            total[i].SetA(total.size() - i);
        }

        if(total[i].type == VM::JUMP && total[i].temp == 2){
            // Val eh ara aixo es un continue
            total[i].temp = 0;
            total[i].SetA(-n - 1 + i);
        }
    }

    PushInstructions(&total, to);
}

void Assembler::AssembleReturn(Nodes::Node* ret, std::vector<Instruction> *to){
    AssembleExpression(ret->children[0], to);

    Instruction retu(VM::RET, ParamType::A);
    retu.SetA(regPointer);

    PushInstruction(retu, to);
}

void Assembler::AssembleTempBreak(Nodes::Node *stop, std::vector<Instruction> *to){
    Instruction tmpStop(VM::JUMP, ParamType::A);
    tmpStop.temp = 1; // Identifier del break

    PushInstruction(tmpStop, to);
}

void Assembler::AssembleTempSkip(Nodes::Node *skip, std::vector<Instruction> *to){
    Instruction tmpSkip(VM::JUMP, ParamType::A);
    tmpSkip.temp = 2; // Identifier del skip

    PushInstruction(tmpSkip, to);
}

void Assembler::AssembleDeclaration(Nodes::Node *dec, std::vector<Instruction> *to){
    // type DEC -> [EXP, TYPE]
    // Primer hauriem de fer un assemble de l'expressió

    // Suposo que hem de fer algo amb el type per optimitzar???
    try {
        AssembleExpression(dec->children[0], to);
    } catch(std::string r){
        throw(r);
    }
    // Ara l'expression hauria d'estar al top del stack
    Instruction push(VM::PUSH, ParamType::AB);
    push.SetA(regPointer);
    push.SetB(regPointer);

    AppendReference(dec);

    PushInstruction(push, to);
}

void Assembler::AssembleAssignation(Nodes::Node* assign, std::vector<Instruction> *to){
    // Ok primer l'expressió
    int where;
    try {
        where = GetRefId(assign->GetDataString());
    } catch(std::string ex){
        throw(ex);
    }
    
    AssembleExpression(assign->children[0], to);

    Instruction move(VM::MOVE, ParamType::AB);

    move.SetA(regPointer);
    move.SetB(where);

    PushInstruction(move, to);

}

void Assembler::AssembleExpression(Nodes::Node *exp, std::vector<Instruction> *to){
    // Esta molt guai tenim una expression hem de fer coses
    // Podem tenir valors literals, la idea es que el resultat final el tinguem al registre que apunta el nostre punter + 1
    // Hem de tenir en compte que els registres després del punter no tenen efecte

    if(exp->type == Nodes::NodeType::EXP_BINARY){
        // La idea es veure si una de les dues doncs es literal o no
        Nodes::Node* f = exp->children[0], *s = exp->children[1], *t;
        
        // Optimització important
        if(IsValue(f->type)){
            t = s;
            s = f;
            f = t;
        }

        Instruction op(TranslateBinOpId(exp->nd), ParamType::AB);
        // Val si cap dels dos es valor podem cridar recursivament AssembleExpression amb un dels dos
        AssembleExpression(f, to);
        // Ara tenim al nostre punter f assembleat. L'augmentem i assemblem t
        op.SetA(regPointer);
        IncreasePointer();
        AssembleExpression(s, to);
        op.SetB(regPointer);
        DecreasePointer();

        PushInstruction(op, to);
    }
    else if(exp->type == Nodes::NodeType::EXP_UNARY){
        Nodes::Node* f = exp->children[0];
        Instruction op(TranslateUnOpId(exp->nd), ParamType::A);
        // Val doncs volem al nostre punter l'expressió
        AssembleExpression(f, to);
        // I ara li apliquem la operació
        op.SetA(regPointer);

        PushInstruction(op, to);
    } else if(IsValue(exp->type)){
        // Bueno carreguem i ja està
        Instruction loadc(VM::Instructions::LOADC, ParamType::AB);
        loadc.SetA(regPointer);
        loadc.SetB(GetConstId(exp));

        PushInstruction(loadc, to);
        return;
    } else if(exp->type == Nodes::NodeType::EXP_CALL){
        AssembleCall(exp, to);
    } else if(exp->type == Nodes::NodeType::REFERENCE){
        int ref;
        try {
            ref = GetRefId(exp->GetDataString());
        } catch(std::string ex){
            throw(ex);
        }
        Instruction load(VM::LOAD, ParamType::AB);
        load.SetA(regPointer);
        load.SetB(ref);

        PushInstruction(load, to);
    }
}

void Assembler::AssembleDef(Nodes::Node* def, std::vector<Instruction> *to){
    // El fill és una expression
    Instruction dif(VM::DEF, ParamType::A);
    AssembleExpression(def->children[0], to);
    dif.SetA(regPointer);

    PushInstruction(dif, to);
}

void Assembler::AssembleCall(Nodes::Node *call, std::vector<Instruction> *to){
    // Ok suposem que call és una call.
    // Primer carreguem la funció a cridar
    Instruction loadc(VM::LOADC, ParamType::AB);
    loadc.SetA(regPointer);
    
    try {
        loadc.SetB(GetRefId(call->GetDataString()));
    } catch(std::string s){
        throw(s);
    }

    PushInstruction(loadc, to);
    IncreasePointer();
    // Ara estaria bé carregar tots els paràmetres
    for(int i = 0; i < call->children.size(); i++){
        AssembleExpression(call->children[i], to);
        IncreasePointer();
    }
    // Tornem enrere on la funció
    for(int i = 0; i <= call->children.size(); i++) DecreasePointer();

    // Empenyem al stack
    Instruction stackPush(VM::PUSH, ParamType::AB);
    stackPush.SetA(regPointer + 1);
    stackPush.SetB(regPointer + call->children.size());

    // Ara cridem a la funció. Hauria de sobreescriure a on es troba en la memoria
    // Com que 
    Instruction cins(VM::CALL, ParamType::ABC);
    cins.SetA(regPointer);
    cins.SetB(regPointer + 1);
    cins.SetC(regPointer + call->children.size());

    PushInstruction(cins, to);
}

// De moment serà una resta chunga
VM::Instructions Assembler::TranslateBinOpId(int data){
    return (VM::Instructions) ((int) VM::Instructions::ADD + data - 5);
}

VM::Instructions Assembler::TranslateUnOpId(int data){
    return VM::Instructions::ADD;
}

void Assembler::AppendReference(That::Nodes::Node* ref){
    // Suposem que ref es de tipus referència. Aleshores doncs té un string molt maco!
    std::string id = ref->GetDataString();
    
    identifierStack.push_back(id);
    
    // std::cout << "Appended: " << id << std::endl;
}

void Assembler::IncreasePointer(){
    regPointer++;
    if(regPointer >= regCount) regCount = regPointer + 1;
}

void Assembler::DecreasePointer(){
    regPointer--;
}

// TODO: Canviar això per suportar més coses
bool Assembler::IsValue(Nodes::NodeType t){
    return (t == Nodes::NodeType::VAL_BOOLEAN ||
    t == Nodes::NodeType::VAL_INT ||
    t == Nodes::NodeType::VAL_NULL ||
    t == Nodes::NodeType::VAL_REAL ||
    t == Nodes::NodeType::VAL_STRING);
}

bool Assembler::IsExpression(Nodes::NodeType t){
    return (IsValue(t) ||
    t == Nodes::NodeType::EXP_BINARY ||
    t == Nodes::NodeType::EXP_UNARY ||
    t == Nodes::NodeType::EXP_CALL);
}

void Assembler::PushInstruction(Instruction ins, std::vector<Instruction> *where){
    where->push_back(ins);
}

void Assembler::PushInstructions(std::vector<Instruction> *from, std::vector<Instruction> *to){
    for(int i = 0; i < from->size(); i++){
        PushInstruction((*from)[i], to);
    }
}

// TODO: Aixo es de prova
int Assembler::GetConstId(Nodes::Node *val){
    // Vale aqui tenim tot el tema de constants i tal.
    // Hem de fer un reg_t per dir doncs quines constants són i tal
    Constant c;
    reg_t data;
    // Ara fem switch segons el que sigui val. Es un VAL_ALGO
    switch(val->type){
        case Nodes::VAL_INT:
            // Aqui data és int
            data.num = val->nd;
            data.type = reg_t::INT;
            break;
        case Nodes::VAL_BOOLEAN:
            data.num = val->nd,
            data.type = reg_t::BOOLEAN;
            break;
        case Nodes::VAL_STRING:
            data.num = val->nd;
            data.data = (uint8_t *) val->data.bytes;

            data.type = reg_t::STRING;
            break;
        case Nodes::VAL_REAL:
            data.num = val->nd;
            data.data = (uint8_t *) val->data.bytes;

            data.type = reg_t::REAL;
            break;
        case Nodes::VAL_NULL:
            data.num = val->nd;
            data.type = reg_t::_NULL;
            break;
        default:
            break;
    }
    c.data = data;

    // Val ja tenim la constant ara la busquem!!!
    int i;
    for(i = 0; i < constants.size(); i++){
        // Comparem constants[i].data amb val->data
        bool eq = true;
        if(constants[i].data.type == data.type){
            if(constants[i].data.num == data.num){
                for(int j = 0; j < data.num; j++){
                    if(constants[i].data.data[j] != data.data[j]){
                        eq = false;
                        break;
                    }
                }

                if(eq){
                    // Son iguals
                    return i;
                }
            }
        }
        // No són iguals
    }
    constants.push_back(c);
    return i;
}


int Assembler::StartContext(){
    stackPointer = identifierStack.size();
    return stackPointer;
}

// TODO: Falta alguna manera para decir a la maquina virtual de hacer close
void Assembler::EndContext(int from, std::vector<Instruction> *to){
    int n = 0;
    while(identifierStack.size() > from){
        identifierStack.pop_back();
        n++;
    }
    stackPointer = from;

    Instruction close(VM::CLOSE, ParamType::A);
    close.SetA(n);
    to->push_back(close);
}

int Assembler::GetRefId(std::string ref){
    for(int i = identifierStack.size() - 1; i >= 0; i--){
        if(identifierStack[i] == ref){
            //std::cout << ref << std::endl;
            return i - stackPointer;
        }
    }
    //std::cout << ref << std::endl;
    // Error
    throw(std::string("Name Error: " + ref + " is not defined."));
}

Instruction::Instruction(){
    Instruction(VM::HALT, ParamType::E);
}

Instruction::Instruction(VM::Instructions type, ParamType paramType){
    this->type = type;
    this->paramType = paramType;
    
    this->a = INT32_MIN;
    this->b = INT32_MIN;
    this->c = INT32_MIN;
    this->temp = 0;
}

void Instruction::SetA(int a){
    this->a = a;
}
void Instruction::SetB(int b){
    this->b = b;
}
void Instruction::SetC(int c){
    this->c = c;
}

int Instruction::GetA(){
    return this->a;
}
int Instruction::GetB(){
    return this->b;
}
int Instruction::GetC(){
    return this->c;
}