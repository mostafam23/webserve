// Strategy Pattern: Defines how debug mode is determined (always on by default).
#ifndef DEBUG_POLICY_HPP
#define DEBUG_POLICY_HPP

struct IDebugPolicy {
    virtual ~IDebugPolicy() {}
    virtual bool isDebug() const = 0;
};

struct AlwaysOnDebugPolicy : public IDebugPolicy {
    bool isDebug() const { return true; }
};

#endif
