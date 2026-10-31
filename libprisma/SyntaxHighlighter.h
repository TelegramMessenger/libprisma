#pragma once
#include <sstream>

#include "TokenList.h"
#include <vector>
#include <map>
#include <optional>

class LanguageTree;
struct Grammar;

// A language and the other names it goes by. An alias with a display name of its own (html,
// under markup) is a language of its own here, as it is in the list a user picks from.
struct LanguageGroup
{
    std::string id;
    std::string name;
    std::vector<std::string> aliases;
};

struct RematchOptions
{
    std::string token;
    size_t reach;
    int j;
};

class SyntaxHighlighter
{
public:
    SyntaxHighlighter(const std::string& languages);

    // Safe to call from several threads at once.
    TokenList tokenize(const std::string& text, const std::string& language) const;

    std::map<std::string, std::string> languages() const;

    std::string languageName(const std::string& language) const;

    std::vector<LanguageGroup> languageGroups() const;

private:
    // An embedded-language token, where it is in the text and where its code starts once it is
    // joined with the others of its run.
    struct EmbeddedPart
    {
        TokenListPtr node;
        size_t offset;
        size_t start;
    };

    // What one call to the public tokenize keeps while it runs. It lives on that call's stack,
    // never in the highlighter, which threads share.
    struct State
    {
        int depth = 0;
    };

    TokenList tokenize(State& state, std::string_view text, const Grammar* grammar) const;
    void highlightEmbedded(State& state, TokenList& tokenList, size_t pos, std::string_view text, const Grammar* fixed, const Grammar*& named) const;
    void highlightParts(State& state, std::vector<EmbeddedPart>& parts, std::string& content, std::string_view text, const Grammar* language) const;
    void matchGrammar(State& state, std::string_view text, TokenList& tokenList, const Grammar* grammar, TokenListPtr startNode, size_t startPos, RematchOptions* rematch) const;

    std::shared_ptr<LanguageTree> m_tree;
};