/**
 * Prism's diff grammar, changed to carry the code of the files it compares: highlighted as
 * "diff-cs", a diff's embedded-language tokens are highlighted as C#. They are the context
 * lines and the declaration a hunk header may end in; added and removed lines keep the diff's
 * own colours.
 */
(function (Prism) {
	var diff = Prism.languages.diff;

	diff.coord = [
		// The files: "---" names the old one and "+++" the new one, coloured as their lines.
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
