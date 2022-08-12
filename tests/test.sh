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

assert 3 '{int a = 3;}'
assert 3 '{int a = 3; a;}'
assert 7 '{int a = 3; int b = 4; a + b;}'
assert 14 '{int a = 3; int b = 5 * 6 - 8; a + b / 2;}'
assert 17 '{int a = 4; int b = 5 * (8 - 2); (a + b) / 2;}'

assert 3 '{int foo = 3;}'
assert 3 '{int foo = 3; foo;}'
assert 7 '{int foo = 3; int bar = 4; foo + bar;}'
assert 14 '{int foo = 3; int bar = 5 * 6 - 8; foo + bar / 2;}'
assert 17 '{int foo = 4; int bar = 5 * (8 - 2); (foo + bar) / 2;}'

assert 3 '{int a=3; return a;}'
assert 8 '{int a=3; int z=5; return a+z;}'

assert 3 '{int a=3; return a;}'
assert 8 '{int a=3; int z=5; return a+z;}'
assert 6 '{int a; int b; a=b=3; return a+b;}'
assert 3 '{int foo=3; return foo;}'
assert 8 '{int foo123=3; int bar=5; return foo123+bar;}'

assert 1 '{return 1; 2; 3;}'
assert 2 '{1; return 2; 3;}'
assert 3 '{1; 2; return 3;}'

assert 3 '{int foo; return foo = 3;}'
assert 3 '{int foo = 3; return foo;}'
assert 7 '{int foo = 3; int bar = 4; return foo + bar;}'
assert 14 '{int foo = 3; int bar = 5 * 6 - 8; return foo + bar / 2;}'
assert 17 '{int foo = 4; int bar = 5 * (8 - 2); return (foo + bar) / 2;}'

assert 2 '{return 2; int foo; foo = 3;}'
assert 3 '{int foo = 3; return foo; foo = 4;}'
assert 7 '{int foo = 3; int bar = 4; return foo + bar; return foo * bar;}'
assert 22 '{int foo = 3; int bar; return bar = 5 * 6 - 8; foo + bar / 2;}'
assert 30 '{int foo = 4; int bar; return bar = 5 * (8 - 2); (foo + bar) / 2; return 6;}'

assert 3 '{int _foo1; return _foo1 = 3;}'
assert 3 '{int _foo1 = 3; return _foo1;}'
assert 7 '{int _foo1 = 3; int bar_baz = 4; return _foo1 + bar_baz;}'
assert 14 '{int _foo1 = 3; int bar_baz = 5 * 6 - 8; return _foo1 + bar_baz / 2;}'
assert 17 '{int _foo1 = 4; int bar_baz = 5 * (8 - 2); return (_foo1 + bar_baz) / 2;}'

assert 3 '{int a = 1; int b = 2; if (a) b = 3; return b;}'
assert 2 '{int a = 0; int b = 2; if (a) b = 3; return b;}'
assert 1 '{int _foo1 = 3; if (_foo1) return 1; return 0;}'
assert 2 '{int _foo1 = 0; if (_foo1) return 1; return 2;}'
assert 3 '{int _foo1 = 3; int bar_baz = 4; if (bar_baz) return _foo1; return 2;}'
assert 3 '{int _foo1 = 3; int bar_baz = 0; if (bar_baz) return 1; return _foo1;}'

assert 3 '{int a = 3; ; a;}'
assert 5 '{;;; return 5;}'
assert 3 '{ {1; {2;} return 3;} }'

assert 3 '{ if (0) return 2; return 3; }'
assert 3 '{ if (1-1) return 2; return 3; }'
assert 2 '{ if (1) return 2; return 3; }'
assert 2 '{ if (2-1) return 2; return 3; }'
assert 4 '{ if (0) { 1; 2; return 3; } else { return 4; } }'
assert 3 '{ if (1) { 1; 2; return 3; } else { return 4; } }'

assert 11 '{ int j = 1; int i = 0; for (i = 0; i < 5; i = i + 1) { j = j + i; } return j; }'
assert 55 '{ int i=0; int j=0; for (i=0; i<=10; i=i+1) j=i+j; return j; }'
assert 3 '{ for (;;) {return 3;} return 5; }'
assert 10 '{ int i=0; while(i<10) { i=i+1; } return i; }'

assert 3 '{ int x=3; return *&x; }'
assert 3 '{ int x=3; int y=&x; int z=&y; return **z; }'
assert 5 '{ int x=3; int y=5; return *(&x-1); }'
assert 3 '{ int x=3; int y=5; return *(&y+1); }'
assert 5 '{ int x=3; int y=&x; *y=5; return x; }'
assert 7 '{ int x=3; int y=5; *(&x-1)=7; return y; }'
assert 7 '{ int x=3; int y=5; *(&y+2-1)=7; return x; }'
assert 5 '{ int x=3; return (&x+2)-&x+3; }'

echo OK

