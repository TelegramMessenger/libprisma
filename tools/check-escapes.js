// Every escape in grammars.dat has to mean the same thing to every engine that reads it.
// A pattern that fails to compile is caught by the probes beside this script; this one is for
// the escapes that compile everywhere and quietly mean something else, which is the worse
// failure - \v was read as U+000B by Boost and as the whole vertical whitespace class, newline
// included, by java.util.regex, which made ini values run over the end of the line.
const fs = require('fs')

// what generate.js is expected to emit, once normalizeEscapes has run over it
const ALLOWED = new Set('BSWbdfnrstuwx123456789'.split(''))

const REASONS = {
    v: 'the vertical whitespace class in java.util.regex, U+000B elsewhere - emit \\x0B',
    h: 'the horizontal whitespace class in java.util.regex, a literal h elsewhere',
    H: 'non-horizontal-whitespace in java.util.regex, a literal H elsewhere',
    V: 'non-vertical-whitespace in java.util.regex, a literal V elsewhere',
    R: 'any line break in java.util.regex, a literal R elsewhere',
    X: 'a grapheme cluster in java.util.regex, a literal X elsewhere',
    A: 'start of input in java.util.regex, a literal A elsewhere',
    Z: 'end of input in java.util.regex, a literal Z elsewhere',
    z: 'end of input in java.util.regex, a literal z elsewhere',
    G: 'end of the previous match in java.util.regex, a literal G elsewhere',
    Q: 'starts a quoted run in java.util.regex, a literal Q elsewhere',
    E: 'ends one in java.util.regex, a literal E elsewhere',
    p: 'a unicode property in both, but the property names differ',
    N: 'a named character in java.util.regex',
    0: 'starts an octal escape in java.util.regex - emit \\x00',
}

function readPatterns(path) {
    const buffer = fs.readFileSync(path)
    let offset = 0

    const uint8 = () => buffer[offset++]
    const uint16 = () => { const value = buffer.readUInt16LE(offset); offset += 2; return value }
    const string = () => {
        let length = uint8()
        if (length >= 254) {
            length = uint8() | (uint8() << 8) | (uint8() << 16)
        }
        const value = buffer.toString('latin1', offset, offset + length)
        offset += length
        return value
    }

    const patterns = []
    for (let count = uint16(); count > 0; count--) {
        patterns.push(string())
    }
    return patterns
}

const patterns = readPatterns(process.argv[2] || 'libprisma/grammars.dat')
const problems = []

for (const item of patterns) {
    const end = item.lastIndexOf('/')
    const source = item.slice(item.indexOf('/') + 1, end < 0 ? undefined : end)

    for (let i = 0; i < source.length; i++) {
        if (source[i] !== '\\') {
            continue
        }

        const escaped = source[++i]
        if (/[a-zA-Z0-9]/.test(escaped) && !ALLOWED.has(escaped)) {
            problems.push([source, '\\' + escaped, REASONS[escaped] || 'not in the allowed set'])
        }
    }
}

console.log(`${patterns.length} patterns checked`)

if (problems.length === 0) {
    process.exit(0)
}

const seen = new Set()
for (const [source, escape, reason] of problems) {
    if (seen.has(escape)) {
        continue
    }
    seen.add(escape)
    console.error(`\n${escape} is ${reason}`)
    console.error(`  e.g. ${source.slice(0, 120)}`)
}
console.error(`\n${problems.length} occurrences. Teach normalizeEscapes to rewrite them, or add the escape to ALLOWED once every engine is known to agree on it.`)
process.exit(1)
