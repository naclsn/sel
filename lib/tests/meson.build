all = [
  'parseApp',
  'parseType',
  'VisRepr',
  'Type_applied',
  'bins_applied',
  'runApp',
  'def_kw',
  'end',
  'each',
]

foreach name : all
  filename = 'test_' + name + '.cpp'
  test(name,
    executable('test_' + name,
      filename,
      include_directories : [inc],
      link_with : [lib],
      dependencies : [llvm], # link_with is not enough to bring the cxxflags (-I) over
    ),
  )
endforeach
