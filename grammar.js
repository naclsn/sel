/// <reference path="node_modules/tree-sitter-cli/dsl.d.ts" />
module.exports = grammar({

  name: 'nasm',

  extras: $ => [$.comment, /\s|\r?\n/],

  rules: {

    script: $ => $._elements1,
    comment: _ => /#[^\n]*\r?\n/,

    _elements1: $ => seq($.element, repeat(seq(',', $.element)), optional(',')),
    element: $ => seq($.atom, repeat($.atom)),

    atom: $ => choice(
      $.literal,
      $.name,
      $.subscript,
      seq($.unop, choice(prec(1, seq($.binop, $.atom)), $.atom)),
      seq($.binop, $.atom),
    ),

    literal: $ => choice($.number, $.string),

    name: _ => /[a-z]+/,
    unop: _ => choice(...'%@'.split('')),
    binop: _ => choice(...'+-./:=_~'.split('')),

    subscript: $ => seq('[', optional($._elements1), ']'),

    number: _ => /[0-9]+(\.[0-9]+)?|0x[0-9A-F]+|0b[01]+|0o[0-7]/,
    string: $ => $._string, _string: $ => seq('{', repeat(choice(/[^}]/, $._string)), '}'),

    reserved: _ => /[\^]/,

  },

});
