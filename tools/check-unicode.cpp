#include "SyntaxHighlighter.h"
#include "UnicodeEscapes.h"

#include <boost/regex.hpp>

#include <algorithm>
#include <atomic>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <utility>
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

// The tokens of a "---" and "+++" pair naming the same file, "a/<stem><name>" and "b/<stem><name>",
// whose language is <name>.
std::vector<Token> Files(const std::string& stem, const std::string& name)
{
    return {
        { "coord", "--- " },
        { "embedded-language-name", "a/" + stem },
        { "embedded-language-name", name },
        { "coord", "\n" },
        { "coord", "+++ " },
        { "embedded-language-name", "b/" + stem },
        { "embedded-language-name", name },
    };
}

std::vector<Token> Concat(std::vector<Token> tokens, const std::vector<Token>& more)
{
    tokens.insert(tokens.end(), more.begin(), more.end());
    return tokens;
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

// Every language tokenized on several threads at once by one highlighter, fresh so that its
// patterns compile while they race, must come out as `reference` tokenizes it alone.
bool CheckThreads(const std::string& grammars, const SyntaxHighlighter& reference)
{
    const auto text = std::string(
        "int main() { return \"x\" + 'y' + 0x1F; } /* a */ // b\n"
        "<a href=\"c\">$d @e #f</a>\n");

    std::vector<std::string> languages;
    std::vector<std::vector<Token>> expected;
    for (const auto& [language, name] : reference.languages())
    {
        auto result = Inspection();
        Inspect(reference.tokenize(text, language), result);
        languages.push_back(language);
        expected.push_back(std::move(result.tokens));
    }

    const auto shared = SyntaxHighlighter(grammars);
    std::atomic<size_t> wrong = 0;
    std::vector<std::thread> threads;
    for (size_t thread = 0; thread < 8; ++thread)
    {
        threads.emplace_back([&, thread] {
            for (size_t k = 0; k < languages.size(); ++k)
            {
                const auto i = (k + thread * languages.size() / 8) % languages.size();
                auto result = Inspection();
                Inspect(shared.tokenize(text, languages[i]), result);
                if (result.text != text || result.tokens != expected[i])
                {
                    ++wrong;
                }
            }
        });
    }
    for (auto& thread : threads)
    {
        thread.join();
    }

    if (wrong)
    {
        std::cerr << wrong << " languages tokenized differently on several threads\n";
    }
    return !wrong;
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

    // The language the file names, or the one asked for whatever they name: as Python, "/*"
    // would be two operators rather than a comment. One asked for that is no language leaves
    // the file names to say.
    const auto hunk = std::string(
        "@@ -1,4 +1,4 @@ class Foo\n"
        " /* one\n"
        "    two */\n"
        "-int x;\n"
        "+int y;\n");
    const auto code = std::vector<Token>{
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
    };
    for (const auto& [language, extension] : std::vector<std::pair<std::string, std::string>>{
        { "diff", "cs" },
        { "diff-cs", "py" },
        { "diff-vue", "cs" },
    })
    {
        const auto patch = "--- a/Foo." + extension + "\n+++ b/Foo." + extension + "\n" + hunk;
        failed += !CheckTokens(highlighter, language, patch, Concat(Files("Foo.", extension), code));
        failed += !CheckLineEnds(highlighter, language, patch);
    }
    failed += !CheckTokens(
        highlighter,
        "diff",
        hunk,
        {
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

    // The files are coloured as their lines. Alone, "---" is a removed line, here a SQL comment,
    // and names no file.
    const auto sql = std::string(
        "--- a/q.sql\n"
        "+++ b/q.sql\n"
        "@@ -1,3 +1,2 @@\n"
        " select 1;\n"
        "--- see helper.py\n"
        " select 2;\n");
    failed += !CheckAliases(
        highlighter,
        "diff",
        sql,
        "coord",
        {
            { "deleted", "--- a/q.sql" },
            { "inserted", "+++ b/q.sql" },
            { "deleted", "--- see helper.py" },
        });
    failed += !CheckTokens(
        highlighter,
        "diff",
        sql,
        Concat(Files("q.", "sql"), {
            { "coord", "@@ -1,3 +1,2 @@" },
            { "prefix", " " },
            { "keyword", "select" },
            { "embedded-language", " " },
            { "number", "1" },
            { "punctuation", ";" },
            { "embedded-language", "\n" },
            { "coord", "--- see helper.py" },
            { "prefix", " " },
            { "keyword", "select" },
            { "embedded-language", " " },
            { "number", "2" },
            { "punctuation", ";" },
            { "embedded-language", "\n" },
        }));

    // Each file's code is in the language its name says, up to the next file: a quoted name
    // still says it, and one that says nothing ends the language before. A deleted file's
    // "+++ /dev/null" leaves its old name standing.
    const auto files = std::string(
        "--- a/x.py\n"
        "+++ b/x.py\n"
        "@@ -1 +1 @@\n"
        " x = 1\n"
        "--- \"a/\\303\\266.cs\"\n"
        "+++ \"b/\\303\\266.cs\"\n"
        "@@ -1 +1 @@\n"
        " /* c */\n"
        "--- a/foo.c~\n"
        "+++ b/foo.c~\n"
        "@@ -1 +1 @@\n"
        " /* c */\n"
        "--- a/Foo.RS\t2024-01-01 00:00:00\n"
        "+++ /dev/null\n"
        "@@ -1 +0,0 @@ fn main()\n"
        " let x = 1;\n"
        "--- a/Makefile\n"
        "+++ b/Makefile\n"
        "@@ -1 +1 @@\n"
        " all:\n");
    failed += !CheckTokens(
        highlighter,
        "diff",
        files,
        {
            { "coord", "--- " },
            { "embedded-language-name", "a/x." },
            { "embedded-language-name", "py" },
            { "coord", "\n" },
            { "coord", "+++ " },
            { "embedded-language-name", "b/x." },
            { "embedded-language-name", "py" },
            { "coord", "@@ -1 +1 @@" },
            { "prefix", " " },
            { "embedded-language", "x " },
            { "operator", "=" },
            { "embedded-language", " " },
            { "number", "1" },
            { "embedded-language", "\n" },
            { "coord", "--- " },
            { "embedded-language-name", "\"a/\\303\\266." },
            { "embedded-language-name", "cs" },
            { "embedded-language-name", "\"" },
            { "coord", "\n" },
            { "coord", "+++ " },
            { "embedded-language-name", "\"b/\\303\\266." },
            { "embedded-language-name", "cs" },
            { "embedded-language-name", "\"" },
            { "coord", "@@ -1 +1 @@" },
            { "prefix", " " },
            { "comment", "/* c */" },
            { "embedded-language", "\n" },
            { "coord", "--- " },
            { "embedded-language-name", "a/foo.c~" },
            { "coord", "\n" },
            { "coord", "+++ " },
            { "embedded-language-name", "b/foo.c~" },
            { "coord", "@@ -1 +1 @@" },
            { "prefix", " " },
            { "embedded-language", "/* c */\n" },
            { "coord", "--- " },
            { "embedded-language-name", "a/Foo." },
            { "embedded-language-name", "RS" },
            { "coord", "\t2024-01-01 00:00:00" },
            { "coord", "\n" },
            { "coord", "+++ /dev/null" },
            { "coord", "@@ -1 +0,0 @@ " },
            { "keyword", "fn" },
            { "embedded-language", " " },
            { "function-definition", "main" },
            { "punctuation", "(" },
            { "punctuation", ")" },
            { "prefix", " " },
            { "keyword", "let" },
            { "embedded-language", " x " },
            { "operator", "=" },
            { "embedded-language", " " },
            { "number", "1" },
            { "punctuation", ";" },
            { "embedded-language", "\n" },
            { "coord", "--- " },
            { "embedded-language-name", "a/" },
            { "embedded-language-name", "Makefile" },
            { "coord", "\n" },
            { "coord", "+++ " },
            { "embedded-language-name", "b/" },
            { "embedded-language-name", "Makefile" },
            { "coord", "@@ -1 +1 @@" },
            { "prefix", " " },
            { "target", "all" },
            { "punctuation", ":" },
            { "embedded-language", "\n" },
        });

    // A greedy search starting after the string is not at the start of a line, so the
    // "^[ \t]*name" form of a mel function cannot match there.
    failed += !CheckTokens(
        highlighter,
        "mel",
        "\"a\" b",
        {
            { "string", "\"a\"" },
        });

    // A file extension that is no language name reads as itself, as an unknown name does.
    for (const auto& [language, name] : std::vector<std::pair<std::string, std::string>>{
        { "rs", "rs" },
        { "rust", "Rust" },
        { "nope", "nope" },
    })
    {
        if (highlighter.languageName(language) != name)
        {
            std::cerr << "Unexpected name for " << language << ": " << highlighter.languageName(language) << '\n';
            ++failed;
        }
    }

    // Every regex match here regroups all the quotes after it, so the rematching grows with the
    // square of the text: it runs out of searches and still hands back the whole text.
    {
        auto dataweave = std::string();
        for (int i = 0; i < 2033; ++i)
        {
            dataweave += "\"/ ";
        }
        auto result = Inspection();
        Inspect(highlighter.tokenize(dataweave, "dataweave"), result);
        if (result.text != dataweave)
        {
            std::cerr << "DataWeave tokens out of searches lost text\n";
            ++failed;
        }
    }
    failed += !CheckTokens(
        highlighter,
        "dataweave",
        "var answer = 42",
        {
            { "keyword", "var" },
            { "operator", "=" },
            { "number", "42" },
        });

    failed += !CheckThreads(grammars, highlighter);

    std::cout << failed << " Unicode highlighting checks failed\n";
    return failed ? 1 : 0;
}
