function unique(a, fn) {
    if (a.length === 0 || a.length === 1) {
      return a;
    }
    if (!fn) {
      return a;
    }
  
    for (let i = 0; i < a.length; i++) {
      for (let j = i + 1; j < a.length; j++) {
        if (fn(a[i], a[j])) {
          a.splice(i, 1);
        }
      }
    }
    return a;
  }

function uniqlo(a, fn) {
    var size = a.length;

    do{
        size = a.length
        a = unique(a, fn)
    }
    while (size > a.length)
    return a
}

var tempPatterns = []
var tempLanguages = {}

var tempTokens = []
var tempGrammars = []

var weak = new WeakMap

function flatten(grammar) {
    var keys = {}

    var cache = weak.get(grammar)
    if (cache !== undefined) {
        return cache
    }

    weak.set(grammar, keys)

    var copy = grammar;
    var rest = copy.rest;
    if (rest) {
        copy = {}

        Object.keys(grammar).forEach(name => {
            copy[name] = grammar[name]
        })

        for (var token in rest) {
            copy[token] = rest[token];
        }

        delete copy.rest;
    }

    function sanitize(pattern) {
        // Unsupported:
        // UTF-16 ranges
        // [^]    => [\s\S] <- matches any character, including new line
        // []     =>   ??   <- matches _empty_ string

        // All the whitelisted languages have 0xFFFF as maximum range
        // This is not the case for all the grammars supported by Prisma.

        pattern = pattern.replaceAll("\\uFFFF", "\\xFF");
        pattern = pattern.replaceAll("[^]", "[\\s\\S]");

        // TODO: This just bruteforces the regex to work, but of course 
        // result may vary from the original one.
        //static const boost::regex hex(R"(\\u([0-9a-fA-F]{4}))");
        //pattern = boost::regex_replace(pattern, hex, R"(\\xFF)");

        // TODO: Again, none of the whitelisted languages use [], but others do.
        // Howhever, it is unclear to me how [] is supposed to work.
        //pattern = replace(pattern, "|[])", ")");
        //pattern = replace(pattern, ":[]", ":");

        return pattern
    }

    for (var token in copy) {
        if (!copy.hasOwnProperty(token) || !copy[token]) {
            continue;
        }

        var patterns = copy[token];
        patterns = Array.isArray(patterns) ? patterns : [patterns];

        var indexes = []

        for (var j = 0; j < patterns.length; ++j) {
            var patternObj = patterns[j];
            var inside = patternObj.inside;
            var lookbehind = !!patternObj.lookbehind;
            var greedy = !!patternObj.greedy;
            var alias = patternObj.alias;

            //alias = Array.isArray(alias) ? alias : [alias];
            //alias = alias.join('/')
            alias = Array.isArray(alias) ? alias[0] : alias;

            var pattern = patternObj.pattern || patternObj;
            var patternStr = sanitize(pattern.toString())

            if (lookbehind) {
                patternStr += "l"
            }
            if (greedy) {
                patternStr += "y"
            }

            var np

            if (alias || inside) {
                np = {
                    pattern: patternStr
                }

                if (alias) {
                    np.alias = alias
                }
                if (inside) {
                    np.inside = flatten(inside)
                }

            } else if (pattern instanceof RegExp) {
                np = patternStr
            } else {
                debugger
            }
            
            tempPatterns.push(np)
            indexes.push(np)
        }

        keys[token] = indexes
        tempTokens.push(indexes)
    }

    tempGrammars.push(keys)
    return keys
}

var whitelist = [ "markup", "css", "clike", "javascript", "sql", "c", "csharp", "cpp", "aspnet", "bash", "basic", "arduino", "brainfuck", "cmake", "cobol", "ruby", "csv", "dart", "crystal", "diff", "docker", "editorconfig", "elixir", "django", "lua", "erlang", "fsharp", "fortran", "gcode", "git", "glsl", "go", "graphql", "groovy", "haskell", "http", "java", "typescript", "json", "gradle", "javadoc", "javadoclike", "jsonp", "json5", "kotlin", "latex", "php", "makefile", "markdown", "matlab", "mongodb", "objectivec", "pascal", "perl", "powershell", "protobuf", "python", "qml", "r", "regex", "rust", "scss", "scala", "smali", "swift", "vbnet", "yaml", "uri", "visual-basic", "wasm", "vim", "batch", "bbcode", "hlsl", "less", "ini" ]

