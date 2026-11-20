#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace libprisma
{
    // Boost matches over bytes and does not read a \uXXXX escape at all: a pattern holding one
    // simply never matches. It also reads \xHH as a byte, where JavaScript and java.util.regex
    // read the code point. Spell every escape above ASCII as the bytes of its UTF-8 encoding
    // instead, and where one sits inside a character class - whose members have to be a single
    // byte each - lift it out into an alternation. This belongs here rather than in
    // grammars.dat, which keeps the original spelling for the clients that match over UTF-16.

    inline bool isHexDigit(char c)
    {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    inline int hexValue(char c)
    {
        return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10;
    }

    // the code point of the \uXXXX escape at begin, or -1 if there is not one there
    inline int unicodeEscapeAt(std::string_view pattern, size_t begin)
    {
        if (begin + 6 > pattern.size() || pattern[begin] != '\\' || pattern[begin + 1] != 'u')
        {
            return -1;
        }

        int value = 0;
        for (size_t i = begin + 2; i < begin + 6; ++i)
        {
            if (!isHexDigit(pattern[i]))
            {
                return -1;
            }

            value = value * 16 + hexValue(pattern[i]);
        }

        return value;
    }

    // the code point of the \xHH escape at begin, or -1 if there is not one there
    inline int byteEscapeAt(std::string_view pattern, size_t begin)
    {
        if (begin + 4 > pattern.size() || pattern[begin] != '\\' || pattern[begin + 1] != 'x'
            || !isHexDigit(pattern[begin + 2]) || !isHexDigit(pattern[begin + 3]))
        {
            return -1;
        }

        return hexValue(pattern[begin + 2]) * 16 + hexValue(pattern[begin + 3]);
    }

    inline void appendByte(std::string& out, unsigned char byte)
    {
        static const char digits[] = "0123456789ABCDEF";

        out += "\\x";
        out += digits[byte >> 4];
        out += digits[byte & 0x0F];
    }

    // Writes the UTF-8 encoding of codePoint into bytes and returns the number of bytes used.
    inline size_t encodeUtf8(int codePoint, unsigned char (&bytes)[4])
    {
        if (codePoint < 0x80)
        {
            bytes[0] = static_cast<unsigned char>(codePoint);
            return 1;
        }
        if (codePoint < 0x800)
        {
            bytes[0] = static_cast<unsigned char>(0xC0 | (codePoint >> 6));
            bytes[1] = static_cast<unsigned char>(0x80 | (codePoint & 0x3F));
            return 2;
        }
        if (codePoint < 0x10000)
        {
            bytes[0] = static_cast<unsigned char>(0xE0 | (codePoint >> 12));
            bytes[1] = static_cast<unsigned char>(0x80 | ((codePoint >> 6) & 0x3F));
            bytes[2] = static_cast<unsigned char>(0x80 | (codePoint & 0x3F));
            return 3;
        }
        bytes[0] = static_cast<unsigned char>(0xF0 | (codePoint >> 18));
        bytes[1] = static_cast<unsigned char>(0x80 | ((codePoint >> 12) & 0x3F));
        bytes[2] = static_cast<unsigned char>(0x80 | ((codePoint >> 6) & 0x3F));
        bytes[3] = static_cast<unsigned char>(0x80 | (codePoint & 0x3F));
        return 4;
    }

    inline void appendUtf8(std::string& out, int codePoint)
    {
        unsigned char bytes[4];
        const size_t length = encodeUtf8(codePoint, bytes);
        for (size_t i = 0; i < length; ++i)
        {
            appendByte(out, bytes[i]);
        }
    }

    // Adds to alternatives the patterns that match the UTF-8 encoding of every code point from
    // low to high. Each pattern is a sequence of byte ranges: for example, U+0400 to U+04FF
    // becomes [\xD0-\xD3][\x80-\xBF].
    //
    // Not every range can be written as one such sequence, so it is split into pieces that
    // can: first where the length of the encoding changes (at U+0080, U+0800 and U+10000),
    // then wherever the continuation bytes would not cover their full range \x80-\xBF. This
    // is the usual approach; Rust's regex-syntax, for example, does the same in Utf8Sequences.
    inline void appendUtf8Range(std::vector<std::string>& alternatives, int low, int high)
    {
        for (const int last : { 0x7F, 0x7FF, 0xFFFF })
        {
            if (low <= last && high > last)
            {
                appendUtf8Range(alternatives, low, last);
                appendUtf8Range(alternatives, last + 1, high);
                return;
            }
        }

        for (int i = 1; i < 4; ++i)
        {
            const int tail = (1 << (6 * i)) - 1;
            if ((low & ~tail) == (high & ~tail))
            {
                continue;
            }
            if ((low & tail) != 0)
            {
                appendUtf8Range(alternatives, low, low | tail);
                appendUtf8Range(alternatives, (low | tail) + 1, high);
                return;
            }
            if ((high & tail) != tail)
            {
                appendUtf8Range(alternatives, low, (high & ~tail) - 1);
                appendUtf8Range(alternatives, high & ~tail, high);
                return;
            }
        }

        unsigned char lows[4];
        unsigned char highs[4];
        const size_t length = encodeUtf8(low, lows);
        encodeUtf8(high, highs);

        std::string sequence;
        for (size_t i = 0; i < length; ++i)
        {
            if (lows[i] == highs[i])
            {
                appendByte(sequence, lows[i]);
                continue;
            }
            sequence += '[';
            appendByte(sequence, lows[i]);
            sequence += '-';
            appendByte(sequence, highs[i]);
            sequence += ']';
        }
        alternatives.push_back(std::move(sequence));
    }

    // Same as appendUtf8Range, but skips the surrogates U+D800 to U+DFFF: they have no UTF-8
    // encoding, so no text can contain them.
    inline void appendCodePoints(std::vector<std::string>& alternatives, int low, int high)
    {
        // In JavaScript a range over every surrogate takes both halves of each pair, so it
        // takes every character above U+FFFF too.
        if (low <= 0xD800 && high >= 0xDFFF)
        {
            appendUtf8Range(alternatives, 0x10000, 0x10FFFF);
        }

        if (low < 0xD800)
        {
            appendUtf8Range(alternatives, low, high < 0xD7FF ? high : 0xD7FF);
        }
        if (high > 0xDFFF)
        {
            appendUtf8Range(alternatives, low > 0xE000 ? low : 0xE000, high);
        }
    }

    // The code point of the \u or \x escape at begin, joining a surrogate pair into the one
    // character it spells, with the characters it occupies in length; -1 if there is none.
    inline int codePointAt(std::string_view pattern, size_t begin, size_t& length)
    {
        if (const int byte = byteEscapeAt(pattern, begin); byte >= 0)
        {
            length = 4;
            return byte;
        }

        const int value = unicodeEscapeAt(pattern, begin);
        length = value < 0 ? 0 : 6;

        if (value >= 0xD800 && value <= 0xDBFF)
        {
            const int low = unicodeEscapeAt(pattern, begin + 6);
            if (low >= 0xDC00 && low <= 0xDFFF)
            {
                length = 12;
                return 0x10000 + ((value - 0xD800) << 10) + (low - 0xDC00);
            }
        }

        return value;
    }

    struct ClassMember
    {
        std::string_view text;  // as spelled in the pattern, "a", "\\n", "\\u2227", "a-z"
        bool range = false;     // "a-z" rather than "a"
        int low = -1;           // code point of the character, or of the start of the range;
        int high = -1;          // of its end; -1 for a set such as \w, or no end at all
    };

    struct CharacterClass
    {
        size_t end = 0;         // index just past the closing ]
        bool negated = false;
        bool wide = false;      // holds a code point that cannot be a single byte
        std::vector<ClassMember> members;
    };

    // The character at begin inside a class, as its code point, with the characters it occupies;
    // -1 for a set such as \w or \d, and for an escape whose meaning differs between engines.
    inline size_t classCharacterAt(std::string_view pattern, size_t at, int& codePoint)
    {
        size_t length = 0;
        codePoint = codePointAt(pattern, at, length);
        if (codePoint >= 0)
        {
            return length;
        }

        const unsigned char c = static_cast<unsigned char>(pattern[at]);
        if (c != '\\' || at + 1 >= pattern.size())
        {
            codePoint = c < 0x80 ? c : -1;
            return 1;
        }

        const unsigned char escaped = static_cast<unsigned char>(pattern[at + 1]);
        switch (escaped)
        {
        case 'n': codePoint = '\n'; break;
        case 'r': codePoint = '\r'; break;
        case 't': codePoint = '\t'; break;
        case 'f': codePoint = '\f'; break;
        case 'b': codePoint = '\b'; break;
        default:
            // \w, \d, \s and the rest are sets; an escaped punctuation mark is itself
            const bool letterOrDigit = (escaped >= '0' && escaped <= '9')
                || ((escaped | 0x20) >= 'a' && (escaped | 0x20) <= 'z');
            codePoint = letterOrDigit || escaped >= 0x80 ? -1 : escaped;
            break;
        }
        return 2;
    }

    // one class member starting at begin, or 0 if the class ends there
    inline size_t readMember(std::string_view pattern, size_t begin, ClassMember& member)
    {
        auto unit = [&](size_t at, int& codePoint) -> size_t {
            return classCharacterAt(pattern, at, codePoint);
        };

        int low = -1;
        size_t length = unit(begin, low);
        size_t at = begin + length;

        // a - b, where a trailing - is a literal rather than the start of a range
        if (at + 1 < pattern.size() && pattern[at] == '-' && pattern[at + 1] != ']')
        {
            int high = -1;
            size_t tail = unit(at + 1, high);
            member.range = true;
            member.low = low;
            member.high = high;
            member.text = pattern.substr(begin, length + 1 + tail);
            return length + 1 + tail;
        }

        member.low = low;
        member.text = pattern.substr(begin, length);
        return length;
    }

    inline bool readClass(std::string_view pattern, size_t begin, CharacterClass& result)
    {
        size_t at = begin + 1;
        if (at < pattern.size() && pattern[at] == '^')
        {
            result.negated = true;
            ++at;
        }

        while (at < pattern.size() && pattern[at] != ']')
        {
            ClassMember member;
            size_t length = readMember(pattern, at, member);
            if (length == 0)
            {
                return false;
            }

            if (member.low > 0x7F || member.high > 0x7F)
            {
                result.wide = true;
            }

            result.members.push_back(member);
            at += length;
        }

        if (at >= pattern.size())
        {
            return false;
        }

        result.end = at + 1;
        return true;
    }

    inline bool isRange(const CharacterClass& characters, int low, int high)
    {
        return characters.members.size() == 1
            && characters.members[0].low == low
            && characters.members[0].high == high;
    }

    // [\uD800-\uDBFF][\uDC00-\uDFFF] and [\uD800-\uDFFF]{2} are how a JavaScript regex spells
    // one astral character, as its two UTF-16 halves. In UTF-8 that is one four byte sequence.
    inline size_t appendSurrogatePair(std::string& out, std::string_view pattern, size_t begin)
    {
        CharacterClass lead;
        if (!readClass(pattern, begin, lead) || lead.negated)
        {
            return 0;
        }

        size_t end = 0;
        if (isRange(lead, 0xD800, 0xDBFF))
        {
            CharacterClass trail;
            if (!readClass(pattern, lead.end, trail) || trail.negated || !isRange(trail, 0xDC00, 0xDFFF))
            {
                return 0;
            }
            end = trail.end;
        }
        else if (isRange(lead, 0xD800, 0xDFFF) && pattern.substr(lead.end, 3) == "{2}")
        {
            end = lead.end + 3;
        }
        else
        {
            return 0;
        }

        out += "(?:[\\xF0-\\xF4][\\x80-\\xBF]{3})";
        return end - begin;
    }

    // the class at begin, with any member that cannot be a single byte moved out of it
    inline size_t appendClass(std::string& out, std::string_view pattern, size_t begin)
    {
        CharacterClass characters;
        if (!readClass(pattern, begin, characters) || !characters.wide)
        {
            return 0;
        }

        std::vector<std::string> alternatives;
        std::string narrow;

        for (const auto& member : characters.members)
        {
            if (!member.range)
            {
                if (member.low > 0x7F)
                {
                    appendCodePoints(alternatives, member.low, member.low);
                }
                else
                {
                    narrow += member.text;
                }
            }
            else if (member.low < 0 || member.high < 0 || member.low > member.high)
            {
                // a range this code cannot read; leave a class above ASCII to Boost as it is
                if (member.low > 0x7F || member.high > 0x7F)
                {
                    return 0;
                }
                narrow += member.text;
            }
            else if (member.high <= 0x7F)
            {
                narrow += member.text;
            }
            else
            {
                // the ASCII part of the range stays in the class, where the i flag applies to it
                int low = member.low;
                if (low <= 0x7F)
                {
                    appendByte(narrow, static_cast<unsigned char>(low));
                    narrow += '-';
                    appendByte(narrow, 0x7F);
                    low = 0x80;
                }
                appendCodePoints(alternatives, low, member.high);
            }
        }

        if (!narrow.empty())
        {
            std::string members = "[";
            if (narrow.front() == '^')
            {
                members += '\\';
            }
            members += narrow;
            members += ']';
            alternatives.insert(alternatives.begin(), std::move(members));
        }

        std::string any;
        for (size_t i = 0; i < alternatives.size(); ++i)
        {
            if (i > 0)
            {
                any += '|';
            }
            any += alternatives[i];
        }

        if (characters.negated)
        {
            // One whole UTF-8 character that the lookahead does not exclude. Each branch has a
            // fixed length, so backtracking cannot give up part of the character.
            out += "(?:";
            if (!any.empty())
            {
                out += "(?!" + any + ")";
            }
            out += "(?:[\\x00-\\x7F]|[\\xC0-\\xDF][\\x80-\\xBF]|[\\xE0-\\xEF][\\x80-\\xBF]{2}|[\\xF0-\\xF7][\\x80-\\xBF]{3}))";
        }
        else
        {
            out += any.empty() ? "(?!)" : "(?:" + any + ")";
        }

        return characters.end - begin;
    }

    inline std::string widenUnicodeClasses(std::string_view pattern)
    {
        bool wide = pattern.find("\\u") != std::string_view::npos;
        for (size_t at = pattern.find("\\x"); !wide && at != std::string_view::npos; at = pattern.find("\\x", at + 2))
        {
            wide = byteEscapeAt(pattern, at) > 0x7F;
        }
        if (!wide)
        {
            return std::string{ pattern };
        }

        std::string out;
        out.reserve(pattern.size());

        for (size_t i = 0; i < pattern.size(); )
        {
            if (pattern[i] == '\\')
            {
                size_t length = 0;
                const int codePoint = codePointAt(pattern, i, length);

                if (codePoint >= 0)
                {
                    // grouped, because a quantifier after it has to apply to the whole
                    // character and not to the last byte of its encoding
                    const bool multiByte = codePoint > 0x7F;
                    if (multiByte)
                    {
                        out += "(?:";
                    }
                    appendUtf8(out, codePoint);
                    if (multiByte)
                    {
                        out += ')';
                    }

                    i += length;
                    continue;
                }

                out += pattern.substr(i, 2);
                i += 2;
                continue;
            }

            if (pattern[i] == '[')
            {
                if (size_t consumed = appendSurrogatePair(out, pattern, i))
                {
                    i += consumed;
                    continue;
                }

                if (size_t consumed = appendClass(out, pattern, i))
                {
                    i += consumed;
                    continue;
                }
            }

            out += pattern[i];
            ++i;
        }

        return out;
    }
}
