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

assert 0 '0;'
assert 42 '42;'
assert 21 '5+20-4;'
assert 41 ' 12 + 34 - 5 ;'
assert 47 '5+6*7;'
assert 15 '5*(9-6);'
assert 4 '(3+5)/2;'
assert 10 '-10+20;'
assert 10 '- -10;'
assert 10 '- - +10;'

assert 0 '0==1;'
assert 1 '42==42;'
assert 1 '0!=1;'
assert 0 '42!=42;'

assert 1 '0<1;'
assert 0 '1<1;'
assert 0 '2<1;'
assert 1 '0<=1;'
assert 1 '1<=1;'
assert 0 '2<=1;'

assert 1 '1>0;'
assert 0 '1>1;'
assert 0 '1>2;'
assert 1 '1>=0;'
assert 1 '1>=1;'
assert 0 '1>=2;'

assert 3 'a = 3; a;'
assert 7 'a = 3; b = 4; a + b;'
assert 14 'a = 3; b = 5 * 6 - 8; a + b / 2;'
assert 17 'a = 4; b = 5 * (8 - 2); (a + b) / 2;'

assert 3 'foo = 3; foo;'
assert 7 'foo = 3; bar = 4; foo + bar;'
assert 14 'foo = 3; bar = 5 * 6 - 8; foo + bar / 2;'
assert 17 'foo = 4; bar = 5 * (8 - 2); (foo + bar) / 2;'

echo OK