whitelist.forEach(name => {
    tempLanguages[name] = flatten(Prism.languages[name])
})

var allTokens = uniqlo(tempTokens, isEqual)
var allGrammars = uniqlo(tempGrammars, isEqual)
var allPatterns = uniqlo(tempPatterns, isEqual)

Object.keys(tempLanguages).forEach(name => {
    var find = allGrammars.find(x => isEqual(x, tempLanguages[name]))
    if (find === undefined) {
        debugger
    }

    tempLanguages[name] = find
})

for (var i = 0; i < allPatterns.length; i++) {
    if (allPatterns[i].inside) {
        var find = allGrammars.find(x => isEqual(x, allPatterns[i].inside))
        if (find === undefined) {
            debugger
        }

        allPatterns[i].inside = find
    }
}

for (var i = 0; i < allTokens.length; i++) {
    var token = allTokens[i]

    for (var j = 0; j < token.length; j++) {
        var find = allPatterns.find(x => isEqual(x, token[j]))
        if (find === undefined) {
            debugger
        }

        token[j] = find
    }
}

for (var i = 0; i < allGrammars.length; i++) {
    Object.keys(allGrammars[i]).forEach(name => {
        var find = allTokens.find(x => isEqual(x, allGrammars[i][name]))
        if (find === undefined) {
            debugger
        }

        allGrammars[i][name] = find
    })
}

for (var i = 0; i < allPatterns.length; i++) {
    if (allPatterns[i].inside) {
        allPatterns[i].inside = allGrammars.indexOf(allPatterns[i].inside)
    }
}

for (var i = 0; i < allTokens.length; i++) {
    var token = allTokens[i]

    for (var j = 0; j < token.length; j++) {
        token[j] = allPatterns.indexOf(token[j])
    }
}

for (var i = 0; i < allGrammars.length; i++) {
    Object.keys(allGrammars[i]).forEach(name => {
        if (allGrammars[i][name].length == 1) {
            //allGrammars[i][name] = allGrammars[i][name][0]
        }
    })
}

for (var i = 0; i < allPatterns.length; i++) {
    if (allPatterns[i].pattern) {
        var patternStr = allPatterns[i].pattern + ",";
        if (allPatterns[i].alias) {
            patternStr += allPatterns[i].alias
        }
            patternStr += ","
        if (allPatterns[i].inside) {
            patternStr += allPatterns[i].inside
        }

        allPatterns[i] = patternStr
    } else {
        allPatterns[i] += ",,"
    }
}

var allLanguages = {}

Object.keys(tempLanguages).forEach(name => {
    var find = allGrammars.find(x => isEqual(x, tempLanguages[name]))
    if (find === undefined) {
        debugger
    }

    allLanguages[name] = allGrammars.indexOf(find)
})

var final = {
    patterns: allPatterns,
    grammars: allGrammars,
    languages: allLanguages
}

const chunks = [];

const writeUint16 = i => chunks.push(new Uint16Array([ i ]))
const writeUint8 = i => chunks.push(new Uint8Array([ i ]))
const writeString = str => {
    if (str.length < 253) {
        writeUint8(str.length)
    } else {
        writeUint8(254 & 0xFF)
        writeUint8(str.length & 0xFF)
        writeUint8((str.length >> 8) & 0xFF)
        writeUint8((str.length >> 16) & 0xFF)
    }
    chunks.push(new Uint8Array(str.split('').map(char => char.charCodeAt(0))))
}

// Patterns
writeUint16(allPatterns.length)

allPatterns.forEach(pattern => {
    writeString(pattern)
})

// Grammars
writeUint16(allGrammars.length)

for (var i = 0; i < allGrammars.length; i++) {
    writeUint8(Object.keys(allGrammars[i]).length)
    
    Object.keys(allGrammars[i]).forEach(name => {
        writeString(name)
        writeUint8(allGrammars[i][name].length)
        allGrammars[i][name].forEach(id => {
            writeUint8(id)
        })
    })
}

// Languages
writeUint16(Object.keys(allLanguages).length)

Object.keys(allLanguages).forEach(name => {
    writeString(name)
    writeUint16(allLanguages[name])
})

const blob = new Blob(chunks, { type: 'application/octet-binary' });
const url = window.URL.createObjectURL(blob)
const a = document.createElement('a')
a.href = url
a.download = 'grammars.dat'
a.click()
