__sel() {
  local curr=`tr -c a-z '\n' <<<$2 | tail -n1`
  COMPREPLY=(`$1 -l 2>/dev/null | grep ^$curr | sed "s/\(.*\)/${2%$curr}\1/"`)
}
complete -F __sel sel
