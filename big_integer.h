#ifndef BIG_INTEGER_H_INCLUDED
#define BIG_INTEGER_H_INCLUDED

#include <random>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <cassert>
#include <stdexcept>
#include <cstring>
#include <cstdint>

using namespace std;

#ifndef __GNUC__ // portable method
inline int log2(uint32_t v)
{
    if(v <= 0)
    {
        return -1;
    }
    const uint32_t bitMask[] = {0x2, 0xC, 0xF0, 0xFF00, 0xFFFF0000};
    const int shift[] = {1, 2, 4, 8, 16};
    int retval = 0;
    for(int i = 4; i >= 0; i--)
    {
        if(v & bitMask[i])
        {
            v >>= shift[i];
            retval |= shift[i];
        }
    }
    return retval;
}

inline int log2(uint64_t v)
{
    if(v <= 0)
    {
        return -1;
    }
    const uint64_t bitMask[] = {0x2, 0xC, 0xF0, 0xFF00, 0xFFFF0000, 0xFFFFFFFF00000000U};
    const int shift[] = {1, 2, 4, 8, 16, 32};
    int retval = 0;
    for(int i = 5; i >= 0; i--)
    {
        if(v & bitMask[i])
        {
            v >>= shift[i];
            retval |= shift[i];
        }
    }
    return retval;
}
#else // fast GCC specific method
inline int log2(uint32_t v)
{
    if(v <= 0)
    {
        return -1;
    }
    if(sizeof(unsigned) == sizeof(uint32_t))
    {
        return 31 - __builtin_clz(v);
    }
    if(sizeof(unsigned long) == sizeof(uint32_t))
    {
        return 31 - __builtin_clzl(v);
    }
    return 31 - __builtin_clzll(v);
}
inline int log2(uint64_t v)
{
    if(v <= 0)
    {
        return -1;
    }
    if(sizeof(unsigned) == sizeof(uint64_t))
    {
        return 63 - __builtin_clz(v);
    }
    if(sizeof(unsigned long) == sizeof(uint64_t))
    {
        return 63 - __builtin_clzl(v);
    }
    return 63 - __builtin_clzll(v);
}
#endif

inline int randomDigit(int base, bool useSecureRandom = false)
{
    static random_device * theRandomDevice = NULL;
    static default_random_engine theDefaultRandomEngine;
    static bool didInitDefaultRandomEngine = false;
    if(!didInitDefaultRandomEngine)
    {
        didInitDefaultRandomEngine = true;
        theDefaultRandomEngine.seed(time(NULL));
    }
    uniform_int_distribution<int> d(0, base - 1);
    if(!useSecureRandom)
        return d(theDefaultRandomEngine);
    try
    {
        if(theRandomDevice == NULL)
        {
            theRandomDevice = new random_device;
            atexit([](){delete theRandomDevice;});
        }
        return d(*theRandomDevice);
    }
    catch(exception * e)
    {
        return d(theDefaultRandomEngine);
    }
}

class BigInteger
{
private:
    bool isNegative;
    uint32_t * digits;
    size_t size, allocated;
    unsigned * referenceCount;
    void handleWrite(size_t minAllocated)
    {
        minAllocated = max(minAllocated, size);
        size_t clearLength = minAllocated;
        if(*referenceCount > 1)
        {
            uint32_t * newDigits = new uint32_t[minAllocated];
            allocated = minAllocated;
            for(size_t i = 0; i < size; i++)
            {
                newDigits[i] = digits[i];
            }
            (*referenceCount)--;
            referenceCount = new unsigned(1);
            digits = newDigits;
        }
        else if(minAllocated > allocated)
        {
            minAllocated = max(minAllocated, allocated * 2);
            uint32_t * newDigits = new uint32_t[minAllocated];
            allocated = minAllocated;
            for(size_t i = 0; i < size; i++)
            {
                newDigits[i] = digits[i];
            }
            digits = newDigits;
        }
        for(size_t i = size; i < clearLength; i++)
        {
            digits[i] = 0;
        }
    }
    void normalize()
    {
        if(size == 0)
        {
            assert(allocated > 0);
            size = 1;
            digits[0] = 0;
        }
        while(size > 1 && digits[size - 1] == 0)
        {
            size--;
        }
        if(size == 1 && digits[0] == 0)
        {
            isNegative = false;
        }
    }
    BigInteger(size_t size, bool isNegative)
    {
        this->isNegative = isNegative;
        digits = new uint32_t[size * 2];
        this->size = size;
        this->allocated = size * 2;
        for(size_t i = 0; i < size; i++)
        {
            digits[i] = 0;
        }
        referenceCount = new unsigned(1);
    }
public:
    BigInteger(int64_t value = 0)
    {
        isNegative = (value < 0);
        if(isNegative)
        {
            value = -value;
        }
        digits = new uint32_t[16];
        size = 2;
        allocated = 16;
        digits[0] = static_cast<uint32_t>(value);
        digits[1] = static_cast<uint32_t>(value >> 32);
        referenceCount = new unsigned(1);
        normalize();
    }
    BigInteger(string str, bool allowOctal = false)
    {
        BigInteger v = parse(str, allowOctal);
        (*v.referenceCount)++;
        referenceCount = v.referenceCount;
        size = v.size;
        allocated = v.allocated;
        digits = v.digits;
        isNegative = v.isNegative;
    }
    BigInteger(const BigInteger & rt)
    {
        isNegative = rt.isNegative;
        referenceCount = rt.referenceCount;
        (*referenceCount)++;
        size = rt.size;
        allocated = rt.allocated;
        digits = rt.digits;
    }
    const BigInteger & operator =(const BigInteger & rt)
    {
        if(referenceCount == rt.referenceCount)
        {
            isNegative = rt.isNegative;
            size = rt.size;
            return *this;
        }
        if(*referenceCount > 1)
        {
            (*referenceCount)--;
        }
        else
        {
            delete referenceCount;
            delete []digits;
        }
        isNegative = rt.isNegative;
        referenceCount = rt.referenceCount;
        (*referenceCount)++;
        size = rt.size;
        allocated = rt.allocated;
        digits = rt.digits;
        return *this;
    }
    ~BigInteger()
    {
        if(*referenceCount > 1)
        {
            (*referenceCount)--;
        }
        else
        {
            delete referenceCount;
            delete []digits;
        }
    }

