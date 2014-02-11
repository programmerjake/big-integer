#ifndef FRACTION_H_INCLUDED
#define FRACTION_H_INCLUDED

#include "big_integer.h"

class Fraction
{
private:
    BigInteger n, d;
    void normalize()
    {
        BigInteger divisor = gcd(n, d);
        if(divisor > BigInteger("1"))
        {
            n /= divisor;
            d /= divisor;
        }
        if(d.sign() < 0)
        {
            n = -n;
            d = -d;
        }
    }
public:
    Fraction(BigInteger v = BigInteger(0))
    {
        n = v;
        d = BigInteger(1);
    }
    Fraction(int64_t v)
    {
        n = BigInteger(v);
        d = BigInteger(1);
    }
    Fraction(BigInteger numerator, BigInteger denominator)
    {
        n = numerator;
        d = denominator;
        normalize();
    }
    Fraction(string str)
    {
        size_t splitpos = str.find_first_of('/');
        if(splitpos == string::npos)
        {
            n = BigInteger(str);
            d = BigInteger(1);
        }
        else
        {
            n = BigInteger(str.substr(0, splitpos));
            d = BigInteger(str.substr(splitpos + 1));
        }
        normalize();
    }
    Fraction operator -() const
    {
        return Fraction(-n, d);
    }
    friend ostream & operator <<(ostream & os, Fraction v)
    {
        if(v.d == BigInteger(1))
        {
            return os << v.n;
        }
        return os << v.n << "/" << v.d;
    }
    friend Fraction operator +(Fraction a, Fraction b)
    {
        return Fraction(a.n * b.d + b.n * a.d, a.d * b.d);
    }
    friend Fraction operator +(BigInteger a, Fraction b)
    {
        return Fraction(a * b.d + b.n, b.d);
    }
    friend Fraction operator +(Fraction a, BigInteger b)
    {
        return Fraction(a.n + b * a.d, a.d);
    }
    friend Fraction operator -(Fraction a, Fraction b)
    {
        return Fraction(a.n * b.d - b.n * a.d, a.d * b.d);
    }
    friend Fraction operator -(BigInteger a, Fraction b)
    {
        return Fraction(a * b.d - b.n, b.d);
    }
    friend Fraction operator -(Fraction a, BigInteger b)
    {
        return Fraction(a.n - b * a.d, a.d);
    }
    friend Fraction operator *(Fraction a, Fraction b)
    {
        return Fraction(a.n * b.n, a.d * b.d);
    }
    friend Fraction operator *(BigInteger a, Fraction b)
    {
        return Fraction(a * b.n, b.d);
    }
    friend Fraction operator *(Fraction a, BigInteger b)
    {
        return Fraction(a.n * b, a.d);
    }
    friend Fraction operator /(Fraction a, Fraction b)
    {
        if(b.n.isZero())
        {
            throw new overflow_error("divide by zero");
        }
        return Fraction(a.n * b.d, a.d * b.n);
    }
    friend Fraction operator /(BigInteger a, Fraction b)
    {
        if(b.n.isZero())
        {
            throw new overflow_error("divide by zero");
        }
        return Fraction(a * b.d, b.n);
    }
    friend Fraction operator /(Fraction a, BigInteger b)
    {
        if(b.isZero())
        {
            throw new overflow_error("divide by zero");
        }
        return Fraction(a.n, a.d * b);
    }
    const Fraction & operator +=(Fraction b)
    {
        return *this = *this + b;
    }
    const Fraction & operator -=(Fraction b)
    {
        return *this = *this - b;
    }
    const Fraction & operator *=(Fraction b)
    {
        return *this = *this * b;
    }
    const Fraction & operator /=(Fraction b)
    {
        return *this = *this / b;
    }
    const Fraction & operator +=(BigInteger b)
    {
        return *this = *this + b;
    }
    const Fraction & operator -=(BigInteger b)
    {
        return *this = *this - b;
    }
    const Fraction & operator *=(BigInteger b)
    {
        return *this = *this * b;
    }
    const Fraction & operator /=(BigInteger b)
    {
        return *this = *this / b;
    }
    friend istream & operator >>(istream & is, Fraction & v)
    {
        v.n = BigInteger(0);
        v.d = BigInteger(1);
        is >> v.n;
        if(!is)
        {
            return is;
        }
        if(is.peek() != '/')
        {
            v.normalize();
            return is;
        }
        char slash;
        is >> slash;
        if(!is)
        {
            return is;
        }
        if(slash != '/')
        {
            is.putback(slash);
            is.setstate(istream::failbit);
            return is;
        }
        is >> v.d;
        if(!is)
        {
            return is;
        }
        if(v.d.isZero())
        {
            is.setstate(istream::failbit);
            return is;
        }
        v.normalize();
        return is;
    }
    friend Fraction operator %(Fraction a, Fraction b)
    {
        if(b.n.isZero())
        {
            throw new overflow_error("divide by zero");
        }
        a.n *= b.d;
        b.n *= a.d;
        a.d = b.d *= a.d;
        a.n %= b.n;
        a.normalize();
        return a;
    }
    friend Fraction operator %(BigInteger a, Fraction b)
    {
        if(b.n.isZero())
        {
            throw new overflow_error("divide by zero");
        }
        a *= b.d;
        BigInteger d = b.d;
        a %= b.n;
        return Fraction(a, d);
    }
    const Fraction & operator %=(const Fraction & b)
    {
        return *this = *this % b;
    }
    friend Fraction operator %(Fraction a, BigInteger b)
    {
        if(b.isZero())
        {
            throw new overflow_error("divide by zero");
        }
        b *= a.d;
        a.n %= b;
        a.normalize();
        return a;
    }
    const Fraction & operator %=(const BigInteger & b)
    {
        return *this = *this % b;
    }
    friend Fraction pow(Fraction base, BigInteger exponent)
    {
        if(exponent.sign() < 0)
        {
            return Fraction(1) / pow(base, -exponent);
        }
        Fraction retval = base;
        if((exponent & BigInteger(1)).isZero())
        {
            retval = Fraction(1);
        }
        exponent &= ~BigInteger(1);
        for(BigInteger v = BigInteger(2); !exponent.isZero(); v <<= 1)
        {
            base.n *= base.n;
            base.d *= base.d;
            if(!(exponent & v).isZero())
            {
                exponent &= ~v;
                retval.n *= base.n;
                retval.d *= base.d;
            }
        }
        return retval;
    }
    friend bool operator ==(Fraction a, Fraction b)
    {
        return a.n == b.n && a.d == b.d;
    }
    friend bool operator <(Fraction a, Fraction b)
    {
        return a.n * b.d < b.n * a.d;
    }
    friend bool operator >(Fraction a, Fraction b)
    {
        return a.n * b.d > b.n * a.d;
    }
    friend bool operator !=(Fraction a, Fraction b)
    {
        return a.n != b.n || a.d != b.d;
    }
    friend bool operator <=(Fraction a, Fraction b)
    {
        return a.n * b.d <= b.n * a.d;
    }
    friend bool operator >=(Fraction a, Fraction b)
    {
        return a.n * b.d >= b.n * a.d;
    }
    bool isZero() const
    {
        return n.isZero();
    }
    int sign() const
    {
        return n.sign();
    }
    friend Fraction modPow(Fraction base, BigInteger exponent, Fraction modulus)
    {
        if(exponent.sign() < 0)
        {
            throw new domain_error("can't use modPow with exponent < 0");
        }
        if(modulus == Fraction(0))
        {
            return Fraction(0);
        }
        base %= modulus;
        Fraction retval = base;
        if((exponent & BigInteger(1)).isZero())
        {
            retval = Fraction(1);
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

    friend Fraction setDenominator(Fraction f, BigInteger denominator)
    {
        if(denominator.sign() <= 0)
        {
            throw new domain_error("can't use setDenominator with denominator <= 0");
        }
        Fraction adjustedFraction = f * denominator + Fraction(BigInteger(1), BigInteger(2));
        return Fraction(floor(adjustedFraction), denominator);
    }
    friend BigInteger ceil(Fraction v)
    {
        if(v.sign() < 0)
        {
            return -floor(-v);
        }
        v.n += v.d - BigInteger(1);
        return v.n / v.d;
    }
    friend BigInteger floor(Fraction v)
    {
        if(v.sign() < 0)
        {
            return -ceil(-v);
        }
        return v.n / v.d;
    }
    const BigInteger & getNumerator() const
    {
        return n;
    }
    const BigInteger & getDenominator() const
    {
        return d;
    }
    friend Fraction abs(Fraction f)
    {
        f.n = abs(f.n);
        return f;
    }
    string getDecimal(size_t fractionalDigits = 15) const
    {
        BigInteger pow10 = pow(BigInteger("10"), fractionalDigits);
        Fraction f = setDenominator(*this, pow10);
        bool isNegative = (f.sign() < 0);
        f = abs(f);
        BigInteger intPart = floor(f);
        if(fractionalDigits == 0)
        {
            return (isNegative ? "-" : "") + static_cast<string>(floor(f));
        }
        string digits = static_cast<string>(floor((f - intPart) * pow10));
        while(digits.length() < fractionalDigits)
        {
            digits = "0" + digits;
        }
        string retval = static_cast<string>(intPart) + "." + digits;
        if(isNegative)
        {
            retval = "-" + retval;
        }
        return retval;
    }
    friend Fraction sqrt(Fraction v, BigInteger denominator)
    {
        if(denominator.sign() <= 0)
        {
            throw new domain_error("can't use setDenominator with denominator <= 0");
        }
        if(v.sign() < 0)
        {
            throw new domain_error("can't use sqrt with v < 0");
        }
        BigInteger denominatorSq = denominator * denominator;
        Fraction adjustedFraction = v * denominatorSq + Fraction(BigInteger(1), BigInteger(2));
        return Fraction(isqrt(floor(adjustedFraction)), denominator);
    }
};

#endif // FRACTION_H_INCLUDED
