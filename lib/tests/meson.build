all = [
  'parseApp',
  'parseType',
  'VisRepr',
  'Type_applied',
  'bins_applied',
  'runApp',
  'def_kw',
]

foreach name : all
  filename = 'test_' + name + '.cpp'
  test(name,
    executable('test_' + name,
      filename,
      include_directories : [inc],
      link_with : [lib],
    ),
  )
endforeach
