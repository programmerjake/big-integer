#include "fraction.h"
#include <iostream>

using namespace std;

struct BadKey : public runtime_error
{
    BadKey()
        : runtime_error("bad key")
    {
    }
};

class RSAEncryptionKey
{
private:
    BigInteger exponent;
    BigInteger modulus;
    void check()
    {
        if(exponent <= 1 || modulus < (BigInteger(1) << 120))
            throw new BadKey();
    }
public:
    RSAEncryptionKey(BigInteger exponent, BigInteger modulus)
        : exponent(exponent), modulus(modulus)
    {
        check();
    }
    RSAEncryptionKey(istream & is)
    {
        is >> exponent >> modulus;
        if(is)
            check();
    }
    const BigInteger getMaxInput() const
    {
        return modulus;
    }
    const BigInteger encrypt(BigInteger v) const
    {
        return modPow(v, exponent, modulus);
    }
    const BigInteger decryptSignature(BigInteger v) const
    {
        return modPow(v, exponent, modulus);
    }
    friend ostream & operator <<(ostream & os, const RSAEncryptionKey & k)
    {
        return os << k.exponent << " " << k.modulus;
    }
    ostream * encrypt(ostream & os, bool autoDelete = false) const;
    ostream * encrypt(ostream * os, bool autoDelete = true) const
    {
        return encrypt(*os, autoDelete);
    }
};

class RSADecryptionKey
{
private:
    BigInteger exponent;
    BigInteger modulus;
    void check()
    {
        if(exponent <= 1 || modulus < (BigInteger(1) << 120))
            throw new BadKey();
    }
public:
    RSADecryptionKey(BigInteger exponent, BigInteger modulus)
        : exponent(exponent), modulus(modulus)
    {
        check();
    }
    RSADecryptionKey(istream & is)
    {
        is >> exponent >> modulus;
        if(is)
            check();
    }
    const BigInteger getMaxInput() const
    {
        return modulus;
    }
    const BigInteger encryptSignature(BigInteger v) const
    {
        return modPow(v, exponent, modulus);
    }
    const BigInteger decrypt(BigInteger v) const
    {
        return modPow(v, exponent, modulus);
    }
    friend ostream & operator <<(ostream & os, const RSADecryptionKey & k)
    {
        return os << k.exponent << " " << k.modulus;
    }
    istream * decrypt(istream & os, bool autoDelete = false) const;
    istream * decrypt(istream * os, bool autoDelete = true) const
    {
        return decrypt(*os, autoDelete);
    }
};

const size_t paddingLength = 16;

class RSAEncryptStreamBuf : public streambuf
{
private:
    ostream & os;
    bool autoDelete;
    RSAEncryptionKey k;
    string buffer;
    size_t bufferLen;
    void encryptBuffer()
    {
        if(buffer.length() == 0)
            return;
        BigInteger v = BigInteger::convertFromASCII(buffer);
        v <<= paddingLength * 8;
        v |= BigInteger::random(paddingLength * 8, true);
        BigInteger encrypted = k.encrypt(v);
        cout << hex << encrypted << " ";
        buffer.clear();
    }
public:
    RSAEncryptStreamBuf(RSAEncryptionKey k, ostream * os, bool autoDelete)
        : os(*os), autoDelete(autoDelete), k(k)
    {
        bufferLen = log2(k.getMaxInput()) / 8;
        assert(bufferLen > paddingLength);
        bufferLen -= paddingLength;
        buffer.reserve(bufferLen);
    }
    virtual ~RSAEncryptStreamBuf()
    {
        encryptBuffer();
        if(autoDelete)
            delete &os;
    }
protected:
    virtual int overflow(int ch = char_traits<char>::eof())
    {
        if(ch == char_traits<char>::eof())
        {
            return sync();
        }
        buffer += (char)ch;
        if(buffer.length() >= bufferLen)
        {
            encryptBuffer();
            return os ? 0 : -1;
        }
        return 0;
    }
    virtual int sync()
    {
        encryptBuffer();
        os.flush();
        return os ? 0 : -1;
    }
};