    const bool isZero() const
    {
        if(size == 1 && digits[0] == 0)
        {
            return true;
        }
        return false;
    }

    const BigInteger operator -() const
    {
        if(isZero())
        {
            return *this;
        }
        BigInteger retval(*this);
        retval.isNegative = !isNegative;
        return retval;
    }

    const int sign() const
    {
        if(isZero())
        {
            return 0;
        }
        if(isNegative)
        {
            return -1;
        }
        return 1;
    }

    const bool operator ==(const BigInteger & r) const
    {
        if(sign() < r.sign())
        {
            return false;
        }
        if(sign() > r.sign())
        {
            return false;
        }
        if(isZero())
        {
            return true;
        }
        if(referenceCount == r.referenceCount)
        {
            return true;
        }
        if(size != r.size)
        {
            return false;
        }
        for(size_t i = 0, j = size - 1; i < size; i++, j--)
        {
            if(digits[j] != r.digits[j])
            {
                return false;
            }
        }
        return true;
    }

    const bool operator <(const BigInteger & r) const
    {
        if(sign() < r.sign())
        {
            return true;
        }
        if(sign() > r.sign())
        {
            return false;
        }
        if(isZero())
        {
            return false;
        }
        if(referenceCount == r.referenceCount)
        {
            return false;
        }
        if(size > r.size)
        {
            return isNegative;
        }
        if(size < r.size)
        {
            return !isNegative;
        }
        for(size_t i = 0, j = size - 1; i < size; i++, j--)
        {
            if(digits[j] < r.digits[j])
            {
                return !isNegative;
            }
            if(digits[j] > r.digits[j])
            {
                return isNegative;
            }
        }
        return false;
    }

    const bool operator >(const BigInteger & r) const
    {
        if(sign() < r.sign())
        {
            return false;
        }
        if(sign() > r.sign())
        {
            return true;
        }
        if(isZero())
        {
            return false;
        }
        if(referenceCount == r.referenceCount)
        {
            return false;
        }
        if(size > r.size)
        {
            return !isNegative;
        }
        if(size < r.size)
        {
            return isNegative;
        }
        for(size_t i = 0, j = size - 1; i < size; i++, j--)
        {
            if(digits[j] > r.digits[j])
            {
                return !isNegative;
            }
            if(digits[j] < r.digits[j])
            {
                return isNegative;
            }
        }
        return false;
    }

    const bool operator <=(const BigInteger & r) const
    {
        return !operator >(r);
    }

    const bool operator >=(const BigInteger & r) const
    {
        return !operator <(r);
    }

    const bool operator !=(const BigInteger & r) const
    {
        return !operator ==(r);
    }

    const BigInteger & operator +=(const BigInteger & r)
    {
        if(r.isZero())
        {
            return *this;
        }
        if(isZero())
        {
            return operator =(r);
        }
        handleWrite(max(size, r.size) + 1);
        size_t oldSize = size;
        size = max(size, r.size) + 1;
        for(size_t i = oldSize; i <= r.size; i++)
        {
            digits[i] = 0;
        }
        if((isNegative && r.isNegative) || (!isNegative && !r.isNegative))
        {
            uint32_t carry = 0;
            size_t i;
            for(i = 0; i < r.size; i++)
            {
                uint64_t sum = carry;
                sum += digits[i];
                sum += r.digits[i];
                carry = static_cast<uint32_t>(sum >> 32);
                digits[i] = static_cast<uint32_t>(sum & ((static_cast<uint64_t>(1) << 32) - 1));
            }
            for(; i < size && carry != 0; i++)
            {
                uint64_t sum = carry;
                sum += digits[i];
                carry = static_cast<uint32_t>(sum >> 32);
                digits[i] = static_cast<uint32_t>(sum & ((static_cast<uint64_t>(1) << 32) - 1));
            }
            normalize();
            return *this;
        }
        else
        {
            uint32_t borrow = 0;
            size_t i;
            for(i = 0; i < r.size; i++)
            {
                uint64_t difference = (static_cast<uint64_t>(1) << 32) - borrow;
                difference += digits[i];
                difference -= r.digits[i];
                borrow = 1 - static_cast<uint32_t>(difference >> 32);
                digits[i] = static_cast<uint32_t>(difference & ((static_cast<uint64_t>(1) << 32) - 1));
            }
            for(; i < size && borrow != 0; i++)
            {
                uint64_t difference = (static_cast<uint64_t>(1) << 32) - borrow;
                difference += digits[i];
                borrow = 1 - static_cast<uint32_t>(difference >> 32);
                digits[i] = static_cast<uint32_t>(difference & ((static_cast<uint64_t>(1) << 32) - 1));
            }
            if(borrow != 0)
            {
                isNegative = !isNegative;
                for(i = 0; i < size; i++)
                {
                    if(digits[i] != 0)
                    {
                        break;
                    }
                }
                if(i < size)
                {
                    digits[i] = -digits[i];
                    i++;
                }
                for(; i < size; i++)
                {
                    digits[i] = ~digits[i];
                }
            }
            normalize();
            return *this;
        }
    }

