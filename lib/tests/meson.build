all = [
  'parseApp',
  'parseType',
  'VisRepr',
  'Type_applied',
  'bins_applied',
]

foreach name : all
  filename = 'test_' + name + '.cpp'
  test(name,
    executable(filename,
      filename,
      include_directories : [inc],
      link_with : [lib],
    ),
  )
endforeach
