project('sel', 'cpp',
  default_options : ['cpp_std=c++11']
)

subdir('lib')

app = executable('sel',
  'src/main.cpp',
  include_directories : [inc],
  link_with : [lib]
)
