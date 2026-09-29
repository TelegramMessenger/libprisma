/**
 * @file Prism.js definition for Tolk
 * @link https://docs.ton.org/tolk/overview
 * @version 1.4.0
 * @author Danil Ovchinnikov (https://github.com/Danil42Russia)
 * @license MIT
 */
(function(Prism) {
  Prism.languages.tolk = {
    'comment': {
      pattern: /\/\/.*/,
      greedy: true,
    },

    'block-comment': {
      pattern: /\/\*[\s\S]*?(?:\*\/|$)/,
      greedy: true,
    },

    'string': [
      {
        pattern: /"""(?:\\[\s\S]|(?!""")[\s\S])*(?:"""|$)/,
        greedy: true,
      },
      {
        pattern: /"(?:\\.|[^"\\\r\n])*(?:"|$)/,
        greedy: true,
      },
    ],

    'attr-name': /@[a-zA-Z0-9_.]+/,

    'number': [
      /\b0x[0-9A-Fa-f_]+\b/,
      /\b0b[01_]+\b/,
      /\b\d[\d_]*\b/,
    ],

    'boolean': /\b(?:false|null|true)\b/,

    'constant': /\b[A-Z][A-Z0-9_]*_[A-Z0-9_]*\b/,

    'keyword': [
      /!is\b/,
      /\b(?:as|asm|assert|break|builtin|catch|const|continue|contract|do|else|enum|fun|get|global|if|import|is|lazy|match|mutate|private|readonly|repeat|return|self|struct|throw|tolk|try|type|val|var|while)\b/,
    ],

    'function': [
      /`[^`\r\n]+`(?=\s*(?:<[^(){};\r\n]*>\s*)?\()/,
      /\b[a-zA-Z_$][a-zA-Z0-9_$]*\b(?=\s*(?:<[^(){};\r\n]*>\s*)?\()/,
    ],

    'builtin': [
      /\b(?:Cell|address|any_address|array|blockchain|bool|builder|cell|coins|continuation|contract|debug|dict|lisp_list|map|never|random|reflect|slice|string|tuple|unknown|void)\b/,
      /\b(?:int\d*|uint\d+|varint\d+|varuint\d+|bits\d+|bytes\d+)\b/,
    ],

    'class-name': /\b[A-Z][a-zA-Z0-9_$]*\b/,

    'property': {
      pattern: /(\.)(?:`[^`\r\n]+`|[a-zA-Z_$][a-zA-Z0-9_$]*)/,
      lookbehind: true,
    },

    'operator': /<<=|>>=|<=>|~>>|\^>>|==|!=|<=|>=|<<|>>|&&|\|\||~\/|\^\/|\+=|-=|\*=|\/=|%=|&=|\|=|\^=|->|=>|\+\+|--|\?\?|[+\-*\/%?=<>!&|^~]/,

    'punctuation': /[.,;:(){}\[\]]/,

    'symbol': /`[^`\r\n]+`/,
  };
}(Prism));
