#include "LanguageTree.h"

#include "TokenList.h"
#include <boost/regex.hpp>

void LanguageTree::load(const std::string& path)
{
    FILE* file;
    if (fopen_s(&file, path.c_str(), "rb") == 0)
    {
        parsePatterns(file);
        parseGrammars(file);
        parseLanguages(file);
        fclose(file);
    }
}

inline uint8_t freadUint8(FILE* file)
{
    uint8_t value;
    fread(&value, sizeof(uint8_t), 1, file);
    return value;
}

inline uint16_t freadUint16(FILE* file)
{
    uint16_t value;
    fread(&value, sizeof(uint16_t), 1, file);
    return value;
}

inline std::string freadString(FILE* file)
{
    size_t length = freadUint8(file);
    if (length >= 254)
    {
        length = freadUint8(file)
            | (freadUint8(file) << 8) 
            | (freadUint8(file) << 16);
    }

    std::string str(length, '\0');
    fread(&str[0], sizeof(char), length, file);
    return str;
}

void LanguageTree::parseLanguages(FILE* file)
{
    uint16_t count = freadUint16(file);

    for (int i = 0; i < count; ++i)
    {
        std::string name = freadString(file);
        uint16_t index = freadUint16(file);
        m_languages.emplace(name, index);
    }
}

void LanguageTree::parseGrammars(FILE* file)
{
    uint16_t count = freadUint16(file);

    for (int i = 0; i < count; ++i)
    {
        auto grammar = std::make_shared<Grammar>();
        const auto& keys = freadUint8(file);

        for (int j = 0; j < keys; ++j)
        {
            std::vector<PatternPtr> indices;

            const auto& key = freadString(file);
            uint8_t ids = freadUint8(file);

            for (int k = 0; k < ids; ++k)
            {
                indices.push_back(PatternPtr(shared_from_this(), freadUint16(file)));
            }

            grammar->tokens.push_back(GrammarToken(key, indices));
        }

        m_grammars.push_back(grammar);
    }
}

void LanguageTree::parsePatterns(FILE* file)
{
    uint16_t count = freadUint16(file);

    for (int i = 0; i < count; ++i)
    {
        std::string item = freadString(file);
        std::string_view value(item);
        std::string alias;

        size_t beg = value.find_first_of('/');
        size_t end = value.find_last_of('/');

        if (beg != std::string::npos && end != std::string::npos)
        {
            std::string_view pattern = value.substr(beg + 1, end - (beg + 1));
            std::string_view options = value.substr(end + 1);

            size_t aliasBeg = options.find_first_of(',');
            size_t aliasEnd = options.find_last_of(',');

            std::string alias{ options.substr(aliasBeg + 1, aliasEnd - (aliasBeg + 1)) };
            size_t inside = 0;

            if (aliasEnd + 1 < options.size())
            {
                for (int i = aliasEnd + 1; i < options.size(); i++)
                {
                    char c = options[i];
                    if (c >= '0' && c <= '9')
                    {
                        inside = inside * 10 + (c - '0');
                    }
                    else
                    {
                        assert(false);
                    }
                }
            }
            else
            {
                inside = std::string::npos;
            }

            bool lookbehind = false;
            bool greedy = false;

            boost::regex_constants::syntax_option_type flags
                = boost::regex_constants::ECMAScript | boost::regex_constants::no_mod_m;

            for (char c : options.substr(0, aliasBeg))
            {
                switch (c)
                {
                case 'l':
                    lookbehind = true;
                    break;
                case 'y':
                    greedy = true;
                    break;
                case 'i':
                    flags |= boost::regex_constants::icase;
                    break;
                case 'm':
                    flags &= ~boost::regex_constants::no_mod_m;
                    break;
                }
            }

            if (inside != std::string::npos)
            {
                m_patterns.push_back(std::make_shared<Pattern>(pattern, flags, lookbehind, greedy, std::string{ alias }, std::make_shared<GrammarPtr>(shared_from_this(), inside)));
            }
            else
            {
                m_patterns.push_back(std::make_shared<Pattern>(pattern, flags, lookbehind, greedy, std::string{ alias }));
            }
        }
    }
}

const Pattern* LanguageTree::resolvePattern(size_t path)
{
    return m_patterns[path].get();
}

const Grammar* LanguageTree::resolveGrammar(size_t path)
{
    return m_grammars[path].get();
}
