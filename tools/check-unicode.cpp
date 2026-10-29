#include "SyntaxHighlighter.h"
#include "UnicodeEscapes.h"

#include <boost/regex.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

struct Token
{
    std::string type;
    std::string value;

    bool operator==(const Token&) const = default;
};

struct Inspection
{
    std::string text;
    std::vector<Token> tokens;
    size_t depth = 0;
};

void Inspect(
    const TokenList& list,
    Inspection& result,
    std::string_view type = {},
    size_t depth = 0)
{
    result.depth = std::max(result.depth, depth);
    for (const auto& node : list)
    {
        if (node.isSyntax())
        {
            const auto& syntax = static_cast<const Syntax&>(node);
            Inspect(syntax.children(), result, syntax.type(), depth + 1);
        }
        else
        {
            const auto text = static_cast<const Text&>(node).value();
            result.text.append(text);
            if (!type.empty())
            {
                result.tokens.push_back({ std::string(type), std::string(text) });
            }
        }
    }
}

bool CheckTokens(
    SyntaxHighlighter& highlighter,
    const std::string& language,
    const std::string& text,
    const std::vector<Token>& expected)
{
    const auto tokens = highlighter.tokenize(text, language);
    auto result = Inspection();
    Inspect(tokens, result);
    if (result.text == text && result.tokens == expected)
    {
        return true;
    }
    std::cerr << "Unexpected tokens for " << language << ": " << text << '\n';
    for (const auto& token : result.tokens)
    {
        std::cerr << "  " << token.type << ": " << token.value << '\n';
    }
    return false;
}

// CRLF line ends are read as LF ones are: the same tokens, each holding the same text but for
// the carriage returns.
bool CheckLineEnds(
    SyntaxHighlighter& highlighter,
    const std::string& language,
    const std::string& text)
{
    auto crlf = std::string();
    for (const char c : text)
    {
        if (c == '\n')
        {
            crlf += '\r';
        }
        crlf += c;
    }

    auto lf = Inspection();
    Inspect(highlighter.tokenize(text, language), lf);
    auto result = Inspection();
    Inspect(highlighter.tokenize(crlf, language), result);

    auto stripped = std::vector<Token>();
    for (auto token : result.tokens)
    {
        std::erase(token.value, '\r');
        if (!token.value.empty())
        {
            stripped.push_back(token);
        }
    }

    if (result.text == crlf && stripped == lf.tokens)
    {
        return true;
    }
    std::cerr << "CRLF tokens differ from LF ones for " << language << ": " << text << '\n';
    return false;
}

void CollectAliases(const TokenList& list, const std::string& type, std::vector<Token>& result)
{
    for (const auto& node : list)
    {
        if (!node.isSyntax())
        {
            continue;
        }
        const auto& syntax = static_cast<const Syntax&>(node);
        if (syntax.type() == type && !syntax.alias().empty())
        {
            auto inspection = Inspection();
            Inspect(syntax.children(), inspection);
            result.push_back({ syntax.alias(), inspection.text });
        }
        CollectAliases(syntax.children(), type, result);
    }
}

// The aliases of the tokens of `type`, each with the text it covers.
bool CheckAliases(
    SyntaxHighlighter& highlighter,
    const std::string& language,
    const std::string& text,
    const std::string& type,
    const std::vector<Token>& expected)
{
    auto result = std::vector<Token>();
    CollectAliases(highlighter.tokenize(text, language), type, result);
    if (result == expected)
    {
        return true;
    }
    std::cerr << "Unexpected aliases of " << type << " for " << language << ": " << text << '\n';
    for (const auto& token : result)
    {
        std::cerr << "  " << token.type << ": " << token.value << '\n';
    }
    return false;
}

