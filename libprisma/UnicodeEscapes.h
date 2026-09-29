#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace libprisma
{
    // Boost matches over bytes and does not read a \uXXXX escape at all: a pattern holding one
    // simply never matches. Spell every escape as the bytes of its UTF-8 encoding instead, and
    // where one sits inside a character class - whose members have to be a single byte each -
    // lift it out into an alternation. This belongs here rather than in grammars.dat, which
    // keeps the \u spelling for the clients that match over UTF-16 and read it correctly.

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

    inline void appendByte(std::string& out, unsigned char byte)
    {
        static const char digits[] = "0123456789ABCDEF";

        out += "\\x";
        out += digits[byte >> 4];
        out += digits[byte & 0x0F];
    }

    inline void appendUtf8(std::string& out, int codePoint)
    {
        if (codePoint < 0x80)
        {
            appendByte(out, static_cast<unsigned char>(codePoint));
        }
        else if (codePoint < 0x800)
        {
            appendByte(out, static_cast<unsigned char>(0xC0 | (codePoint >> 6)));
            appendByte(out, static_cast<unsigned char>(0x80 | (codePoint & 0x3F)));
        }
        else if (codePoint < 0x10000)
        {
            appendByte(out, static_cast<unsigned char>(0xE0 | (codePoint >> 12)));
            appendByte(out, static_cast<unsigned char>(0x80 | ((codePoint >> 6) & 0x3F)));
            appendByte(out, static_cast<unsigned char>(0x80 | (codePoint & 0x3F)));
        }
        else
        {
            appendByte(out, static_cast<unsigned char>(0xF0 | (codePoint >> 18)));
            appendByte(out, static_cast<unsigned char>(0x80 | ((codePoint >> 12) & 0x3F)));
            appendByte(out, static_cast<unsigned char>(0x80 | ((codePoint >> 6) & 0x3F)));
            appendByte(out, static_cast<unsigned char>(0x80 | (codePoint & 0x3F)));
        }
    }

    // The code point at begin, joining a surrogate pair into the one character it spells, with
    // the characters it occupies in length. A lone \uXXXX is left to Boost, which expands it
    // to UTF-8 itself; a pair it would expand to two three byte sequences instead.
    inline int codePointAt(std::string_view pattern, size_t begin, size_t& length)
    {
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
        int low = -1;           // code point, for a member that is a lone \u escape
        int high = -1;          // and the other end, when the member is a range of them
    };

    struct CharacterClass
    {
        size_t end = 0;         // index just past the closing ]
        bool negated = false;
        bool wide = false;      // holds a code point that cannot be a single byte
        std::vector<ClassMember> members;
    };

    // one class member starting at begin, or 0 if the class ends there
    inline size_t readMember(std::string_view pattern, size_t begin, ClassMember& member)
    {
        auto unit = [&](size_t at, int& codePoint) -> size_t {
            size_t length = 0;
            codePoint = codePointAt(pattern, at, length);
            if (codePoint >= 0)
            {
                return length;
            }
            return pattern[at] == '\\' && at + 1 < pattern.size() ? 2 : 1;
        };

        int low = -1;
        size_t length = unit(begin, low);
        size_t at = begin + length;

        // a - b, where a trailing - is a literal rather than the start of a range
        if (at + 1 < pattern.size() && pattern[at] == '-' && pattern[at + 1] != ']')
        {
            int high = -1;
            size_t tail = unit(at + 1, high);
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

        std::vector<int> wide;
        std::string narrow;

        for (const auto& member : characters.members)
        {
            if (member.low <= 0x7F && member.high <= 0x7F)
            {
                narrow += member.text;
            }
            else if (member.high < 0)
            {
                wide.push_back(member.low);
            }
            else if (!characters.negated)
            {
                // a range of code points that is not the surrogate idiom; leave it be rather
                // than expand what could be the whole of Unicode
                return 0;
            }
        }

        // a negated class drops its wide members: they exclude characters that cannot occur
        // as bytes in valid UTF-8 anyway, and as bytes they would exclude unrelated ones
        if (characters.negated || wide.empty())
        {
            out += '[';
            if (characters.negated)
            {
                out += '^';
            }
            out += narrow;
            out += ']';
            return characters.end - begin;
        }

        out += "(?:";
        if (!narrow.empty())
        {
            out += '[';
            out += narrow;
            out += ']';
            out += '|';
        }
        for (size_t i = 0; i < wide.size(); ++i)
        {
            if (i > 0)
            {
                out += '|';
            }
            appendUtf8(out, wide[i]);
        }
        out += ')';

        return characters.end - begin;
    }

    inline std::string widenUnicodeClasses(std::string_view pattern)
    {
        if (pattern.find("\\u") == std::string_view::npos)
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
