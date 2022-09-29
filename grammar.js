/// <reference path="node_modules/tree-sitter-cli/dsl.d.ts" />
// grammar is simplified; could not get it to correct precedence for:
// - `map -1` should be (app (base "map") (arg (app (base "-") (arg "1")))
// - `%-1` should be (app (base (app (base "%") (arg "-"))) (arg "1"))
// that is unop binds tighter with the following expression than binop than name
module.exports = grammar({

  name: 'nasm',

  extras: $ => [$.comment, /\s|\r?\n/],

  rules: {

    script: $ => seq($.fitting, repeat(seq(',', $.fitting)), optional(',')),
    comment: _ => /#[^\n]*\r?\n/,

    fitting: $ => $._expression,

    _expression: $ => choice(
      $.name,
      $.unop,
      $.binop,
      $.literal,
      $.grouping,
      $.application,
    ),

    application: $ => prec.left(seq(
      alias($._expression, $.base),
      alias($._expression, $.argument),
    )),

    name: _ => /[a-z]+/,
    unop: _ => choice(...'%@'.split('')),
    binop: _ => choice(...'+-./:=_~'.split('')),

    literal: $ => choice($.number, $.string),

    grouping: $ => seq('[', $._expression, ']'),

    number: _ => /[0-9]+(\.[0-9]+)?|0x[0-9A-F]+|0b[01]+|0o[0-7]/,
    string: $ => $._string,
    _string: $ => seq('{', repeat(choice(/[^}]/, $._string)), '}'),

    reserved: _ => /[\^]/,

  },

});

