#include <iostream>
#include <string>
#include <memory>
#include <system_error>
#include "AST.h"
#include "CodeGen.h"
#include "parser.tab.hpp"

#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>

extern int yyparse();
extern "C" FILE* yyin;
extern std::unique_ptr<ProgramAST> rootNode;

int main(int argc, char** argv) {
    bool print_ast = false;
    std::string filename;
    std::string output_file = "output.ll";
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ast") {
            print_ast = true;
        } else if (arg == "-o" && i + 1 < argc) {
            output_file = argv[++i];
        } else {
            filename = arg;
        }
    }

    if (!filename.empty()) {
        yyin = fopen(filename.c_str(), "r");
        if (!yyin) {
            std::cerr << "Cannot open " << filename << std::endl;
            return 1;
        }
    }

    if (yyparse() == 0) {
        if (print_ast && rootNode) {
            rootNode->print();
            return 0;
        } 
        
        if (rootNode) {
            if (!rootNode->checkSemantics()) {
                std::cerr << "Semantic analysis failed." << std::endl;
                return 1;
            }
            rootNode->optimize();
            llvm::Value* prog = rootNode->codegen();
            if (!prog) {
                std::cerr << "Code generation failed." << std::endl;
                return 1;
            }
            
            std::error_code EC;
            llvm::raw_fd_ostream dest(output_file, EC, llvm::sys::fs::OF_None);
            if (EC) {
                std::cerr << "Could not open file: " << EC.message() << std::endl;
                return 1;
            }
            cgContext.module->print(dest, nullptr);
            std::cout << "Successfully generated " << output_file << std::endl;
        }
    } else {
        std::cerr << "Parsing failed." << std::endl;
        return 1;
    }
    
    return 0;
}
