/*
 * EdgeFlow Interpreter — Phase 1
 * Main entry point.
 *
 * Usage:
 *   edgeflow-interp <source.edge>
 *   edgeflow-interp            (reads from stdin)
 *
 * No LLVM, no code generation, no external libraries beyond
 * the C++ standard library. The interpreter executes EdgeFlow
 * programs directly inside the Bison grammar actions.
 */

#include <cstdio>
#include <iostream>
#include <string>

extern int   yyparse();
extern "C" FILE* yyin;

int main(int argc, char* argv[]) {
    if (argc > 1) {
        yyin = fopen(argv[1], "r");
        if (!yyin) {
            std::cerr << "EdgeFlow interpreter: cannot open file '"
                      << argv[1] << "'" << std::endl;
            return 1;
        }
    } else {
        /* Read from stdin if no file is given */
        yyin = stdin;
    }

    int result = yyparse();

    if (yyin && yyin != stdin)
        fclose(yyin);

    return result;
}
