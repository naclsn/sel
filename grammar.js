/// <reference path="node_modules/tree-sitter-cli/dsl.d.ts" />
module.exports = grammar({

  name: 'sel',

  extras: $ => [$.comment, /\s|\r?\n/],

  rules: {

    script: $ => seq($.element, repeat(seq(choice(',', ';'), $.element))),
    comment: _ => /#[^\n]*/,

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

    //subscript: $ => seq('[', $.script, ']'),
    subscript: $ => seq('[', seq($.element, repeat(seq(choice(',', ';'), $.element))), ']'),

    number: _ => /[0-9]+(\.[0-9]+)?|0x[0-9A-F]+|0b[01]+|0o[0-7]/,
    string: _ => token(seq(':', /([^\\:]|\\[abtnvfre]|::)*/, ':')),
    list: $ => seq('{', seq($.element, repeat(seq(',', $.element))), '}'),

    reserved: _ => /[~^]/,

  },

});
