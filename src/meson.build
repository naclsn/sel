# completions
if true
  bash_comp = dependency('bash-completion',
    required : false,
    not_found_message : 'not able to known where to install bash completion',
  )
  if bash_comp.found()
    bash_comp_dir = bash_comp.get_variable('completionsdir')
    install_data('etc/sel.bash',
      install_dir : bash_comp_dir,
      install_tag : 'completions',
    )
  endif

  # TODO: don't known how to zsh completion...
  # https://zsh.sourceforge.io/Doc/Release/Completion-System.html -> 20.8
  # https://github.com/zsh-users/zsh-completions/blob/master/zsh-completions-howto.org

endif
# end completions

install_man('etc/sel.1')

app = executable('sel',
  'main.cpp',
  include_directories : [inc],
  link_with : [lib],
  install : true,
)