class RSADecryptStreamBuf : public streambuf
{
private:
    istream & is;
    bool autoDelete;
    RSADecryptionKey k;
    string buffer;
    char * buffer2;
    bool good;
    bool decryptBuffer()
    {
        if(!good)
            return false;
        BigInteger v;
        is >> v;
        if(!is)
            return false;
        if(v.sign() < 0 || v >= k.getMaxInput())
            throw new runtime_error("v is out of range");
        v = k.decrypt(v);
        v >>= paddingLength * 8;
        buffer = v.convertToASCII();
        return buffer.length() > 0;
    }
public:
    RSADecryptStreamBuf(RSADecryptionKey k, istream * is, bool autoDelete)
        : is(*is), autoDelete(autoDelete), k(k), buffer2(NULL), good(true)
    {
        size_t bufferLen = log2(k.getMaxInput()) / 8;
        assert(bufferLen > paddingLength);
    }
    virtual ~RSADecryptStreamBuf()
    {
        if(autoDelete)
            delete &is;
        delete []buffer2;
    }
    virtual int underflow()
    {
        if(!decryptBuffer())
        {
            good = false;
            return char_traits<char>::eof();
        }
        delete []buffer2;
        buffer2 = new char[buffer.length()];
        for(size_t i = 0; i < buffer.length(); i++)
            buffer2[i] = buffer[i];
        setg(buffer2, buffer2, buffer2 + buffer.length());
        return buffer2[0];
    }
};

class RSAEncryptOStream : public ostream
{
private:
    RSAEncryptStreamBuf * sb;
public:
    RSAEncryptOStream(RSAEncryptionKey k, ostream * wrappedStream, bool autoDelete)
        : ostream(NULL), sb(new RSAEncryptStreamBuf(k, wrappedStream, autoDelete))
    {
        rdbuf(sb);
    }
    virtual ~RSAEncryptOStream()
    {
        delete sb;
    }
};

class RSADecryptIStream : public istream
{
private:
    RSADecryptStreamBuf * sb;
public:
    RSADecryptIStream(RSADecryptionKey k, istream * wrappedStream, bool autoDelete)
        : istream(NULL), sb(new RSADecryptStreamBuf(k, wrappedStream, autoDelete))
    {
        rdbuf(sb);
    }
    virtual ~RSADecryptIStream()
    {
        delete sb;
    }
};

ostream * RSAEncryptionKey::encrypt(ostream & os, bool autoDelete) const
{
    return new RSAEncryptOStream(*this, &os, autoDelete);
}

istream * RSADecryptionKey::decrypt(istream & is, bool autoDelete) const
{
    return new RSADecryptIStream(*this, &is, autoDelete);
}

class RSAKeyPair
{
private:
    BigInteger e, d, n;
public:
    RSAKeyPair(size_t bitCount)
    {
        bitCount >>= 1;
        if(bitCount < 128)
            throw new range_error("bitCount out of range");
        BigInteger u, v;
        u = BigInteger::makeProbablePrime(bitCount);
        v = BigInteger::makeProbablePrime(bitCount);
        n = u * v;
        BigInteger phi = (u - BigInteger(1)) * (v - BigInteger(1));
        e = BigInteger(65537);
        d = e.modularInverse(phi);
    }
    const RSAEncryptionKey getEncryptionKey() const
    {
        return RSAEncryptionKey(e, n);
    }
    const RSADecryptionKey getDecryptionKey() const
    {
        return RSADecryptionKey(d, n);
    }
};

void testAdd()
{
    cout << "testing fraction addition:\na:";
    Fraction a, b;
    cin >> a;
    cout << "b:";
    cin >> b;
    cout << "\n" << a << " + " << b << " = " << (a + b) << endl;
}

void testSub()
{
    cout << "testing fraction subtraction:\na:";
    Fraction a, b;
    cin >> a;
    cout << "b:";
    cin >> b;
    cout << "\n" << a << " - " << b << " = " << (a - b) << endl;
}

void testMul()
{
    cout << "testing fraction multiplication:\na:";
    Fraction a, b;
    cin >> a;
    cout << "b:";
    cin >> b;
    cout << "\n" << a << " * " << b << " = " << (a * b) << endl;
}

void testDiv()
{
    cout << "testing fraction division:\na:";
    Fraction a, b;
    cin >> a;
    cout << "b:";
    cin >> b;
    cout << "\n" << a << " / " << b << " = " << (a / b) << endl;
}

void testMod()
{
    cout << "testing fraction modulus:\na:";
    Fraction a, b;
    cin >> a;
    cout << "b:";
    cin >> b;
    cout << "\n" << a << " % " << b << " = " << (a % b) << endl;
}

void testPow()
{
    cout << "testing fracion powers:\na:";
    Fraction a;
    BigInteger b;
    cin >> a;
    cout << "b:";
    cin >> b;
    cout << "\n" << a << " ** " << b << " = " << pow(a, b) << endl;
}

void testDecimal()
{
    cout << "testing decimal output:\na:";
    Fraction a;
    cin >> a;
    cout << "\n" << a << " = " << a.getDecimal(50) << endl;
}

