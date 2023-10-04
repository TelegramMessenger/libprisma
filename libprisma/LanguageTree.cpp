#include "LanguageTree.h"

#include "TokenList.h"
#include <boost/regex.hpp>

void LanguageTree::parse(const std::string& text)
{
    std::string parsingError;
    auto json = json11::Json::parse(text, parsingError);

    assert(json.is_object());

    const auto& languages = json["languages"];
    const auto& grammars = json["grammars"];
    const auto& patterns = json["patterns"];

    parseLanguages(languages);
    parseGrammars(grammars);
    parsePatterns(patterns);
}

void LanguageTree::parseLanguages(const json11::Json& node)
{
    assert(node.is_object());

    for (const auto& kv : node.object_items())
    {
        m_languages.emplace(kv.first, kv.second.int_value());
    }
}

void LanguageTree::parseGrammars(const json11::Json& node)
{
    assert(node.is_array());

    for (const auto& i : node.array_items())
    {
        auto grammar = std::make_shared<Grammar>();
        const auto& items = i.object_items();

        for (const auto& key : items.keys())
        {
            std::vector<PatternPtr> indices;

            const auto& kv = i[key];
            if (kv.is_number())
            {
                indices.push_back(PatternPtr(shared_from_this(), kv.int_value()));
            }
            else
            {
                for (const auto& jv : kv.array_items())
                {
                    indices.push_back(PatternPtr(shared_from_this(), jv.int_value()));
                }
            }

            grammar->tokens.push_back(GrammarToken(key, indices));
        }

        m_grammars.push_back(grammar);
    }
}

inline std::string getString(const std::map<std::string, json11::Json>& items, std::string key, std::string defaultValue)
{
    const auto value = items.find(key);
    if (value != items.end())
    {
        return value->second.string_value();
    }

    return defaultValue;
}

inline size_t getInt(const std::map<std::string, json11::Json>& items, std::string key, size_t defaultValue)
{
    const auto value = items.find(key);
    if (value != items.end())
    {
        return value->second.int_value();
    }

    return defaultValue;
}

void LanguageTree::parsePatterns(const json11::Json& node)
{
    assert(node.is_array());

    for (const auto& item : node.array_items())
    {
        assert(item.is_string());

        std::string_view value = item.string_value();
        std::string alias;

        size_t beg = value.find_first_of('/');
        size_t end = value.find_last_of('/');

        if (beg != std::string::npos && end != std::string::npos)
        {
            std::string_view pattern = value.substr(beg + 1, end - (beg + 1));
            std::string_view options = value.substr(end + 1);

            size_t aliasBeg = options.find_first_of(',');
            size_t aliasEnd = options.find_last_of(',');

            std::string alias{options.substr(aliasBeg + 1, aliasEnd - (aliasBeg + 1))};
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
                m_patterns.push_back(std::make_shared<Pattern>(pattern, flags, lookbehind, greedy, std::string{alias}, std::make_shared<GrammarPtr>(shared_from_this(), inside)));
            }
            else
            {
                m_patterns.push_back(std::make_shared<Pattern>(pattern, flags, lookbehind, greedy, std::string{alias}));
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