    const BigInteger & operator -=(const BigInteger & r)
    {
        return operator +=(r.operator - ());
    }

    const BigInteger operator +(const BigInteger & r) const
    {
        BigInteger retval(*this);
        return retval += r;
    }

    const BigInteger operator -(const BigInteger & r) const
    {
        BigInteger retval(*this);
        return retval -= r;
    }

    const BigInteger & operator <<=(size_t shiftAmount)
    {
        if(shiftAmount == 0 || isZero())
        {
            return *this;
        }
        size_t newDigitCount = (shiftAmount + 32 - 1) / 32;
        size_t skipDigitCount = shiftAmount / 32;
        size_t digitShiftAmount = shiftAmount % 32;
        handleWrite(size + newDigitCount);
        if(digitShiftAmount == 0)
        {
            size_t size = this->size;
            this->size += skipDigitCount;
            for(size_t i = 0, j = size - 1, k = size + skipDigitCount - 1; i < size; i++, j--, k--)
            {
                digits[k] = digits[j];
            }
            for(size_t i = 0; i < skipDigitCount; i++)
            {
                digits[i] = 0;
            }
            normalize();
            return *this;
        }
        size_t size = this->size;
        this->size += newDigitCount;
        for(size_t i = 0, j = size, k = size + skipDigitCount; i <= size; i++, j--, k--)
        {
            if(j == size)
            {
                digits[k] = digits[j - 1] >> (32 - digitShiftAmount);
            }
            else if(j > 0)
            {
                digits[k] = (digits[j - 1] >> (32 - digitShiftAmount)) | (digits[j] << digitShiftAmount);
            }
            else
            {
                digits[k] = digits[j] << digitShiftAmount;
            }
        }
        for(size_t i = 0; i < skipDigitCount; i++)
        {
            digits[i] = 0;
        }
        normalize();
        return *this;
    }

    const BigInteger & operator >>=(size_t shiftAmount)
    {
        if(shiftAmount == 0 || isZero())
        {
            return *this;
        }
        size_t skipDigitCount = shiftAmount / 32;
        size_t digitShiftAmount = shiftAmount % 32;
        handleWrite(size);
        bool needRound = false;
        if(isNegative)
        {
            for(size_t i = 0; i <= skipDigitCount; i++)
            {
                if(i == skipDigitCount)
                {
                    if(digits[i] - ((digits[i] >> shiftAmount) << shiftAmount) != 0)
                    {
                        needRound = true;
                        break;
                    }
                }
                else if(digits[i] != 0)
                {
                    needRound = true;
                    break;
                }
            }
        }
        if(digitShiftAmount == 0)
        {
            for(size_t i = 0, j = skipDigitCount; j < size; i++, j++)
            {
                digits[i] = digits[j];
            }
            if(skipDigitCount >= size && needRound)
            {
                return *this = BigInteger(-1);
            }
            if(skipDigitCount >= size)
            {
                return *this = BigInteger(0);
            }
            size -= skipDigitCount;
            normalize();
            if(needRound)
            {
                operator -=(BigInteger(1));
            }
            return *this;
        }
        for(size_t i = 0, j = skipDigitCount; i < size - skipDigitCount; i++, j++)
        {
            if(j < size - 1)
            {
                digits[i] = (digits[j + 1] << (32 - digitShiftAmount)) | (digits[j] >> digitShiftAmount);
            }
            else if(j < size)
            {
                digits[i] = digits[j] >> digitShiftAmount;
            }
        }
        size -= skipDigitCount;
        normalize();
        if(needRound)
        {
            operator -=(BigInteger(1));
        }
        return *this;
    }

    const BigInteger operator <<(size_t shiftAmount) const
    {
        BigInteger retval(*this);
        retval <<= shiftAmount;
        return retval;
    }

    const BigInteger operator >>(size_t shiftAmount) const
    {
        BigInteger retval(*this);
        retval >>= shiftAmount;
        return retval;
    }

    const BigInteger operator *(const BigInteger & r) const
    {
        if(size > r.size)
        {
            return r.operator * (*this);
        }
        BigInteger retval(size + r.size + 1, isNegative ^ r.isNegative);
        for(size_t i = 0; i < size; i++)
        {
            uint64_t multiplierDigit = digits[i];
            uint32_t carry = 0;
            for(size_t j = 0; j < r.size; j++)
            {
                uint64_t sum = multiplierDigit * r.digits[j] + carry;
                sum += retval.digits[i + j];
                carry = static_cast<uint32_t>(sum >> 32);
                retval.digits[i + j] = static_cast<uint32_t>(sum); // cut off upper bits
            }
            for(size_t j = r.size + i; j <= r.size + size; j++)
            {
                if(carry == 0)
                {
                    break;
                }
                uint64_t sum = retval.digits[j];
                sum += carry;
                carry = static_cast<uint32_t>(sum >> 32);
                retval.digits[j] = static_cast<uint32_t>(sum); // cut off upper bits
            }
        }
        retval.normalize();
        return retval;
    }

