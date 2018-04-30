/**
 * Debug logging utility.
 * 
 * Copyright (c) 2018 Sparkbit Co., Ltd. All rights reserved.
 * 
 * This work is licensed under the terms of the MIT license.  
 * See LICENSE file in the project root for details.
 */

#include "debug.h"

static const char HEXMAP[] = "0123456789ABCDEF";

Sparkbit::Debug::Debug() {
    _noTagOnce = false;
    _tagOff = false;
}

Sparkbit::Debug::Debug(const char *tag) {
    Debug();
    setTag(tag);
}

void Sparkbit::Debug::setTag(const char *tag) {
    size_t tagLen = strlen(tag);

    if (tagLen > DEBUG_TAG_STR_LEN) {
        return;
    }

    sprintf(_tagBuf, "[%s] ", tag);
    _tag = _tagBuf;
}

void Sparkbit::Debug::clearTag() {
    memset(_tagBuf, 0, sizeof(_tagBuf));
    _tag = NULL;
}

Sparkbit::Debug& Sparkbit::Debug::noTagOnce() {
    _noTagOnce = true;
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::tagOff() {
    _tagOff = true;
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::tagOn() {
    _tagOff = false;
    return *this;
}

void Sparkbit::Debug::_printTag() {
    if (_noTagOnce) {
        _noTagOnce = false;
        return;
    }

    if (!_tag || _tagOff) {
        return;
    }

    DEBUG_STREAM->print(_tag);
}

// write
Sparkbit::Debug& Sparkbit::Debug::write(uint8_t b) {
    _printTag();
    DEBUG_STREAM->write(b);
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::write(const char *str) {
    _printTag();
    DEBUG_STREAM->write(str);
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::write(const uint8_t *buffer, size_t size) {
    _printTag();
    DEBUG_STREAM->write(buffer, size);
    return *this;
}

// print
Sparkbit::Debug& Sparkbit::Debug::print(const __FlashStringHelper *ifsh) {
    _printTag();
    DEBUG_STREAM->print(ifsh);
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::print(const String &s) {
    _printTag();
    DEBUG_STREAM->print(s);
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::print(const char s[]) {
    _printTag();
    DEBUG_STREAM->print(s);
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::print(char c) {
    _printTag();
    DEBUG_STREAM->print(c);
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::print(unsigned char b, int base) {
    _printTag();
    DEBUG_STREAM->print(b, base);
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::print(int n, int base) {
    _printTag();
    DEBUG_STREAM->print(n, base);
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::print(unsigned int n, int base) {
    _printTag();
    DEBUG_STREAM->print(n, base);
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::print(long n, int base) {
    _printTag();
    DEBUG_STREAM->print(n, base);
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::print(unsigned long n, int base) {
    _printTag();
    DEBUG_STREAM->print(n, base);
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::print(double n, int digits) {
    _printTag();
    DEBUG_STREAM->print(n, digits);
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::print(const Printable &x) {
    _printTag();
    DEBUG_STREAM->print(x);
    return *this;
}


// println
Sparkbit::Debug& Sparkbit::Debug::println(const __FlashStringHelper *ifsh) {
    _printTag();
    DEBUG_STREAM->println(ifsh);
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::println(const String &s) {
    _printTag();
    DEBUG_STREAM->println(s);
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::println(const char s[]) {
    _printTag();
    DEBUG_STREAM->println(s);
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::println(char c) {
    _printTag();
    DEBUG_STREAM->println(c);
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::println(unsigned char b, int base) {
    _printTag();
    DEBUG_STREAM->println(b, base);
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::println(int n, int base) {
    _printTag();
    DEBUG_STREAM->println(n, base);
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::println(unsigned int n, int base) {
    _printTag();
    DEBUG_STREAM->println(n, base);
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::println(long n, int base) {
    _printTag();
    DEBUG_STREAM->println(n, base);
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::println(unsigned long n, int base) {
    _printTag();
    DEBUG_STREAM->println(n, base);
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::println(double n, int digits) {
    _printTag();
    DEBUG_STREAM->println(n, digits);
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::println(const Printable &x) {
    _printTag();
    DEBUG_STREAM->println(x);
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::println(void) {
    _printTag();
    DEBUG_STREAM->println();
    return *this;
}

// hex dump
Sparkbit::Debug& Sparkbit::Debug::hexByte(uint8_t b,  bool zeroXPrefix, bool trailingNewline) {
    char buf[5];

    _printTag();

    if (zeroXPrefix) {
        DEBUG_STREAM->write(buf, sprintf(buf, "0x%02X", b));
    }
    else {
        DEBUG_STREAM->write(buf, sprintf(buf, "%02X", b));
    }

    if (trailingNewline) {
        DEBUG_STREAM->println();
    }
    
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::hexShort(uint16_t val,  bool zeroXPrefix, bool trailingNewline) {
    char buf[7];

    _printTag();

    if (zeroXPrefix) {
        DEBUG_STREAM->write(buf, sprintf(buf, "0x%04X", val));
    }
    else {
        DEBUG_STREAM->write(buf, sprintf(buf, "%04X", val));
    }

    if (trailingNewline) {
        DEBUG_STREAM->println();
    }
    
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::hexInt(uint32_t val,  bool zeroXPrefix, bool trailingNewline) {
    char buf[11];

    _printTag();

    if (zeroXPrefix) {
        DEBUG_STREAM->write(buf, sprintf(buf, "0x%08lX", (unsigned long int)val));
    }
    else {
        DEBUG_STREAM->write(buf, sprintf(buf, "%08lX", (unsigned long int)val));
    }

    if (trailingNewline) {
        DEBUG_STREAM->println();
    }
    
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::hexLong(uint64_t val,  bool zeroXPrefix, bool trailingNewline) {
    char buf[17];

    _printTag();

    if (zeroXPrefix) {
        DEBUG_STREAM->write(buf, sprintf(buf, "0x%016llX", (unsigned long long int)val));
    }
    else {
        DEBUG_STREAM->write(buf, sprintf(buf, "%016llX", (unsigned long long int)val));
    }

    if (trailingNewline) {
        DEBUG_STREAM->println();
    }
    
    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::hexString(const uint8_t *buffer, size_t size, bool zeroXPrefix, bool trailingNewline) {
    size_t i;

    _printTag();

    if (zeroXPrefix) {
        DEBUG_STREAM->print("0x");
    }
    
    for (i = 0; i < size; i++) {
        DEBUG_STREAM->write(HEXMAP[(buffer[i] & 0xF0) >> 4]);
        DEBUG_STREAM->write(HEXMAP[buffer[i] & 0x0F]);
    }

    if (trailingNewline) {
        DEBUG_STREAM->println();
    }

    return *this;
}

Sparkbit::Debug& Sparkbit::Debug::hexDump(const uint8_t *buffer, size_t size, size_t lineLen, bool trailingNewline) {
    size_t i;

    if (lineLen == 0) {
        _printTag();
    }

    for (i = 0; i < size; i++) {
        if (i > 0 && lineLen > 0 && (i % lineLen) == 0) {
            DEBUG_STREAM->println();
        }
        
        DEBUG_STREAM->write(HEXMAP[(buffer[i] & 0xF0) >> 4]);
        DEBUG_STREAM->write(HEXMAP[buffer[i] & 0x0F]);
        DEBUG_STREAM->write(' ');
    }

    if (trailingNewline) {
        DEBUG_STREAM->println();
    }

    return *this;
}
