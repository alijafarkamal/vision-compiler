#ifndef SEMANTIC_H
#define SEMANTIC_H

#include <map>
#include <string>
#include <vector>
#include <iostream>

struct SemanticContext {
    std::vector<std::map<std::string, int>> scopes;
    std::map<std::string, int> functions;

    SemanticContext() {
        scopes.push_back(std::map<std::string, int>());
        functions["load_image"] = 1;
        functions["resize"] = 3;
        functions["grayscale"] = 1;
        functions["blur"] = 2;
        functions["show"] = 1;
        functions["save"] = 2;
        functions["open_camera"] = 1;
        functions["camera_read"] = 1;
        functions["load_model"] = 1;
        functions["detect"] = 2;
    }

    void enterScope() { scopes.push_back(std::map<std::string, int>()); }
    void exitScope() { scopes.pop_back(); }

    bool declareVariable(const std::string& name, int type) {
        if (scopes.back().count(name)) return false;
        scopes.back()[name] = type;
        return true;
    }

    int getVariable(const std::string& name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            if (it->count(name)) return (*it)[name];
        }
        return -1;
    }
    
    void reportError(int line, const std::string& msg) {
        std::cerr << "EdgeFlow semantic error [line " << line << "]: " << msg << std::endl;
    }
};

extern SemanticContext semContext;

#endif
