#pragma once
#include <cstddef>
#include <cstdint>
// Only the output seam used by SerialDiagnostics; no Arduino/radio behavior.
class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(const uint8_t* data, size_t size) = 0;
};
