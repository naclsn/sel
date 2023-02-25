inc = include_directories('include')
install_subdir('include/sel', install_dir : 'include')

# note to self: if it is not finding the correct version
# despite having `llvm-config --version` check out, this
# is because it is only trying `llvm-config-64`...
llvm = dependency('llvm',
  version : '< 9',
  required : get_option('llvm_codegen'),
)

lib = library('sel',
  'src/utils.cpp',
  'src/errors.cpp',
  'src/types.cpp',
  'src/engine.cpp',
  'src/builtins.cpp',
  'src/parser.cpp',
  'src/unicode.cpp',
  'src/visitors.cpp',
  'src/visitors/repr.cpp',
  'src/visitors/help.cpp',
  'src/visitors/codegen.cpp',
  include_directories : [inc],
  dependencies : [llvm],
  install : true,
)

if not meson.is_subproject()
  subdir('tests')
endif
