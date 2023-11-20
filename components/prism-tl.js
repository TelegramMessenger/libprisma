Prism.languages.tl = {
	'builtin': [
        /---(functions|types)---/,
        {
            pattern: /([^a-z0-9_])#/,
            lookbehind: true,
            greedy: true
        },
        {
            pattern: /(\w*:)(#|flags\.\d*\?)/,
            lookbehind: true,
            greedy: true
        }
    ],
	'comment': [
		{
			pattern: /(^|[^\\])\/\*[\s\S]*?(?:\*\/|$)/,
			lookbehind: true,
			greedy: true
		},
		{
			pattern: /(^|[^\\:])\/\/.*/,
			lookbehind: true,
			greedy: true
		}
	],
    'function': {
        pattern: /(:(flags\d?\.\d+\?)?)([a-zA-Z0-9_!<>]+)/,
        lookbehind: true,
        greedy: true
    },
	'punctuation': /[{}[\];]/,
	'operator': /[:=]/,
	'entity': /#[a-fA-F0-9]+/,
    'class-name': {
        pattern: /(\s*=\s*)(.*);/,
        lookbehind: true,
        greedy: true
    },
};

Prism.languages.typelanguage = Prism.languages.tl;
