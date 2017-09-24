#include "math256.h"

#include <algorithm>

uint256_t::uint256_t()
{
    hi = 0;
    lo = 0;
}

uint256_t::uint256_t(__uint128_t number)
{
    hi = 0;
    lo = number;
}

bool uint256_t::is_odd() const
{
    return lo & 1;
}

uint256_t uint256_t::div2(const uint256_t number)
{
    uint256_t result;
    __uint128_t carry = number.hi & 1;
    result.hi = number.hi >> 1;
    result.lo = (number.lo >> 1) + (carry << 127);
    return result;
}

uint256_t uint256_t::mul2(const uint256_t number)
{
    uint256_t result;
    __uint128_t carry = number.lo >> 127;
    result.hi = (number.hi << 1) + carry;
    result.lo =  number.lo << 1;
    return result;
}

uint256_t uint256_t::mul3p1(const uint256_t number)
{
    uint256_t intermediate = mul2(number);
    intermediate.lo++;

    return number + intermediate;
}

const uint_fast32_t pow2_128mod10 = 6;

uint_fast32_t uint256_t::mod10(const uint256_t number)
{
    uint_fast32_t hi = number.hi % 10;
    uint_fast32_t lo = number.lo % 10;

    return ((hi * pow2_128mod10 + lo) % 10);
}

const __uint128_t pow2_128div10 = (((__uint128_t) 0) - 1) / 10;

uint256_t uint256_t::div10(const uint256_t number)
{
    uint256_t result;
    result.hi = number.hi / 10;
    uint_fast32_t hi_res = number.hi % 10;
    result.lo = number.lo / 10;
    uint_fast32_t lo_res = number.lo % 10;

    result.lo += hi_res * pow2_128div10 + (hi_res * pow2_128mod10 + lo_res) / 10;

    return result;
}

std::string uint256_t::to_string() const
{
    std::string ascii_number;
    ascii_number.reserve(80);

    uint256_t number = *this;

    uint_fast32_t i = 0;
    do
    {
        uint_fast32_t digit = mod10(number);
        ascii_number.append(std::to_string(digit));
        number = div10(number);
        i++;
    } while (number > 0);

    std::reverse(ascii_number.begin(), ascii_number.end());
    return ascii_number;
}

uint256_t uint256_t::from_string(const std::string& str)
{
    uint256_t result(0);
    for(const char& c :str)
    {
        if((c > 47) && (c < 58))
        {
            int digit = c - 48;
            // TODO: implement real multiplication for this type
            for(int i = 0; i < 10; i++)
            {
                result = result + result;
            }
            result = result + digit;
        }
    }
    return result;
}

//Berechnet Anzahl Binärstellen; nach gonz
uint_fast32_t uint256_t::bitnum(const uint256_t myvalue)
{
    uint_fast32_t result = 1;
    uint256_t comp(2);
    while (comp <= myvalue)
    {
        result++;
        comp = mul2(comp);
    }
    return result;
}
