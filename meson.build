project('sel', 'cpp',
  default_options : ['cpp_std=c++11'],
  version : '0.1.7',
  license : 'MIT',
)

add_project_arguments(
  '-DSEL_VERSION=' + meson.project_version(),
  language : 'cpp',
)

subdir('lib')
subdir('src')
