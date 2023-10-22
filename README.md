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
As mentioned, grammars dictionary is generated starting from prism.js source code.
Currently, this is done manually by visiting prism's [test drive](https://prismjs.com/test.html).
Once on the page, it is necessary to select all the languages, open the browser console and paste in both `isEqual.js` and `generate.js`.
After a few seconds, the file `grammars.dat` will be downloaded.

OR

Use 
```sh
node generate-node.js
```
The new version of `grammars.dat` will appear in the `libprisma/grammars.dat`.