    const BigInteger operator *(uint32_t r) const
    {
        BigInteger retval(size + 1 + 1, isNegative);
        uint64_t multiplierDigit = r;
        uint32_t carry = 0;
        for(size_t j = 0; j < size; j++)
        {
            uint64_t sum = multiplierDigit * digits[j] + carry;
            carry = static_cast<uint32_t>(sum >> 32);
            retval.digits[j] = static_cast<uint32_t>(sum); // cut off upper bits
        }
        retval.digits[size] = carry;
        retval.normalize();
        return retval;
    }

    const BigInteger operator *(int32_t r) const
    {
        bool isRNeg = (r < 0);
        if(isRNeg)
        {
            r = -r;
        }
        BigInteger retval(size + 1 + 1, isNegative ^ isRNeg);
        uint64_t multiplierDigit = static_cast<uint32_t>(r);
        uint32_t carry = 0;
        for(size_t j = 0; j < size; j++)
        {
            uint64_t sum = multiplierDigit * digits[j] + carry;
            carry = static_cast<uint32_t>(sum >> 32);
            retval.digits[j] = static_cast<uint32_t>(sum); // cut off upper bits
        }
        retval.digits[size] = carry;
        retval.normalize();
        return retval;
    }

    friend const BigInteger operator *(uint32_t l, const BigInteger & r)
    {
        return r.operator * (l);
    }

    friend const BigInteger operator *(int32_t l, const BigInteger & r)
    {
        return r.operator * (l);
    }

    const BigInteger & operator *=(const BigInteger & r)
    {
        return operator =(this->operator *(r));
    }

    const BigInteger & operator *=(uint32_t r)
    {
        return operator =(this->operator *(r));
    }

    const BigInteger & operator *=(int32_t r)
    {
        bool isNeg = (r < 0);
        if(isNeg)
        {
            r = -r;
        }
        operator =(this->operator *(r));
        if(isNeg)
        {
            isNegative = !isNegative;
        }
        return *this;
    }

    int64_t toInt64() const
    {
        int64_t v = 0;
        if(size > 1)
        {
            v = static_cast<int64_t>(digits[1]) << 32;
        }
        v |= digits[0];
        if(isNegative)
        {
            return -v;
        }
        return v;
    }
private:
    uint32_t operator [](size_t index) const
    {
        if(index >= size)
            return 0;
        return digits[index];
    }
public:
    friend size_t log2(const BigInteger & v)
    {
        if(v.sign() <= 0)
        {
            throw new domain_error("can't take the log of a value <= 0");
        }
        return (v.size - 1) * 32 + log2(v.digits[v.size - 1]);
    }

    const BigInteger divide(uint32_t divisor, uint32_t & remainder) const
    {
        if(divisor == 0)
        {
            throw new overflow_error("divide by zero");
        }
        BigInteger dividend = *this;
        if(dividend.isZero())
        {
            remainder = 0;
            return BigInteger(0);
        }
        if(dividend.sign() < 0)
        {
            throw new domain_error("can't use divide(uint32_t, uint32_t &) on a negative number");
        }
        if(dividend.size == 1)
        {
            remainder = dividend.digits[0] % divisor;
            return BigInteger(dividend.digits[0] / divisor);
        }
        if(dividend.size == 2)
        {
            uint64_t v = (static_cast<uint64_t>(dividend.digits[1]) << 32) | dividend.digits[0];
            remainder = static_cast<uint32_t>(v % divisor);
            v /= divisor;
            BigInteger quotient = BigInteger(2, false);
            quotient.digits[0] = static_cast<uint32_t>(v);
            quotient.digits[1] = static_cast<uint32_t>(v >> 32);
            quotient.normalize();
            return quotient;
        }
        dividend.handleWrite(dividend.size);
        uint32_t rem = 0;
        for(size_t i = 0, j = dividend.size - 1; i < dividend.size; i++, j--)
        {
            uint64_t v = rem;
            v <<= 32;
            v |= dividend.digits[j];
            rem = v % divisor;
            dividend.digits[j] = v / divisor;
        }
        remainder = rem;
        dividend.normalize();
        return dividend;
    }

