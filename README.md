This is a C++ porting of [prism.js](https://github.com/PrismJS/prism) library.
The code depends on Boost.Regex, as it's a faster and more comprehensive than STD's.

Grammars file is generated from prism.js source code itself, instructions later in the file.

### Key concepts:
```cpp
string text = ReadFile("grammars.dat");
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

### How to update
Run 
```sh
npm start
```
The new version of `grammars.dat` will appear in the `libprisma/grammars.dat`.
