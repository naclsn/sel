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
  # pre,
  include_directories : [inc],
  # version : '..',
  install : true,
)

# unset_variable(pre)

if not meson.is_subproject()
  subdir('tests')
endif
