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

if '0.60.0' < meson.version()
  unset_variable('all')
  unset_variable('name')
  unset_variable('filename')
endif
