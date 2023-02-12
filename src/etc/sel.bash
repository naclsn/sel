__sel() {
  sel=$1
  word=$2
  before=$3

  COMPREPLY=()

  case $before in
    -f)
      COMPREPLY=($(compgen -f -- "$word"))
      ;;
    $sel)
      if [[ $word == -* ]]
        then
          COMPREPLY=($(compgen -W '-h -l -D -n -s -t -f --help --version' -- "$word"))
          return
      fi
      ;;& # fallthrough
    *)
      # do not complete in string literals
      local nb_col=$(<<<$COMP_LINE $sel count ::::)
      if [ 0 -eq $((nb_col & 1)) ]
        then
          # take the [a-z] word from the end
          #local curr=$(<<<$word $sel reverse, takewhile isasciilower, reverse)
          local curr=$(tr -c a-z '\n' <<<$word | tail -n1)
          COMPREPLY=($(compgen -P "${word%$curr}" -W "`$sel -l 2>/dev/null`" -- $curr))
      fi
      ;;
  esac
} && complete -o filenames -F __sel sel
