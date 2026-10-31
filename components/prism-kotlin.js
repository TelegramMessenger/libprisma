/**
 * Kotlin, replacing Prism's grammar. Tokens are matched in the order they are declared, and a
 * lookbehind or lookahead only sees the text no earlier token has taken, so the order below is
 * what makes the context rules work: names of types are found before keywords take the
 * keyword in front of them, and modifiers before the declaration keyword after them.
 */
(function (Prism) {
	// Completes a pattern that refers to itself through <self>, nesting it depth times.
	function nested(pattern, depth) {
		for (var i = 0; i < depth; i++) {
			pattern = pattern.replace(/<self>/g, function () { return '(?:' + pattern + ')'; });
		}
		return pattern.replace(/<self>/g, '[^\\s\\S]');
	}

	function re(pattern) {
		return RegExp(pattern.replace(/<(\w+)>/g, function (m, name) { return parts[name]; }));
	}

	var parts = {};

	// Kotlin's block comments nest; two levels inside the outer one cover real code.
	parts.block = nested(/\/\*(?:[^*/]|\*(?!\/)|\/(?!\*)|<self>)*\*\//.source, 2);

	// The text of a line before a comment starts: code, and strings, characters, backticked
	// names and block comments that close on it. A "//" or "/*" inside one of those is not a
	// comment, which matching comments first would otherwise make it. Lazy, or it would take
	// in the first of two comments on a line and match the second.
	parts.code = re(/(?:[^"'`\/\r\n]|\/(?![\/*])|"(?:\\.|[^"\\\r\n])*"|'(?:\\.|[^'\\\r\n])*'|`[^`\r\n]*`|<block>)*?/.source).source;

	// a template's braces, which may hold one more level of braces, a lambda's
	parts.braces = /\{(?:[^{}]|\{[^{}]*\})*\}/.source;

	// the package and the outer classes a type may be qualified by
	parts.qualifier = /(?:[a-z]\w*\s*\.\s*)*(?:[A-Z]\w*\s*\.\s*)*/.source;

	// type arguments in a call, nested twice; a function type in them is not recognised
	parts.arguments = /<(?:[\w\s,.?*]|<(?:[\w\s,.?*]|<[\w\s,.?*]*>)*>)*>/.source;

	parts.call = re(/\s*(?:<arguments>)?\s*\(/.source).source;

	var modifiers = 'abstract|actual|annotation|companion|const|data|enum|expect|external|final|infix|inline|inner|internal|open|operator|override|private|protected|public|sealed|value';

	var type = {
		'namespace': {
			pattern: /^[a-z]\w*(?:\s*\.\s*[a-z]\w*)*(?:\s*\.)?/,
			inside: {
				'punctuation': /\./
			}
		},
		'punctuation': /\./
	};

	var interpolation = {
		'interpolation-punctuation': {
			pattern: /^\$\{?|\}$/,
			alias: 'punctuation'
		},
		'expression': {
			pattern: /[\s\S]+/
		}
	};

	var kotlin = {
		// Not greedy: each match is searched for in the text after the comment before it, so "^"
		// is a place outside any string, which a line start may not be.
		'comment': [
			{
				pattern: re(/((?:^|[\r\n])<code>)<block>/.source),
				lookbehind: true
			},
			{
				pattern: re(/((?:^|[\r\n])<code>)\/\/.*/.source),
				lookbehind: true
			}
		],
		'string-literal': [
			{
				// a raw string may end in quotes of its own: """a"""" holds a"
				pattern: re(/"""(?:[^$]|\$(?!\{)|\$<braces>)*?"""(?!")/.source),
				alias: 'multiline',
				greedy: true,
				inside: {
					'interpolation': {
						pattern: re(/\$(?:[a-zA-Z_]\w*|<braces>)/.source),
						inside: interpolation
					},
					'string': /[\s\S]+/
				}
			},
			{
				pattern: re(/"(?:[^"\\\r\n$]|\\.|\$(?!\{)|\$<braces>)*"/.source),
				alias: 'singleline',
				greedy: true,
				inside: {
					'interpolation': {
						pattern: re(/((?:^|[^\\])(?:\\{2})*)\$(?:[a-zA-Z_]\w*|<braces>)/.source),
						lookbehind: true,
						inside: interpolation
					},
					'string': /[\s\S]+/
				}
			}
		],
		'char': {
			pattern: /'(?:[^'\\\r\n]|\\(?:.|u[a-fA-F0-9]{0,4}))'/,
			greedy: true
		},
		'annotation': {
			// "@get:Inject", "@file:JvmName", "@kotlin.jvm.JvmStatic", "@field:[A B]"
			pattern: /\B@(?:\w+:)?(?:(?:[a-z]\w*\.)*[A-Z]\w*|\[[^\]]+\])/,
			alias: 'builtin'
		},
		'label': [
			{
				// "loop@ for", and not "return@forEach"
				pattern: /\b\w+@(?!\w)/,
				alias: 'symbol'
			},
			{
				pattern: /(\b(?:break|continue|return|super|this))@\w+/,
				lookbehind: true,
				alias: 'symbol'
			}
		],
		'class-name': [
			{
				// the name a declaration, a type check or a cast gives
				pattern: re(/(\b(?:as\??|class|interface|is|object|typealias)\s+)<qualifier>[A-Z]\w*\b/.source),
				lookbehind: true,
				inside: type
			},
			{
				// A type annotation, a supertype or a bound. A colon after another one or after
				// "?" is "::" or the Elvis operator, which no type follows.
				pattern: re(/((?:^|[^:?]):\s*)<qualifier>[A-Z]\w*\b/.source),
				lookbehind: true,
				inside: type
			}
		],
		'keyword': [
			{
				// a modifier is one only in front of what it modifies; "data" and "open" are
				// names everywhere else
				pattern: RegExp('(^|[^.])\\b(?:' + modifiers + ')\\b(?=\\s+(?:@|(?:' + modifiers + '|class|constructor|fun|get|init|interface|lateinit|object|set|suspend|tailrec|typealias|val|var)\\b))'),
				lookbehind: true
			},
			{
				pattern: /(^|[^.])\b(?:as|break|by|catch|class|continue|crossinline|do|else|finally|for|fun|if|import|in|interface|is|lateinit|noinline|null|object|package|reified|return|super|suspend|tailrec|this|throw|try|typealias|val|var|vararg|when|where|while)\b/,
				lookbehind: true
			},
			{
				// variance, "Array<out T>"; a parameter or a variable named "out" is common
				pattern: /([<,]\s*)out\b(?=\s+[\w*@])/,
				lookbehind: true
			},
			{
				pattern: /(^|[^.])\b(?:init(?=\s*\{)|constructor(?=\s*\())/,
				lookbehind: true
			},
			{
				// property accessors: "get() = x", "set(value) {", and "private set" ending its line
				pattern: /(^|[^.\w])(?:get|set)(?=[ \t]*(?:[\r\n]|$|\(\s*\w*\s*(?::[^()=]*)?\)\s*(?:[{:]|=(?!=))))/,
				lookbehind: true
			}
		],
		'boolean': /\b(?:false|true)\b/,
		'function': [
			{
				pattern: re(/`[^\r\n`]+`(?=<call>|\s*\{)/.source),
				greedy: true
			},
			re(/\b\w+(?=<call>)/.source),
			{
				// a call with a trailing lambda: "list.map {", "remember {"
				pattern: /(^|[^\w])[a-z_]\w*(?=\s*\{)/,
				lookbehind: true
			},
			{
				// a reference, "::println"
				pattern: /(::\s*)[a-z_]\w*/,
				lookbehind: true
			}
		],
		// After 'function', which has to see a call's type arguments before their names are
		// taken, and which makes a constructor or a composable call a function.
		'type-name': {
			// a name capitalised and with a lower-case letter
			pattern: re(/(^|[^\w.])<qualifier>[A-Z](?:[\d_A-Z]*[a-z]\w*)?\b/.source),
			lookbehind: true,
			inside: type,
			alias: 'class-name'
		},
		'number': [
			{
				// ".5f", first so its digits are not taken alone; not the second dot of "0..5"
				pattern: /(^|[^\w.])\.\d+(?:_+\d+)*(?:[eE][+-]?\d+(?:_+\d+)*)?[fF]?\b/,
				lookbehind: true
			},
			/\b0[xX][\da-fA-F]+(?:_+[\da-fA-F]+)*(?:[uU]L?|L)?\b|\b0[bB][01]+(?:_+[01]+)*(?:[uU]L?|L)?\b|\b\d+(?:_+\d+)*(?:\.\d+(?:_+\d+)*)?(?:[eE][+-]?\d+(?:_+\d+)*)?(?:[fF]|[uU]L?|L)?\b/
		],
		'constant': /\b[A-Z][A-Z_\d]+\b/,
		'operator': [
			{
				// Infix functions, only between two operands: "val or = 1" names a variable. An
				// operand may already be a token, a string or a number, so the text may end there.
				pattern: /((?:^|[\w)\]}])[ \t]+)(?:and|downTo|or|shl|shr|step|to|until|ushr|xor)(?=[ \t]+(?:[\w"'(\[{@-]|$))/,
				lookbehind: true
			},
			/\+[+=]?|-[-=>]?|==?=?|!(?:!|==?)?|[\/*%<>]=?|[?:]:?|\.\.<?|&&|\|\|/
		],
		'punctuation': /[{}[\];(),.:]/
	};

	interpolation.expression.inside = kotlin;

	Prism.languages.kotlin = kotlin;
	Prism.languages.kt = kotlin;
	Prism.languages.kts = kotlin;
}(Prism));
