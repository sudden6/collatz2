#ifndef MATH256_H
#define MATH256_H

#include <cinttypes>
#include <string>

class uint256_t
{
public:
    uint256_t();
    uint256_t(__uint128_t number);
    bool is_odd() const;
    static uint256_t div2(const uint256_t number);
    static uint256_t mul2(const uint256_t number);
    static uint256_t mul3p1(const uint256_t number);
    static uint_fast32_t mod10(const uint256_t number);
    static uint256_t div10(const uint256_t number);
    static uint_fast32_t bitnum(const uint256_t myvalue);
    static uint256_t from_string(const std::string& str);
    std::string to_string() const;

    inline bool operator==(const uint256_t& rhs) const
    {
        return (lo == rhs.lo) && (hi == rhs.hi);
    }
    inline bool operator==(const __uint128_t rhs) const
    {
        return (lo == rhs) && (hi == 0);
    }
    inline bool operator<(const uint256_t& rhs) const
    {
        return ((hi < rhs.hi) || ((hi == rhs.hi) && (lo < rhs.lo)));
    }

    inline uint256_t operator+(const uint256_t rhs) const
    {
        uint256_t sum;
        sum.lo = lo + rhs.lo;
        unsigned int carry = sum.lo < lo;
        sum.hi = hi + rhs.hi + carry;
        return sum;
    }

private:
    __uint128_t hi;
    __uint128_t lo;
};

inline bool operator!=(const uint256_t& lhs, const uint256_t& rhs){ return !(lhs == rhs); }
inline bool operator> (const uint256_t& lhs, const uint256_t& rhs){ return rhs < lhs; }
inline bool operator<=(const uint256_t& lhs, const uint256_t& rhs){ return !(lhs > rhs); }
inline bool operator>=(const uint256_t& lhs, const uint256_t& rhs){ return !(lhs < rhs); }

#endif // MATH256_H
