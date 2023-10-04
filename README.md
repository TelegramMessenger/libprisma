This is a C++ porting of [prism.js](https://github.com/PrismJS/prism) library.
The code depends on Boost.Regex and DropBox's json11 with a small edit to preserve object keys order.

Grammars file is generated from the prism source code itself.
Generation script will be included in the future.

Key concepts:
```cpp
string text = ReadFile("grammars.json");
m_highlighter = std::make_shared<SyntaxHighlighter>(text);
TokenList tokens = m_highlighter->tokenize(code, language);

for (auto it = tokens.begin(); it != tokens.end(); ++it)
{
    auto& node = *it;
    if (node.isSyntax())
    {
        const auto& child = dynamic_cast<const Syntax&>(node);
        // child.type() <- main token type (eg "include")
        // child.alias() <- "base" token type (eg "keyword")
        // child.begin() + node.end() <- list of tokens
    }
    else
    {
        const auto& child = dynamic_cast<const Text&>(node);
        // child.value() <- the actual text to highlight
    }
}
```
