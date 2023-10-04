#pragma once
#include <sstream>

#include "TokenList.h"
#include <vector>
#include <map>
#include <optional>
#include <format>

class LanguageTree;
struct Grammar;

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

    TokenList tokenize(const std::string& text, const std::string& language);

    std::vector<std::string> languages() const;

private:
    TokenList tokenize(std::string_view text, const Grammar* grammar);
    void matchGrammar(std::string_view text, TokenList& tokenList, const Grammar* grammar, TokenListPtr startNode, size_t startPos, RematchOptions* rematch);

    std::shared_ptr<LanguageTree> m_tree;
};