    const BigInteger divide(BigInteger divisor, BigInteger & remainder) const
    {
        if(divisor.isZero())
        {
            throw new overflow_error("divide by zero");
        }
        if(abs(*this) < abs(divisor))
        {
            remainder = *this;
            return BigInteger(0);
        }
        if(abs(*this) == abs(divisor))
        {
            remainder = BigInteger(0);
            if(isNegative ^ divisor.isNegative)
                return BigInteger(-1);
            return BigInteger(1);
        }
        if(divisor.size == 1)
        {
            uint32_t rem;
            BigInteger quotient = abs(*this).divide(divisor.digits[0], rem);
            remainder = BigInteger(1, isNegative);
            remainder.digits[0] = rem;
            remainder.normalize();
            if(isNegative ^ divisor.isNegative)
                return -quotient;
            return quotient;
        }
#if 1
        size_t dividendScale = log2(divisor) + 1;
        size_t divisorScale = 32;
        bool divisorSign = divisor.isNegative;
        divisor.isNegative = false;
        BigInteger oldDivisor = divisor;
        divisor <<= divisorScale;
        divisorScale += dividendScale;
        BigInteger x = BigInteger(3) << (divisorScale - 1); // 1.5
        BigInteger one = BigInteger(1) << divisorScale;
        BigInteger eps = BigInteger(1);
        BigInteger lastX = x;
        do
        {
            lastX = x;
            x = x + (x * (one - (divisor * x >> divisorScale)) >> divisorScale);
        }
        while(abs(lastX - x) > eps);
        BigInteger quotient = x * abs(*this);
        quotient >>= (dividendScale + divisorScale);
        remainder = abs(*this) - quotient * abs(oldDivisor);
        while(remainder < 0)
        {
            remainder += abs(oldDivisor);
            quotient -= 1;
        }
        while(remainder >= abs(oldDivisor))
        {
            remainder -= abs(oldDivisor);
            quotient += 1;
        }
        if(isNegative)
            remainder = -remainder;
        if(isNegative ^ divisorSign)
            quotient = -quotient;
        return quotient;
#else // old slow algorithm
        BigInteger dividend = abs(*this);
        BigInteger quotient = BigInteger(dividend.size, isNegative ^ divisor.isNegative);
        divisor = abs(divisor);
        size_t count = log2(dividend) - log2(divisor) + 2;
        BigInteger scaledDivisor = divisor << count;
        for(size_t i = 0, j = count; i <= count; i++, j--)
        {
            if(dividend >= scaledDivisor)
            {
                quotient.digits[j / 32] |= static_cast<uint32_t>(1) << (j % 32);
                dividend -= scaledDivisor;
            }
            scaledDivisor >>= 1;
        }
        remainder = dividend;
        remainder.isNegative = isNegative;
        quotient.normalize();
        return quotient;
#endif
    }

    const BigInteger operator /(const BigInteger & r) const
    {
        BigInteger remainder;
        return divide(r, remainder);
    }

    const BigInteger operator %(const BigInteger & r) const
    {
        BigInteger remainder;
        divide(r, remainder);
        return remainder;
    }

    const BigInteger operator /=(const BigInteger & r)
    {
        return *this = *this / r;
    }

    const BigInteger operator %=(const BigInteger & r)
    {
        return *this = *this % r;
    }

    const BigInteger operator ~() const
    {
        return BigInteger(-1) - *this;
    }

    const BigInteger & operator &=(BigInteger r)
    {
        if(isZero())
        {
            return *this;
        }
        if(r.isZero())
        {
            *this = r;
            return *this;
        }
        size_t newSize = max(size, r.size);
        if(!isNegative)
        {
            newSize = size;
        }
        handleWrite(newSize);
        size = newSize;
        bool carryA = isNegative, carryB = r.isNegative;
        bool newSign = isNegative && r.isNegative;
        bool carryResult = newSign;
        for(size_t i = 0; i < size; i++)
        {
            uint32_t a = digits[i], b = 0;
            if(i < r.size)
            {
                b = r.digits[i];
            }
            if(r.isNegative)
            {
                b = ~b;
            }
            if(isNegative)
            {
                a = ~a;
            }
            if(carryA)
            {
                if(a == ~static_cast<uint32_t>(0))
                {
                    a = 0;
                }
                else
                {
                    a++;
                    carryA = false;
                }
            }
            if(carryB)
            {
                if(b == ~static_cast<uint32_t>(0))
                {
                    b = 0;
                }
                else
                {
                    b++;
                    carryB = false;
                }
            }
            uint32_t result = a & b;
            if(newSign)
            {
                result = ~result;
            }
            if(carryResult)
            {
                if(result == ~static_cast<uint32_t>(0))
                {
                    result = 0;
                }
                else
                {
                    carryResult = false;
                    result++;
                }
            }
            digits[i] = result;
        }
        isNegative = newSign;
        normalize();
        return *this;
    }

    friend const BigInteger operator &(BigInteger l, const BigInteger & r)
    {
        return l &= r;
    }

    const BigInteger & operator |=(BigInteger r)
    {
        if(r.isZero())
        {
            return *this;
        }
        if(isZero())
        {
            *this = r;
            return *this;
        }
        size_t newSize = max(size, r.size);
        if(isNegative)
        {
            newSize = size;
        }
        handleWrite(newSize);
        size = newSize;
        bool carryA = isNegative, carryB = r.isNegative;
        bool newSign = isNegative || r.isNegative;
        bool carryResult = newSign;
        for(size_t i = 0; i < size; i++)
        {
            uint32_t a = digits[i], b = 0;
            if(i < r.size)
            {
                b = r.digits[i];
            }
            if(r.isNegative)
            {
                b = ~b;
            }
            if(isNegative)
            {
                a = ~a;
            }
            if(carryA)
            {
                if(a == ~static_cast<uint32_t>(0))
                {
                    a = 0;
                }
                else
                {
                    a++;
                    carryA = false;
                }
            }
            if(carryB)
            {
                if(b == ~static_cast<uint32_t>(0))
                {
                    b = 0;
                }
                else
                {
                    b++;
                    carryB = false;
                }
            }
            uint32_t result = a | b;
            if(newSign)
            {
                result = ~result;
            }
            if(carryResult)
            {
                if(result == ~static_cast<uint32_t>(0))
                {
                    result = 0;
                }
                else
                {
                    carryResult = false;
                    result++;
                }
            }
            digits[i] = result;
        }
        isNegative = newSign;
        normalize();
        return *this;
    }

    friend const BigInteger operator |(BigInteger l, const BigInteger & r)
    {
        return l |= r;
    }

