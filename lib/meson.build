inc = include_directories('include', '.') # '.' is for generated headers
install_subdir('include/sel', install_dir : 'include')

pre = custom_target('prelude',
  command : [
    executable('primer',
      'src/primer.cpp',
      'src/types.cpp',
      include_directories : [inc],
      native : true,
      install : false,
    ),
    '@INPUT@',
    '@OUTPUT@',
  ],
  input : ['src/prelude.txt'],
  output : ['prelude.hpp', 'prelude.cpp', 'prelude_visit_each'],
  install : true,
  install_dir : ['include/sel', false, 'include/sel'],
)

lib = library('sel',
  'src/errors.cpp',
  'src/types.cpp',
  'src/engine.cpp',
  'src/parser.cpp',
  'src/visitors.cpp',
  'src/visitors/repr.cpp',
  pre,
  include_directories : [inc],
  install : true,
)

if not meson.is_subproject()
  subdir('tests')
endif
