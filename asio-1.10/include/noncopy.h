#pragma once

class NonCopy {
public:
    NonCopy(const NonCopy& oth) = delete;
    NonCopy& operator=(const NonCopy& oth) = delete;

protected:
    NonCopy() = default;
    ~NonCopy() = default;
};