bool CheckPattern(
    const std::string& pattern,
    const std::string& text,
    bool expected)
{
    const auto regex = boost::regex(
        libprisma::widenUnicodeClasses(pattern),
        boost::regex_constants::ECMAScript | boost::regex_constants::no_mod_m);
    if (boost::regex_match(text, regex) == expected)
    {
        return true;
    }
    std::cerr << "Unexpected match for " << pattern << ": " << text << '\n';
    return false;
}

}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: check-unicode <grammars.dat>\n";
        return 2;
    }
    auto input = std::ifstream(argv[1], std::ios::binary);
    if (!input)
    {
        std::cerr << "Cannot read grammar table\n";
        return 2;
    }
    const auto grammars = std::string(std::istreambuf_iterator<char>(input), {});
    auto highlighter = SyntaxHighlighter(grammars);
    auto failed = 0;

    for (const auto& quoted : std::vector<std::string>{
        "\u201cone\u201d",
        "\u201ca\u2014b\u201d",
        "\u201ca\u201d\u201db\u201d",
        "\"one\"",
    })
    {
        failed += !CheckTokens(
            highlighter,
            "visual-basic",
            quoted + " + 42 + " + quoted,
            {
                { "string", quoted },
                { "operator", "+" },
                { "number", "42" },
                { "operator", "+" },
                { "string", quoted },
            });
    }
    for (const auto& quoted : std::vector<std::string>{
        "\u2018foo\u2019",
        "\u2018a\u2014b\u2019",
        "`foo'",
    })
    {
        failed += !CheckTokens(
            highlighter,
            "stata",
            quoted + " + 42 + " + quoted,
            {
                { "variable", quoted },
                { "operator", "+" },
                { "number", "42" },
                { "operator", "+" },
                { "variable", quoted },
            });
    }

    failed += !CheckPattern(R"([^\u2019]+)", "a\u2019b", false);
    failed += !CheckPattern(R"([^\u2019]+)", "a\u2014\U0001F600b", true);
    failed += !CheckPattern(R"([^"\u201c\u201d]+)", "a\u201db", false);
    failed += !CheckPattern(R"([^"\u201c\u201d]+)", "a\"b", false);
    failed += !CheckPattern(R"([^"\u201c\u201d]+)", "a\u2014b\n", true);
    failed += !CheckPattern(R"([^\u2019]+)", "", false);
    failed += !CheckPattern(R"([^\u2019]{2})", "ab", true);
    failed += !CheckPattern(R"([^\u2019]{2})", "a\u2019", false);
    failed += !CheckPattern(R"([^^\u2019]+)", "a^b", false);
    failed += !CheckPattern(R"([^^\u2019]+)", "abc", true);
    failed += !CheckPattern(R"([\u201c\u201d]+)", "\u201c\u201d", true);
    failed += !CheckPattern(R"([\uD800-\uDBFF][\uDC00-\uDFFF])", "\U0001F600", true);

    const auto text = std::string("#iFclude <a>");
    const auto tokens = highlighter.tokenize(text, "brightscript");
    auto result = Inspection();
    Inspect(tokens, result);
    if (result.text != text || result.depth > 32)
    {
        std::cerr << "BrightScript recursion guard failed\n";
        ++failed;
    }
    failed += !CheckTokens(
        highlighter,
        "brightscript",
        "print 42",
        {
            { "keyword", "print" },
            { "number", "42" },
        });

    const auto diff = std::string(
        "--- a/Foo.cs\n"
        "+++ b/Foo.cs\n"
        "@@ -1,4 +1,4 @@ class Foo\n"
        " /* one\n"
        "    two */\n"
        "-int x;\n"
        "+int y;\n");
    // A language after "diff-" that is none leaves the diff as it is on its own.
    for (const auto& language : { "diff", "diff-vue" })
    {
        failed += !CheckTokens(
            highlighter,
            language,
            diff,
            {
                { "coord", "--- a/Foo.cs" },
                { "coord", "+++ b/Foo.cs" },
                { "coord", "@@ -1,4 +1,4 @@ " },
                { "embedded-language", "class Foo" },
                { "prefix", " " },
                { "embedded-language", "/* one\n" },
                { "prefix", " " },
                { "embedded-language", "   two */\n" },
                { "prefix", "-" },
                { "line", "int x;\n" },
                { "prefix", "+" },
                { "line", "int y;\n" },
            });
    }
    failed += !CheckTokens(
        highlighter,
        "diff-cs",
        diff,
        {
            { "coord", "--- a/Foo.cs" },
            { "coord", "+++ b/Foo.cs" },
            { "coord", "@@ -1,4 +1,4 @@ " },
            { "keyword", "class" },
            { "embedded-language", " " },
            { "class-name", "Foo" },
            { "prefix", " " },
            { "comment", "/* one\n" },
            { "prefix", " " },
            { "comment", "   two */" },
            { "embedded-language", "\n" },
            { "prefix", "-" },
            { "line", "int x;\n" },
            { "prefix", "+" },
            { "line", "int y;\n" },
        });

    failed += !CheckLineEnds(highlighter, "diff-cs", diff);

    // The files are coloured as their lines.
    failed += !CheckAliases(
        highlighter,
        "diff",
        diff,
        "coord",
        {
            { "deleted", "--- a/Foo.cs" },
            { "inserted", "+++ b/Foo.cs" },
        });

    std::cout << failed << " Unicode highlighting checks failed\n";
    return failed ? 1 : 0;
}
