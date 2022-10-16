project('sel', 'cpp',
  default_options : ['cpp_std=c++11'],
  # license : '..',
  # version : '..' or file('VERSION'),
)

subdir('lib')

app = executable('sel',
  'src/main.cpp',
  include_directories : [inc],
  link_with : [lib],
  install : true,
)
