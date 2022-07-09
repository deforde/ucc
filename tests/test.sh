#!/usr/bin/env bash
set -u
IFS=$'\n\t'

DIR=${0%/*}

assert() {
  expected=$1
  input=$2

  $DIR/../build/ucc $input > tmp.s
  cc -o tmp tmp.s
  ./tmp
  actual=$?

  if [[ $actual == $expected ]]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi

  rm tmp.s tmp
}

assert 0 0
assert 42 42
assert 123 123
assert 21 "5+20-4"
assert 41 " 12 + 34 - 5 "
assert 47 "5+6*7"
assert 15 "5*(9-6)"
assert 4 "(3+5)/2"

echo OK

