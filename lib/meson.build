inc = include_directories('include')
install_subdir('include/sel', install_dir : 'include')

lib = library('sel',
  'src/utils.cpp',
  'src/errors.cpp',
  'src/types.cpp',
  'src/engine.cpp',
  'src/builtins.cpp',
  'src/parser.cpp',
  'src/visitors.cpp',
  # 'src/visitors/repr.cpp',
  'src/visitors/help.cpp',
  include_directories : [inc],
  install : true,
)

if not meson.is_subproject()
  subdir('tests')
endif