    const BigInteger & operator ^=(BigInteger r)
    {
        if(r.isZero())
        {
            return *this;
        }
        if(isZero())
        {
            *this = r;
            return *this;
        }
        size_t newSize = max(size, r.size);
        if(isNegative)
        {
            newSize = size;
        }
        handleWrite(newSize);
        size = newSize;
        bool carryA = isNegative, carryB = r.isNegative;
        bool newSign = isNegative ^ r.isNegative;
        bool carryResult = newSign;
        for(size_t i = 0; i < size; i++)
        {
            uint32_t a = digits[i], b = 0;
            if(i < r.size)
            {
                b = r.digits[i];
            }
            if(r.isNegative)
            {
                b = ~b;
            }
            if(isNegative)
            {
                a = ~a;
            }
            if(carryA)
            {
                if(a == ~static_cast<uint32_t>(0))
                {
                    a = 0;
                }
                else
                {
                    a++;
                    carryA = false;
                }
            }
            if(carryB)
            {
                if(b == ~static_cast<uint32_t>(0))
                {
                    b = 0;
                }
                else
                {
                    b++;
                    carryB = false;
                }
            }
            uint32_t result = a ^ b;
            if(newSign)
            {
                result = ~result;
            }
            if(carryResult)
            {
                if(result == ~static_cast<uint32_t>(0))
                {
                    result = 0;
                }
                else
                {
                    carryResult = false;
                    result++;
                }
            }
            digits[i] = result;
        }
        isNegative = newSign;
        normalize();
        return *this;
    }

    friend const BigInteger operator ^(BigInteger l, const BigInteger & r)
    {
        return l ^= r;
    }

    friend const BigInteger abs(BigInteger v)
    {
        v.isNegative = false;
        return v;
    }

    friend const BigInteger gcd(BigInteger a, BigInteger b)
    {
        if(a.isZero() || b.isZero())
        {
            return BigInteger(0);
        }
        a = abs(a);
        b = abs(b);
        for(;;)
        {
            BigInteger c = a % b;
            if(c.isZero())
            {
                return b;
            }
            a = b;
            b = c;
        }
    }



private:
    static void writeHelper(ostream & os, BigInteger v, size_t expectedLength)
    {
        BigInteger divisor("10000000000000000000");
        const size_t log10Divisor = 19;
        if(v >= divisor)
        {
            size_t l = log10Divisor;
            BigInteger nextPower = divisor * divisor;
            while(v >= nextPower)
            {
                divisor = nextPower;
                nextPower = divisor * divisor;
                l *= 2;
            }
            BigInteger remainder;
            writeHelper(os, v.divide(divisor, remainder), (expectedLength > l ? expectedLength - l : 0));
            writeHelper(os, remainder, l);
        }
        else
        {
            uint64_t r = v.toInt64();
            char str[log10Divisor + 1] = {0};
            for(size_t i = 0; i < log10Divisor; i++)
            {
                str[log10Divisor - i - 1] = static_cast<char>(r % 10 + '0');
                r /= 10;
            }
            char * strp = &str[0];
            size_t l = strlen(strp);
            while(*strp == '0' && expectedLength < l)
            {
                strp++;
                l--;
            }
            for(size_t i = l; i < expectedLength; i++)
            {
                os << '0';
            }
            if(*strp)
            {
                os << strp;
            }
        }
    }
public:
    friend ostream & operator <<(ostream & os, BigInteger v)
    {
        if(v.sign() < 0)
        {
            os << '-';
            v = -v;
        }
        if((os.flags() & os.basefield) == os.hex)
        {
            os << "0x";
            bool isFirst = true;
            for(size_t i = 0, j = v.size - 1; i < v.size; i++, j--)
            {
                for(int k = 32 - 4; k >= 0; k -= 4)
                {
                    uint32_t digit = v.digits[j] >> k;
                    digit &= 0xF;
                    if(isFirst && digit == 0)
                    {
                        continue;
                    }
                    isFirst = false;
                    if(digit >= 0xA)
                    {
                        os << static_cast<char>(digit + 'A' - '\xA');
                    }
                    else
                    {
                        os << static_cast<char>(digit + '0');
                    }
                }
            }
            if(isFirst)
            {
                return os << '0';
            }
            return os;
        }
        if(v.isZero())
            return os << "0";
        writeHelper(os, v, true);
        return os;
    }

    explicit operator string() const
    {
        ostringstream os;
        os << *this;
        return os.str();
    }

    static const BigInteger parseHex(string v)
    {
        const char * str = v.c_str();
        BigInteger retval(0);
        while(*str == ' ' || *str == '\t')
        {
            str++;
        }
        bool isNegative = false;
        if(*str == '-' || *str == '+')
        {
            isNegative = (*str == '-');
            str++;
        }
        while(*str && isxdigit(*str))
        {
            int digit;
            if(isupper(*str))
            {
                digit = *str - 'A' + 0xA;
            }
            else if(islower(*str))
            {
                digit = *str - 'a' + 0xA;
            }
            else
            {
                digit = *str - '0';
            }
            retval <<= 4;
            retval += BigInteger(digit);
            str++;
        }
        if(isNegative)
        {
            retval = -retval;
        }
        return retval;
    }

    static const BigInteger parseOct(string v)
    {
        const char * str = v.c_str();
        BigInteger retval(0);
        while(*str == ' ' || *str == '\t')
        {
            str++;
        }
        bool isNegative = false;
        if(*str == '-' || *str == '+')
        {
            isNegative = (*str == '-');
            str++;
        }
        while(*str && isdigit(*str))
        {
            int digit = *str - '0';
            retval <<= 3;
            retval += BigInteger(digit);
            str++;
        }
        if(isNegative)
        {
            retval = -retval;
        }
        return retval;
    }

