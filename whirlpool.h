#ifndef WHIRLPOOL_H_INCLUDED
#define WHIRLPOOL_H_INCLUDED

#include <string>
#include <stdexcept>
#include "nessie.h"

using namespace std;

class Whirlpool
{
private:
    NESSIEstruct s;
    bool done;
public:
    Whirlpool()
    {
        NESSIEinit(&s);
        done = false;
    }
    string close(bool asHex = false)
    {
        if(done)
            return "";
        unsigned char retval[DIGESTBYTES * 2];
        NESSIEfinalize(&s, retval);
        done = true;
        if(asHex)
        {
            for(size_t i = DIGESTBYTES * 2 - 2, j = DIGESTBYTES - 1, k = 0; k < DIGESTBYTES; k++, i -= 2, j--)
            {
                unsigned v = retval[j];
                char ch1 = ((v & 0xF0) >> 4) + '0';
                if(ch1 >= '0' + 0xA)
                    ch1 += '0' - 0xA + 'A';
                char ch2 = (v & 0xF) + '0';
                if(ch2 >= '0' + 0xA)
                    ch2 += '0' - 0xA + 'A';
                retval[i] = ch1;
                retval[i + 1] = ch2;
            }
            return string(reinterpret_cast<const char *>(retval), DIGESTBYTES * 2);
        }
        return string(reinterpret_cast<const char *>(retval), DIGESTBYTES);
    }
    Whirlpool & add(unsigned char data)
    {
        if(done)
            throw new logic_error("already closed");
        NESSIEadd(&data, 8, &s);
        return *this;
    }
    Whirlpool & add(const unsigned char * data, size_t length)
    {
        if(done)
            throw new logic_error("already closed");
        if(length == 0)
            return *this;
        NESSIEadd(data, 8 * length, &s);
        return *this;
    }
    Whirlpool & add(string data)
    {
        NESSIEadd(reinterpret_cast<const unsigned char *>(data.data()), 8 * data.length(), &s);
        return *this;
    }
};

#endif // WHIRLPOOL_H_INCLUDED
