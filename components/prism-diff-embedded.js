/**
 * Prism's diff grammar, changed to carry the code of the files it compares. Its
 * embedded-language tokens, the context lines and the declaration a hunk header may end in,
 * are highlighted in the language of the file named before them, or in C# when the diff is
 * highlighted as "diff-cs". Added and removed lines keep the diff's own colours.
 */
(function (Prism) {
	var diff = Prism.languages.diff;

	// The language of a file. Its path names one only when it is a known name; the extension in
	// it, or the name of a file with none ("Makefile"), names it more closely. A path that names
	// nothing still ends the language of the file before. git quotes a path with unusual
	// characters, a timestamp may follow it after a tab, and "/dev/null" is no file.
	var file = {
		'embedded-language-name': {
			pattern: /(^[-+]{3} )(?!\/dev\/null)[^\t]*\S/,
			lookbehind: true,
			inside: {
				'embedded-language-name': [
					{
						pattern: /(\.)[\w+-]+(?="?$)/,
						lookbehind: true
					},
					{
						pattern: /(^"?(?:.*\/)?)[\w+-]+(?="?$)/,
						lookbehind: true
					}
				]
			}
		}
	};

	diff.coord = [
		// The files, "---" the old one and "+++" the new one, coloured as their lines. Only the
		// two together name them: alone, either may be a removed "--" or an added "++" line.
		{
			pattern: /^-{3} .*(?:\r\n?|\n)\+{3} .*$/m,
			inside: {
				'coord': [
					{
						pattern: /^-{3}.*/,
						alias: 'deleted',
						inside: file
					},
					{
						pattern: /^\+{3}.*/m,
						alias: 'inserted',
						inside: file
					}
				]
			}
		},
		{
			pattern: /^-{3}.*$/m,
			alias: 'deleted'
		},
		{
			pattern: /^\+{3}.*$/m,
			alias: 'inserted'
		},
		/^\*{3}.*$/m,
		// "@@ -1,5 +1,5 @@", or "@@@" in a combined diff, then the declaration the hunk is in.
		{
			pattern: /^@@.*@@.*$/m,
			inside: {
				'embedded-language': {
					pattern: /(^(@{2,})[^@\r\n]*\2[ \t]*)\S.*/,
					lookbehind: true
				}
			}
		},
		/^\d.*$/m
	];

	diff.unchanged.inside = {
		'embedded-language': diff.unchanged.inside.line,
		'prefix': diff.unchanged.inside.prefix
	};
}(Prism));
