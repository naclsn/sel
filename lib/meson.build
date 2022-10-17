inc = include_directories('include')
install_subdir('include/sel', install_dir : 'include')

# pre = custom_target('prelude',
#   command : executable('primer',
#     'src/types.cpp',
#     '..',
#     include_directories : [inc],
#     native : true,
#     install : false,
#   ),
#   output : ['..'],
# )

lib = library('sel',
  'src/errors.cpp',
  'src/types.cpp',
  'src/engine.cpp',
  'src/parser.cpp',
  'src/visitors/repr.cpp',
  # pre,
  include_directories : [inc],
  # version : '..',
  install : true,
)

# if '0.60.0' < meson.version()
#   unset_variable(pre)
# endif

if not meson.is_subproject()
  subdir('tests')
endif