void testRSA()
{
    const size_t bitLimit = 128;
    cout << "WARNING: This does NOT use a secure padding method.  This program should NOT be used to\nsecurely encrypt anything.\n\ntesting RSA:\n";
    cout << "Enter the number of bits to use in the primes (>= " << bitLimit << "): ";
    int bits;
    cin >> bits;
    if(bits < static_cast<int>(bitLimit))
    {
        cout << "error : the entered number of bits is < " << bitLimit << endl;
        return;
    }
    BigInteger u, v;
    u = BigInteger::makeProbablePrime(bits);
    cout << "u = " << u << endl;
    v = BigInteger::makeProbablePrime(bits);
    cout << "v = " << v << endl;
    BigInteger n = u * v;
    BigInteger phi = (u - BigInteger(1)) * (v - BigInteger(1));
    cout << "n = " << n << endl;
    cout << "phi = " << phi << endl;
    BigInteger e = BigInteger(65537);
    cout << "e = " << e << endl;
    BigInteger d;
    try
    {
        d = e.modularInverse(phi);
    }
    catch(domain_error err)
    {
        cout << "error : " << err.what() << endl;
        return;
    }
    cout << "d = " << d << endl;
    cout << "enter text to encrypt:";
    string plaintext, decryptedText;
    cin.ignore(10000, '\n');
    getline(cin, plaintext);
    const size_t paddingLength = 16;
    size_t blockLength = log2(n) / 8 - paddingLength;
    cout << "\nencrypted = ";
    for(size_t i = 0; i < plaintext.length(); i += blockLength)
    {
        BigInteger v = BigInteger::convertFromASCII(plaintext.substr(i, blockLength));
        v <<= paddingLength * 8;
        v |= BigInteger::random(paddingLength * 8, true);
        BigInteger encrypted = modPow(v, e, n);
        cout << encrypted << endl;
        BigInteger decrypted = modPow(encrypted, d, n);
        decrypted >>= paddingLength * 8;
        decryptedText += decrypted.convertToASCII();
    }
    cout << "\ndecrypted = '" << decryptedText << "'\n";
}

void testISqrt()
{
    cout << "testing isqrt:\na:";
    BigInteger a;
    cin >> a;
    cout << isqrt(a) << endl;
}

void testSqrt()
{
    cout << "testing sqrt:\na:";
    Fraction a;
    cin >> a;
    cout << "digit count:";
    int digitCount;
    cin >> digitCount;
    if(digitCount <= 0)
    {
        cout << "error: the digit count must be more than 0\n";
        return;
    }
    cout << "sqrt(" << a << ") = " << sqrt(a, pow(BigInteger(10), digitCount)).getDecimal(digitCount) << endl;
}

void quitProgram()
{
    exit(0);
}

struct MenuEntry
{
    void (*fn)();
    const char * name;
    MenuEntry(void (*fn)(), const char * name)
    {
        this->fn = fn;
        this->name = name;
    }
};

class QuitException : public exception
{
};

void quitSubmenu()
{
    throw new QuitException;
}

void testKeyGen()
{
    cout << "testing key generation\nnumber of bits:";
    int bits;
    cin >> bits;
    if(bits <= 0)
    {
        cout << "error : number of bits must be more than zero" << endl;
        return;
    }
    try
    {
        RSAKeyPair kp(bits);
        cout << "public key : " << kp.getEncryptionKey() << endl;
        cout << "private key : " << kp.getDecryptionKey() << endl;
    }
    catch(exception * e)
    {
        cout << "error : " << e->what() << endl;
    }
}

void testEncrypt()
{
    cout << "testing encryption\npublic key:";
    try
    {
        RSAEncryptionKey k(cin);
        cout << "enter text to encrypt:";
        string str;
        cin.ignore(10000, '\n');
        getline(cin, str);
        ostream * os = k.encrypt(cout);
        *os << str;
        delete os;
        cout << endl;
    }
    catch(exception * e)
    {
        cout << "error : " << e->what() << endl;
    }
}

void testDecrypt()
{
    cout << "testing decryption\nprivate key:";
    try
    {
        RSADecryptionKey k(cin);
        cout << "enter numbers to decrypt all on one line:";
        string str;
        cin.ignore(10000, '\n');
        getline(cin, str);
        istream * is = k.decrypt(new istringstream(str));
        while(is->peek() != char_traits<char>::eof())
        {
            cout << (char)is->get();
        }
        delete is;
        cout << endl;
    }
    catch(exception * e)
    {
        cout << "error : " << e->what() << endl;
    }
}

