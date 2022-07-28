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

assert 0 '{0;}'
assert 42 '{42;}'
assert 21 '{5+20-4;}'
assert 41 '{ 12 + 34 - 5 ;}'
assert 47 '{5+6*7;}'
assert 15 '{5*(9-6);}'
assert 4 '{(3+5)/2;}'
assert 10 '{-10+20;}'
assert 10 '{- -10;}'
assert 10 '{- - +10;}'

assert 0 '{0==1;}'
assert 1 '{42==42;}'
assert 1 '{0!=1;}'
assert 0 '{42!=42;}'

assert 1 '{0<1;}'
assert 0 '{1<1;}'
assert 0 '{2<1;}'
assert 1 '{0<=1;}'
assert 1 '{1<=1;}'
assert 0 '{2<=1;}'

assert 1 '{1>0;}'
assert 0 '{1>1;}'
assert 0 '{1>2;}'
assert 1 '{1>=0;}'
assert 1 '{1>=1;}'
assert 0 '{1>=2;}'

assert 0 '{return 0;}'
assert 42 '{return 42;}'
assert 21 '{return 5+20-4;}'
assert 41 '{return  12 + 34 - 5 ;}'
assert 47 '{return 5+6*7;}'
assert 15 '{return 5*(9-6);}'
assert 4 '{return (3+5)/2;}'
assert 10 '{return -10+20;}'
assert 10 '{return - -10;}'
assert 10 '{return - - +10;}'

assert 0 '{return 0==1;}'
assert 1 '{return 42==42;}'
assert 1 '{return 0!=1;}'
assert 0 '{return 42!=42;}'

assert 1 '{return 0<1;}'
assert 0 '{return 1<1;}'
assert 0 '{return 2<1;}'
assert 1 '{return 0<=1;}'
assert 1 '{return 1<=1;}'
assert 0 '{return 2<=1;}'

assert 1 '{return 1>0;}'
assert 0 '{return 1>1;}'
assert 0 '{return 1>2;}'
assert 1 '{return 1>=0;}'
assert 1 '{return 1>=1;}'
assert 0 '{return 1>=2;}'

assert 3 '{a = 3;}'
assert 3 '{a = 3; a;}'
assert 7 '{a = 3; b = 4; a + b;}'
assert 14 '{a = 3; b = 5 * 6 - 8; a + b / 2;}'
assert 17 '{a = 4; b = 5 * (8 - 2); (a + b) / 2;}'

assert 3 '{foo = 3;}'
assert 3 '{foo = 3; foo;}'
assert 7 '{foo = 3; bar = 4; foo + bar;}'
assert 14 '{foo = 3; bar = 5 * 6 - 8; foo + bar / 2;}'
assert 17 '{foo = 4; bar = 5 * (8 - 2); (foo + bar) / 2;}'

assert 3 '{a=3; return a;}'
assert 8 '{a=3; z=5; return a+z;}'

assert 3 '{a=3; return a;}'
assert 8 '{a=3; z=5; return a+z;}'
assert 6 '{a=b=3; return a+b;}'
assert 3 '{foo=3; return foo;}'
assert 8 '{foo123=3; bar=5; return foo123+bar;}'

assert 1 '{return 1; 2; 3;}'
assert 2 '{1; return 2; 3;}'
assert 3 '{1; 2; return 3;}'

assert 3 '{return foo = 3;}'
assert 3 '{foo = 3; return foo;}'
assert 7 '{foo = 3; bar = 4; return foo + bar;}'
assert 14 '{foo = 3; bar = 5 * 6 - 8; return foo + bar / 2;}'
assert 17 '{foo = 4; bar = 5 * (8 - 2); return (foo + bar) / 2;}'

assert 2 '{return 2; foo = 3;}'
assert 3 '{foo = 3; return foo; foo = 4;}'
assert 7 '{foo = 3; bar = 4; return foo + bar; return foo * bar;}'
assert 22 '{foo = 3; return bar = 5 * 6 - 8; foo + bar / 2;}'
assert 30 '{foo = 4; return bar = 5 * (8 - 2); (foo + bar) / 2; return 6;}'

assert 3 '{return _foo1 = 3;}'
assert 3 '{_foo1 = 3; return _foo1;}'
assert 7 '{_foo1 = 3; bar_baz = 4; return _foo1 + bar_baz;}'
assert 14 '{_foo1 = 3; bar_baz = 5 * 6 - 8; return _foo1 + bar_baz / 2;}'
assert 17 '{_foo1 = 4; bar_baz = 5 * (8 - 2); return (_foo1 + bar_baz) / 2;}'

assert 3 '{a = 1; b = 2; if (a) b = 3; return b;}'
assert 2 '{a = 0; b = 2; if (a) b = 3; return b;}'
assert 1 '{_foo1 = 3; if (_foo1) return 1; return 0;}'
assert 2 '{_foo1 = 0; if (_foo1) return 1; return 2;}'
assert 3 '{_foo1 = 3; bar_baz = 4; if (bar_baz) return _foo1; return 2;}'
assert 3 '{_foo1 = 3; bar_baz = 0; if (bar_baz) return 1; return _foo1;}'

assert 3 '{a = 3; ; a;}'
assert 5 '{;;; return 5;}'

assert 3 '{ if (0) return 2; return 3; }'
assert 3 '{ if (1-1) return 2; return 3; }'
assert 2 '{ if (1) return 2; return 3; }'
assert 2 '{ if (2-1) return 2; return 3; }'
assert 4 '{ if (0) { 1; 2; return 3; } else { return 4; } }'
assert 3 '{ if (1) { 1; 2; return 3; } else { return 4; } }'

assert 11 '{ j = 1; for (i = 0; i < 5; i = i + 1) { j = j + i; } return j; }'
assert 55 '{ i=0; j=0; for (i=0; i<=10; i=i+1) j=i+j; return j; }'
assert 3 '{ for (;;) {return 3;} return 5; }'

echo OK

