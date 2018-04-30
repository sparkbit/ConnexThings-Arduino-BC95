/**
 * Debug logging utility.
 * 
 * Copyright (c) 2018 Sparkbit Co., Ltd. All rights reserved.
 * 
 * This work is licensed under the terms of the MIT license.  
 * See LICENSE file in the project root for details.
 */

#ifndef SPARKBIT_DEBUG_H
#define SPARKBIT_DEBUG_H

#include <Arduino.h>

#define DEBUG_TAG_STR_LEN  32
#define DEBUG_TAG_BUF_LEN  (DEBUG_TAG_STR_LEN + 4)  // brackets(2) + space(1) + null-terminated 

#define DEBUG_STREAM  ((Stream *)&Serial)

namespace Sparkbit {

class Debug {
    public:
        Debug();
        Debug(const char *tag);

        void setTag(const char *tag);
        void clearTag();

        Debug& noTagOnce();
        Debug& tagOff();
        Debug& tagOn();

        Debug& write(uint8_t b);
        Debug& write(const char *str);
        Debug& write(const uint8_t *buffer, size_t size);

        Debug& print(const __FlashStringHelper *ifsh);
        Debug& print(const String &s);
        Debug& print(const char s[]);
        Debug& print(char c);
        Debug& print(unsigned char b, int base = DEC);
        Debug& print(int n, int base = DEC);
        Debug& print(unsigned int n, int base = DEC);
        Debug& print(long n, int base = DEC);
        Debug& print(unsigned long n, int base = DEC);
        Debug& print(double n, int digits = 2);
        Debug& print(const Printable &x);

        Debug& println(const __FlashStringHelper *ifsh);
        Debug& println(const String &s);
        Debug& println(const char s[]);
        Debug& println(char c);
        Debug& println(unsigned char b, int base = DEC);
        Debug& println(int n, int base = DEC);
        Debug& println(unsigned int n, int base = DEC);
        Debug& println(long n, int base = DEC);
        Debug& println(unsigned long n, int base = DEC);
        Debug& println(double n, int digits = 2);
        Debug& println(const Printable &x);
        Debug& println(void);

        Debug& hexByte(uint8_t b, bool zeroXPrefix = false, bool trailingNewline = true);
        Debug& hexShort(uint16_t val, bool zeroXPrefix = false, bool trailingNewline = true);
        Debug& hexInt(uint32_t val, bool zeroXPrefix = false, bool trailingNewline = true);
        Debug& hexLong(uint64_t val, bool zeroXPrefix = false, bool trailingNewline = true);
        Debug& hexString(const uint8_t *buffer, size_t size, bool zeroXPrefix = false, bool trailingNewline = true);
        Debug& hexDump(const uint8_t *buffer, size_t size, size_t lineLen = 0, bool trailingNewline = true);

    private:
        static Stream *_stream;

        char *_tag;
        char _tagBuf[DEBUG_TAG_BUF_LEN];
        bool _noTagOnce;
        bool _tagOff;

        void _printTag();
};

}  /* namespace Sparkbit */

#endif  /* SPARKBIT_DEBUG_H */