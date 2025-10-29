// Chain of Responsibility Pattern: Modular CLI validation steps.
#ifndef ARG_VALIDATION_HPP
#define ARG_VALIDATION_HPP

#include <string>
#include <vector>

class ArgHandler {
public:
    ArgHandler() : next(0) {}
    virtual ~ArgHandler() {}
    void setNext(ArgHandler* nextHandler) { next = nextHandler; }
    bool process(const std::vector<std::string>& args, std::string& error) {
        if (!handle(args, error)) return false;
        if (next) return next->process(args, error);
        return true;
    }
protected:
    virtual bool handle(const std::vector<std::string>& args, std::string& error) = 0;
private:
    ArgHandler* next;
};

class ArgCountHandler : public ArgHandler {
protected:
    bool handle(const std::vector<std::string>& args, std::string& error);
};

class ConfExtensionHandler : public ArgHandler {
protected:
    bool handle(const std::vector<std::string>& args, std::string& error);
};

#endif