    static const BigInteger parseDec(string v)
    {
        const char * str = v.c_str();
        BigInteger retval(0);
        while(*str == ' ' || *str == '\t')
        {
            str++;
        }
        bool isNegative = false;
        if(*str == '-' || *str == '+')
        {
            isNegative = (*str == '-');
            str++;
        }
        while(*str && isdigit(*str))
        {
            int digit = *str - '0';
            retval *= BigInteger(10);
            retval += BigInteger(digit);
            str++;
        }
        if(isNegative)
        {
            retval = -retval;
        }
        return retval;
    }

    static const BigInteger parse(string v, bool allowOctal = false)
    {
        const char * str = v.c_str();
        bool isNegative = false;
        while(*str == ' ' || *str == '\t')
        {
            str++;
        }
        if(*str == '-' || *str == '+')
        {
            isNegative = (*str == '-');
            str++;
        }
        BigInteger retval;
        if(*str == '0')
        {
            if(str[1] == 'x' || str[1] == 'X')
            {
                retval = parseHex(str + 2);
            }
            else if(!allowOctal)
            {
                retval = parseDec(str);
            }
            else
            {
                retval = parseOct(str);
            }
        }
        else
        {
            retval = parseDec(str);
        }
        if(isNegative)
        {
            return -retval;
        }
        return retval;
    }

    struct BigIntegerIStreamHelper
    {
        BigInteger & v;
        bool allowOctal;
        BigIntegerIStreamHelper(BigInteger & v, bool allowOctal = false)
            : v(v), allowOctal(allowOctal)
        {
        }
    };

    friend BigIntegerIStreamHelper allowOctal(BigInteger & v)
    {
        return BigIntegerIStreamHelper(v, true);
    }

    friend istream & operator >>(istream & is, BigInteger & v)
    {
        return is >> BigIntegerIStreamHelper(v);
    }

    friend istream & operator >>(istream & is, BigIntegerIStreamHelper arg)
    {
        BigInteger & v = arg.v;
        char signOrDigit;
        is >> signOrDigit;
        if(!is)
        {
            return is;
        }
        bool isNegative = false;
        v = BigInteger(0);
        bool gotDigit = false;
        if(signOrDigit == '+' || signOrDigit == '-')
        {
            if(signOrDigit == '-')
            {
                isNegative = true;
            }
        }
        else if(!isdigit(signOrDigit))
        {
            is.putback(signOrDigit);
            is.setstate(istream::failbit);
            return is;
        }
        else if(signOrDigit == '0')
        {
            if(is.peek() == 'x' || is.peek() == 'X')
            {
                char X = is.get();
                if(isxdigit(is.peek()))
                {
                    v = BigInteger(0);
                    while(isxdigit(is.peek()))
                    {
                        int digit;
                        if(isupper(is.peek()))
                        {
                            digit = is.peek() - 'A' + 0xA;
                        }
                        else if(islower(is.peek()))
                        {
                            digit = is.peek() - 'a' + 0xA;
                        }
                        else
                        {
                            digit = is.peek() - '0';
                        }
                        v <<= 4;
                        v += BigInteger(digit);
                        is.get();
                    }
                    return is;
                }
                else
                {
                    is.putback(X);
                    v = BigInteger(0);
                    return is;
                }
            }
            else if(!arg.allowOctal)
            {
                v = BigInteger(0);
                while(isdigit(is.peek()))
                {
                    v *= BigInteger(10);
                    v += BigInteger(is.get() - '0');
                }
                return is;
            }
            else
            {
                v = BigInteger(0);
                while(isdigit(is.peek()))
                {
                    v <<= 3;
                    v += BigInteger(is.get() - '0');
                }
                return is;
            }
        }
        else
        {
            v = BigInteger(signOrDigit - '0');
            gotDigit = true;
        }
        while(isdigit(is.peek()) && is)
        {
            v *= BigInteger(10);
            v += BigInteger(is.get() - '0');
            gotDigit = true;
        }
        if(!gotDigit)
        {
            is.setstate(istream::failbit);
        }
        if(isNegative)
        {
            v = -v;
        }
        return is;
    }

    friend const BigInteger pow(BigInteger base, BigInteger exponent)
    {
        if(exponent.sign() < 0)
        {
            throw new domain_error("can't use modPow with exponent < 0");
        }
        BigInteger retval = base;
        if((exponent & BigInteger(1)).isZero())
        {
            retval = BigInteger(1);
        }
        exponent &= ~BigInteger(1);
        for(BigInteger v = BigInteger(2); !exponent.isZero(); v <<= 1)
        {
            base *= base;
            if(!(exponent & v).isZero())
            {
                exponent &= ~v;
                retval *= base;
            }
        }
        return retval;
    }

    friend const BigInteger modPow(BigInteger base, BigInteger exponent, BigInteger modulus)
    {
        if(exponent.sign() < 0)
        {
            throw new domain_error("can't use modPow with exponent < 0");
        }
        if(abs(modulus) <= BigInteger(1))
        {
            return BigInteger(0);
        }
        base %= modulus;
        BigInteger retval = base;
        if((exponent & BigInteger(1)).isZero())
        {
            retval = BigInteger(1);
        }
        exponent &= ~BigInteger(1);
        for(BigInteger v = BigInteger(2); !exponent.isZero(); v <<= 1)
        {
            base *= base;
            base %= modulus;
            if(!(exponent & v).isZero())
            {
                exponent &= ~v;
                retval *= base;
                retval %= modulus;
            }
        }
        return retval;
    }

