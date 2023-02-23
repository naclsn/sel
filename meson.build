project('sel', 'cpp',
  default_options : ['cpp_std=c++17'],
  version : '0.1.8',
  license : 'MIT',
)

add_project_arguments(
  '-DSEL_VERSION=' + meson.project_version(),
  language : 'cpp',
)

if get_option('bins_min')
  add_project_arguments(
    '-DBINS_MIN=1',
    language : 'cpp',
  )
endif

subdir('lib')
subdir('src')
