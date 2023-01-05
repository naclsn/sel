/// <reference path="node_modules/tree-sitter-cli/dsl.d.ts" />
module.exports = grammar({

  name: 'sel',

  extras: $ => [$.comment, /\s|\r?\n/],

  rules: {

    script: $ => $._elements1,
    comment: _ => /#[^\n]*/,

    _elements1: $ => seq($.element, repeat(seq(choice(',', ';'), $.element))),
    element: $ => seq($.atom, repeat($.atom)),

    atom: $ => choice(
      $.literal,
      $.name,
      $.subscript,
      seq($.unop, choice(prec(1, seq($.binop, $.atom)), $.atom)),
      seq($.binop, $.atom),
    ),

    literal: $ => choice($.number, $.string, $.list),

    name: _ => /[a-z]+/,
    unop: _ => choice(...'%@'.split('')),
    binop: _ => choice(...'+-./=_'.split('')),

    subscript: $ => seq('[', optional($._elements1), ']'),

    number: _ => /[0-9]+(\.[0-9]+)?|0x[0-9A-F]+|0b[01]+|0o[0-7]/,
    string: _ => seq(':', /([^\\:]|\\[abtnvfre]|::)*/, ':'),
    list: $ => seq('{', $._elements1, '}'),

    reserved: _ => /[~^]/,

  },

});
