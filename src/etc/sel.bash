__sel() {
  #printf '\e7\e[E\r\e[K{!'

  COMPREPLY=()

  case $3 in
    $1$2)
      COMPREPLY=($(compgen -W '-h -l -D -n -s -t -f --help --version' -- "$2"))
      ;;
    -f)
      COMPREPLY=($(compgen -f -- "$2"))
      ;;
    *)
      # do not complete in string literals
      #local nb_col=`<<<$COMP_LINE $1 take $COMP_POINT, split ::::, len`
      local nb_col=$(<<<$COMP_LINE $1 codepoints, take $COMP_POINT, uncodepoints, split ::::, join :\\n: | wc -l)
      if [ 0 -eq $((nb_col & 1)) ]
        then
          # take the [a-z] word from the end
          #local curr=`<<<$2 $1 reverse, takewhile isasciilower, reverse`
          local curr=`tr -c a-z '\n' <<<$2 | tail -n1`
          COMPREPLY=($(compgen -P "${2%$curr}" -W "`$1 -l 2>/dev/null`" -- $curr))
      fi
      ;;
  esac

  #printf '!}\e8'
} && complete -o filenames -F __sel sel