    static BigInteger random(size_t bits, bool useSecureRandom = false)
    {
        if(bits > 10)
        {
            size_t a = bits / 2;
            return (random(bits - a, useSecureRandom) << a) | random(a, useSecureRandom);
        }
        return BigInteger(randomDigit(1 << bits, useSecureRandom));
    }

    friend bool isProbablePrime(BigInteger n, size_t log2Probability = 100, bool useSecureRandom = false)
    {
        if(n <= BigInteger(1))
            return false;
        if(n <= BigInteger(3))
            return true;
        size_t k = (log2Probability + 1) / 2;
        if((n >> 1) << 1 == n)
            return false;
        if(n % BigInteger(3) == BigInteger(0))
            return false;
        if(n == BigInteger(5))
            return true;
        if(n % BigInteger(5) == BigInteger(0))
            return false;
        if(n == BigInteger(7))
            return true;
        if(n % BigInteger(7) == BigInteger(0))
            return false;
        if(n == BigInteger(11))
            return true;
        if(n % BigInteger(11) == BigInteger(0))
            return false;
        if(n == BigInteger(13))
            return true;
        if(n % BigInteger(13) == BigInteger(0))
            return false;
        if(n <= BigInteger(13 * 13))
            return true;
        BigInteger d = n - BigInteger(1);
        size_t s = 0;
        while((d & BigInteger(1)).isZero())
        {
            d >>= 1;
            s++;
        }
        for(size_t i = 0; i < k; i++)
        {
            BigInteger rv = BigInteger::random(2 + log2(n), useSecureRandom) % (n - BigInteger(3)) + BigInteger(2);
            BigInteger x = modPow(rv, d, n);
            if(x == BigInteger(1) || x == n - BigInteger(1))
                continue;
            for(size_t j = 1; ; j++)
            {
                if(j >= s)
                    return false;
                x = (x * x) % n;
                if(x == BigInteger(1))
                    return false;
                if(x == n - BigInteger(1))
                    break;
            }
        }
        return true;
    }

    static BigInteger makeProbablePrime(size_t bits, size_t log2Probability = 100, bool useGenSecureRandom = true, bool useTestSecureRandom = false)
    {
        if(bits < 3)
            bits = 3;
        int dotCount = 1;
        cout << "\x1B[s\n";
        for(;;)
        {
            BigInteger n = (random(bits - 2, useGenSecureRandom) << 1) | (BigInteger(1) << bits) | BigInteger(1);
            cout << "Testing";
            for(int i = 0; i < dotCount; i++)
                cout << ".";
            cout << "\x1B[K\r";
            cout.flush();
            dotCount %= 10;
            dotCount++;
            if(isProbablePrime(n, log2Probability, useTestSecureRandom))
            {
                cout << "\x1B[K\x1B[u";
                cout.flush();
                return n;
            }
        }
    }

    BigInteger modularInverse(const BigInteger modulus) const
    {
        BigInteger t = BigInteger(0), newT = BigInteger(1);
        BigInteger r = modulus;
        BigInteger newR = *this;
        while(!newR.isZero())
        {
            BigInteger quotient = r / newR;
            BigInteger temp = t - quotient * newT;
            t = newT;
            newT = temp;
            temp = r - quotient * newR;
            r = newR;
            newR = temp;
        }
        if(r > BigInteger(1))
            throw new domain_error("there is no inverse");
        if(t.sign() < 0)
            t += modulus;
        return t;
    }

    friend BigInteger isqrt(BigInteger v)
    {
        if(v.isZero())
            return v;
        if(v.isNegative)
        {
            throw new domain_error("can't use isqrt on negative numbers");
        }
        BigInteger origV = v;
        size_t vScale = log2(v);
        vScale -= vScale % 2;
        size_t scale = vScale;
        vScale += 8;
        v <<= 8;
        BigInteger x = BigInteger(1) << vScale;
        BigInteger lastX = x;
        BigInteger eps = BigInteger(2);
        do
        {
            lastX = x;
            x = x + (v << vScale) / x;
            x >>= 1;
        }
        while(abs(x - lastX) > eps);
        x >>= (vScale - scale / 2);
        BigInteger xSq = x * x;
        v = origV;
        if(xSq > v)
        {
            x -= 1;
            xSq -= (x << 1) + 1;
        }
        return x;
    }

    string convertToASCII() const
    {
        size_t bitLength = log2(*this);
        if(bitLength % 8 != 0)
        {
            throw new domain_error("wrong bit length for convertToASCII");
        }
        BigInteger v = *this;
        string retval = "";
        retval.reserve(bitLength / 8);
        for(size_t i = 0; i < bitLength / 8; i++)
        {
            unsigned char ch = (v & BigInteger(0xFF)).toInt64();
            retval += ch;
            v >>= 8;
        }
        return retval;
    }

    static BigInteger convertFromASCII(string s)
    {
        BigInteger retval = BigInteger(1);
        for(string::const_reverse_iterator i = s.rbegin(); i != s.rend(); i++)
        {
            retval <<= 8;
            retval += BigInteger((unsigned char)*i);
        }
        return retval;
    }
};


#endif // BIG_INTEGER_H_INCLUDED
