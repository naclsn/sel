inc = include_directories('include')
install_subdir('include/sel', install_dir : 'include')

lib = library('sel',
  'src/application.cpp',
  'src/builtins.cpp',
  'src/engine.cpp',
  'src/errors.cpp',
  'src/parser.cpp',
  'src/types.cpp',
  'src/unicode.cpp',
  'src/utils.cpp',
  'src/visitors.cpp',
  'src/visitors/help.cpp',
  'src/visitors/name.cpp',
  'src/visitors/repr.cpp',
  'src/visitors/show.cpp',
  include_directories : [inc],
  install : true,
)

if not meson.is_subproject()
  subdir('tests')
endif