void testRSAStreams()
{
    MenuEntry menuItems[] =
    {
        MenuEntry(testKeyGen, "test key generation"),
        MenuEntry(testEncrypt, "test encryption"),
        MenuEntry(testDecrypt, "test decryption"),
        MenuEntry(quitSubmenu, "close menu")
    };
    try
    {
        while(true)
        {
            cout << dec << "Select a menu entry :\n";
            for(size_t i = 0; i < sizeof(menuItems) / sizeof(menuItems[0]); i++)
            {
                cout << (i + 1) << ". " << menuItems[i].name << endl;
            }
            int item;
            cin >> item;
            cin.ignore(10000, '\n');
            if(!cin)
                return;
            item--;
            if(item < 0 || item >= static_cast<int>(sizeof(menuItems) / sizeof(menuItems[0])))
            {
                cout << "Invalid selection.\n";
                continue;
            }
            menuItems[item].fn();
        }
    }
    catch(QuitException * e)
    {
        delete e;
        return;
    }
}

MenuEntry menuItems[] =
{
    MenuEntry(testAdd, "test addition"),
    MenuEntry(testSub, "test subtraction"),
    MenuEntry(testMul, "test multiplication"),
    MenuEntry(testDiv, "test division"),
    MenuEntry(testPow, "test powers"),
    MenuEntry(testDecimal, "test decimal output"),
    MenuEntry(testRSA, "test RSA"),
    MenuEntry(testRSAStreams, "test RSA streams"),
    MenuEntry(testISqrt, "test isqrt"),
    MenuEntry(testSqrt, "test sqrt"),
    MenuEntry(quitProgram, "quit")
};

int main(int argc, char ** argv)
{
    if(argc >= 2 && string(argv[1]) == "rsa")
    {
        try
        {
            if(argc >= 3 && string(argv[2]) == "generate")
            {
                ptrdiff_t bits = 1024;
                if(argc > 3 && string(argv[3]).length() > 0)
                {
                    istringstream is;
                    is.str(argv[3]);
                    is >> bits;
                    if(!is)
                        throw new runtime_error(string("can't parse bit length : '") + argv[3] + "'");
                    if(bits < 0)
                        throw new range_error("bitCount out of range");
                }
                RSAKeyPair kp(bits);
                cout << kp.getEncryptionKey() << endl;
                cerr << kp.getDecryptionKey() << endl;
            }
            else if(argc >= 3 && string(argv[2]) == "encrypt")
            {
                string str = "";
                if(argc > 3)
                    str += string(argv[3]) + " ";
                else
                {
                    cout << "missing key\n";
                    return 1;
                }

                if(argc > 4)
                    str += string(argv[4]) + " ";
                istringstream is;
                is.str(str);
                RSAEncryptionKey k(is);
                ostream * os = k.encrypt(cout);
                while(cin.peek() != char_traits<char>::eof() && cin)
                {
                    *os << (char)cin.get();
                }
                delete os;
            }
            else if(argc >= 3 && string(argv[2]) == "decrypt")
            {
                string str = "";
                if(argc > 3)
                    str += string(argv[3]) + " ";
                else
                {
                    cout << "missing key\n";
                    return 1;
                }
                if(argc > 4)
                    str += string(argv[4]) + " ";
                istringstream keyIS;
                keyIS.str(str);
                RSADecryptionKey k(keyIS);
                istream * is = k.decrypt(cin);
                while(is->peek() != char_traits<char>::eof() && *is)
                {
                    cout << (char)is->get();
                }
                delete is;
            }
            else if(argc < 3 || string(argv[2]).length() == 0)
            {
                cout << "missing argument -- can use 'generate', 'encrypt', or 'decrypt'\n";
                return 1;
            }
            else
            {
                cout << "illegal argument : " << argv[2] << endl;
                return 1;
            }
        }
        catch(exception * e)
        {
            cerr << "error : " << e->what() << endl;
            return 1;
        }
        return 0;
    }
#if 0
    cout << sqrt(Fraction(2), pow(BigInteger(10), 20)).getDecimal(20) << endl;
    return 0;
#else
    while(true)
    {
        cout << dec << "Select a menu entry :\n";
        for(size_t i = 0; i < sizeof(menuItems) / sizeof(menuItems[0]); i++)
        {
            cout << (i + 1) << ". " << menuItems[i].name << endl;
        }
        int item;
        cin >> item;
        cin.ignore(10000, '\n');
        if(!cin)
            return 0;
        item--;
        if(item < 0 || item >= static_cast<int>(sizeof(menuItems) / sizeof(menuItems[0])))
        {
            cout << "Invalid selection.\n";
            continue;
        }
        menuItems[item].fn();
    }
#endif
}
