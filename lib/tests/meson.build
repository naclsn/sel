all = [
  'parseType',
]

foreach name : all
  filename = 'test_' + name + '.cpp'
  test(name,
    executable(filename,
      filename,
      include_directories : [inc],
      link_with : [lib]
    )
  )
endforeach

unset_variable('all')
unset_variable('name')
unset_variable('filename')
