inc = include_directories('include')
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
    '--libroot', '@CURRENT_SOURCE_DIR@',
    '--private', '@PRIVATE_DIR@',
    ' @INPUT@ ', '@OUTPUT@',
  ],
  input : ['src/prelude.txt'],
  output : [
    'prelude.hpp', # 'include/sel/prelude.hpp',
    'prelude.cpp', # 'src/prelude.cpp',
  ],
  install : true,
  install_dir : [
    'include/sel', # .hpp
    false, # .cpp
  ],
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
  # version : '..',
  install : true,
)

if '0.60.0' < meson.version()
  unset_variable('pre')
endif

if not meson.is_subproject()
  subdir('tests')
endif
