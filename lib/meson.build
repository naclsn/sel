inc = include_directories('include')

lib = library('sel',
  'src/errors.cpp',
  'src/types.cpp',
  include_directories : [inc]
)

if not meson.is_subproject()
  subdir('tests')
endif
