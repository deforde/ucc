#!/usr/bin/env bash
set -u
IFS=$'\n\t'

DIR=${0%/*}

cat <<EOF | gcc -xc -c -o tmp2.o -
int ret3() { return 3; }
int ret5() { return 5; }
int add(int x, int y) { return x+y; }
int sub(int x, int y) { return x-y; }
int add6(int a, int b, int c, int d, int e, int f) {
  return a+b+c+d+e+f;
}
EOF

assert() {
  expected=$1
  input=$2

  $DIR/../build/ucc $input > tmp.s
  cc -o tmp tmp.s tmp2.o
  ./tmp
  actual=$?

  if [[ $actual == $expected ]]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    rm tmp2.o
    exit 1
  fi

  rm tmp.s tmp
}

assert 0 'int main() {0;}'
assert 42 'int main() {42;}'
assert 21 'int main() {5+20-4;}'
assert 41 'int main() { 12 + 34 - 5 ;}'
assert 47 'int main() {5+6*7;}'
assert 15 'int main() {5*(9-6);}'
assert 4 'int main() {(3+5)/2;}'
assert 10 'int main() {-10+20;}'
assert 10 'int main() {- -10;}'
assert 10 'int main() {- - +10;}'

assert 0 'int main() {0==1;}'
assert 1 'int main() {42==42;}'
assert 1 'int main() {0!=1;}'
assert 0 'int main() {42!=42;}'

assert 1 'int main() {0<1;}'
assert 0 'int main() {1<1;}'
assert 0 'int main() {2<1;}'
assert 1 'int main() {0<=1;}'
assert 1 'int main() {1<=1;}'
assert 0 'int main() {2<=1;}'

assert 1 'int main() {1>0;}'
assert 0 'int main() {1>1;}'
assert 0 'int main() {1>2;}'
assert 1 'int main() {1>=0;}'
assert 1 'int main() {1>=1;}'
assert 0 'int main() {1>=2;}'

assert 0 'int main() {return 0;}'
assert 42 'int main() {return 42;}'
assert 21 'int main() {return 5+20-4;}'
assert 41 'int main() {return  12 + 34 - 5 ;}'
assert 47 'int main() {return 5+6*7;}'
assert 15 'int main() {return 5*(9-6);}'
assert 4 'int main() {return (3+5)/2;}'
assert 10 'int main() {return -10+20;}'
assert 10 'int main() {return - -10;}'
assert 10 'int main() {return - - +10;}'

assert 0 'int main() {return 0==1;}'
assert 1 'int main() {return 42==42;}'
assert 1 'int main() {return 0!=1;}'
assert 0 'int main() {return 42!=42;}'

assert 1 'int main() {return 0<1;}'
assert 0 'int main() {return 1<1;}'
assert 0 'int main() {return 2<1;}'
assert 1 'int main() {return 0<=1;}'
assert 1 'int main() {return 1<=1;}'
assert 0 'int main() {return 2<=1;}'

assert 1 'int main() {return 1>0;}'
assert 0 'int main() {return 1>1;}'
assert 0 'int main() {return 1>2;}'
assert 1 'int main() {return 1>=0;}'
assert 1 'int main() {return 1>=1;}'
assert 0 'int main() {return 1>=2;}'

assert 3 'int main() {int a = 3;}'
assert 3 'int main() {int a = 3; a;}'
assert 7 'int main() {int a = 3; int b = 4; a + b;}'
assert 14 'int main() {int a = 3; int b = 5 * 6 - 8; a + b / 2;}'
assert 17 'int main() {int a = 4; int b = 5 * (8 - 2); (a + b) / 2;}'

assert 3 'int main() {int foo = 3;}'
assert 3 'int main() {int foo = 3; foo;}'
assert 7 'int main() {int foo = 3; int bar = 4; foo + bar;}'
assert 14 'int main() {int foo = 3; int bar = 5 * 6 - 8; foo + bar / 2;}'
assert 17 'int main() {int foo = 4; int bar = 5 * (8 - 2); (foo + bar) / 2;}'

assert 3 'int main() {int a=3; return a;}'
assert 8 'int main() {int a=3; int z=5; return a+z;}'

assert 3 'int main() {int a=3; return a;}'
assert 8 'int main() {int a=3; int z=5; return a+z;}'
assert 6 'int main() {int a; int b; a=b=3; return a+b;}'
assert 3 'int main() {int foo=3; return foo;}'
assert 8 'int main() {int foo123=3; int bar=5; return foo123+bar;}'

assert 1 'int main() {return 1; 2; 3;}'
assert 2 'int main() {1; return 2; 3;}'
assert 3 'int main() {1; 2; return 3;}'

assert 3 'int main() {int foo; return foo = 3;}'
assert 3 'int main() {int foo = 3; return foo;}'
assert 7 'int main() {int foo = 3; int bar = 4; return foo + bar;}'
assert 14 'int main() {int foo = 3; int bar = 5 * 6 - 8; return foo + bar / 2;}'
assert 17 'int main() {int foo = 4; int bar = 5 * (8 - 2); return (foo + bar) / 2;}'

assert 2 'int main() {return 2; int foo; foo = 3;}'
assert 3 'int main() {int foo = 3; return foo; foo = 4;}'
assert 7 'int main() {int foo = 3; int bar = 4; return foo + bar; return foo * bar;}'
assert 22 'int main() {int foo = 3; int bar; return bar = 5 * 6 - 8; foo + bar / 2;}'
assert 30 'int main() {int foo = 4; int bar; return bar = 5 * (8 - 2); (foo + bar) / 2; return 6;}'

assert 3 'int main() {int _foo1; return _foo1 = 3;}'
assert 3 'int main() {int _foo1 = 3; return _foo1;}'
assert 7 'int main() {int _foo1 = 3; int bar_baz = 4; return _foo1 + bar_baz;}'
assert 14 'int main() {int _foo1 = 3; int bar_baz = 5 * 6 - 8; return _foo1 + bar_baz / 2;}'
assert 17 'int main() {int _foo1 = 4; int bar_baz = 5 * (8 - 2); return (_foo1 + bar_baz) / 2;}'

assert 3 'int main() {int a = 1; int b = 2; if (a) b = 3; return b;}'
assert 2 'int main() {int a = 0; int b = 2; if (a) b = 3; return b;}'
assert 1 'int main() {int _foo1 = 3; if (_foo1) return 1; return 0;}'
assert 2 'int main() {int _foo1 = 0; if (_foo1) return 1; return 2;}'
assert 3 'int main() {int _foo1 = 3; int bar_baz = 4; if (bar_baz) return _foo1; return 2;}'
assert 3 'int main() {int _foo1 = 3; int bar_baz = 0; if (bar_baz) return 1; return _foo1;}'

assert 3 'int main() {int a = 3; ; a;}'
assert 5 'int main() {;;; return 5;}'
assert 3 'int main() { {1; {2;} return 3;} }'

assert 3 'int main() { if (0) return 2; return 3; }'
assert 3 'int main() { if (1-1) return 2; return 3; }'
assert 2 'int main() { if (1) return 2; return 3; }'
assert 2 'int main() { if (2-1) return 2; return 3; }'
assert 4 'int main() { if (0) { 1; 2; return 3; } else { return 4; } }'
assert 3 'int main() { if (1) { 1; 2; return 3; } else { return 4; } }'

assert 11 'int main() { int j = 1; int i = 0; for (i = 0; i < 5; i = i + 1) { j = j + i; } return j; }'
assert 55 'int main() { int i=0; int j=0; for (i=0; i<=10; i=i+1) j=i+j; return j; }'
assert 3 'int main() { for (;;) {return 3;} return 5; }'
assert 10 'int main() { int i=0; while(i<10) { i=i+1; } return i; }'

assert 3 'int main() { int x=3; return *&x; }'
assert 3 'int main() { int x=3; int* y=&x; int** z=&y; return **z; }'
assert 5 'int main() { int x=3; int y=5; return *(&x-1); }'
assert 3 'int main() { int x=3; int y=5; return *(&y+1); }'
assert 5 'int main() { int x=3; int* y=&x; *y=5; return x; }'
assert 7 'int main() { int x=3; int y=5; *(&x-1)=7; return y; }'
assert 7 'int main() { int x=3; int y=5; *(&y+2-1)=7; return x; }'
assert 5 'int main() { int x=3; return (&x+2)-&x+3; }'

assert 3 'int main() { return ret3(); }'
assert 5 'int main() { return ret5(); }'
assert 8 'int main() { return add(3, 5); }'
assert 2 'int main() { return sub(5, 3); }'
assert 21 'int main() { return add6(1,2,3,4,5,6); }'
assert 66 'int main() { return add6(1,2,add6(3,4,5,6,7,8),9,10,11); }'
assert 136 'int main() { return add6(1,2,add6(3,add6(4,5,6,7,8,9),10,11,12,13),14,15,16); }'

assert 0 'int main() { return 0; }'
assert 42 'int main() { return 42; }'
assert 21 'int main() { return 5+20-4; }'
assert 41 'int main() { return  12 + 34 - 5 ; }'
assert 47 'int main() { return 5+6*7; }'
assert 15 'int main() { return 5*(9-6); }'
assert 4 'int main() { return (3+5)/2; }'
assert 10 'int main() { return -10+20; }'
assert 10 'int main() { return - -10; }'
assert 10 'int main() { return - - +10; }'

assert 0 'int main() { return 0==1; }'
assert 1 'int main() { return 42==42; }'
assert 1 'int main() { return 0!=1; }'
assert 0 'int main() { return 42!=42; }'

assert 1 'int main() { return 0<1; }'
assert 0 'int main() { return 1<1; }'
assert 0 'int main() { return 2<1; }'
assert 1 'int main() { return 0<=1; }'
assert 1 'int main() { return 1<=1; }'
assert 0 'int main() { return 2<=1; }'

assert 1 'int main() { return 1>0; }'
assert 0 'int main() { return 1>1; }'
assert 0 'int main() { return 1>2; }'
assert 1 'int main() { return 1>=0; }'
assert 1 'int main() { return 1>=1; }'
assert 0 'int main() { return 1>=2; }'

assert 3 'int main() { int a; a=3; return a; }'
assert 3 'int main() { int a=3; return a; }'
assert 8 'int main() { int a=3; int z=5; return a+z; }'

assert 1 'int main() { return 1; 2; 3; }'
assert 2 'int main() { 1; return 2; 3; }'
assert 3 'int main() { 1; 2; return 3; }'

assert 3 'int main() { int a=3; return a; }'
assert 8 'int main() { int a=3; int z=5; return a+z; }'
assert 6 'int main() { int a; int b; a=b=3; return a+b; }'
assert 3 'int main() { int foo=3; return foo; }'
assert 8 'int main() { int foo123=3; int bar=5; return foo123+bar; }'

assert 3 'int main() { if (0) return 2; return 3; }'
assert 3 'int main() { if (1-1) return 2; return 3; }'
assert 2 'int main() { if (1) return 2; return 3; }'
assert 2 'int main() { if (2-1) return 2; return 3; }'

assert 55 'int main() { int i=0; int j=0; for (i=0; i<=10; i=i+1) j=i+j; return j; }'
assert 3 'int main() { for (;;) return 3; return 5; }'

assert 10 'int main() { int i=0; while(i<10) i=i+1; return i; }'

assert 3 'int main() { {1; {2;} return 3;} }'
assert 5 'int main() { ;;; return 5; }'

assert 10 'int main() { int i=0; while(i<10) i=i+1; return i; }'
assert 55 'int main() { int i=0; int j=0; while(i<=10) {j=i+j; i=i+1;} return j; }'

assert 3 'int main() { int x=3; return *&x; }'
assert 3 'int main() { int x=3; int *y=&x; int **z=&y; return **z; }'
assert 5 'int main() { int x=3; int y=5; return *(&x-1); }'
assert 3 'int main() { int x=3; int y=5; return *(&y+1); }'
assert 3 'int main() { int x=3; int y=5; return *(&y-(-1)); }'
assert 5 'int main() { int x=3; int *y=&x; *y=5; return x; }'
assert 7 'int main() { int x=3; int y=5; *(&x-1)=7; return y; }'
assert 7 'int main() { int x=3; int y=5; *(&y+2-1)=7; return x; }'
# assert 5 'int main() { int x=3; return (&x-2)-&x-3; }'
assert 8 'int main() { int x, y; x=3; y=5; return x+y; }'
assert 8 'int main() { int x=3, y=5; return x+y; }'

assert 3 'int main() { return ret3(); }'
assert 5 'int main() { return ret5(); }'
assert 8 'int main() { return add(3, 5); }'
assert 2 'int main() { return sub(5, 3); }'
assert 21 'int main() { return add6(1,2,3,4,5,6); }'
assert 66 'int main() { return add6(1,2,add6(3,4,5,6,7,8),9,10,11); }'
assert 136 'int main() { return add6(1,2,add6(3,add6(4,5,6,7,8,9),10,11,12,13),14,15,16); }'

assert 32 'int main() { return ret32(); } int ret32() { return 32; }'
assert 7 'int main() { return add2(3,4); } int add2(int x, int y) { return x+y; }'
assert 1 'int main() { return sub2(4,3); } int sub2(int x, int y) { return x-y; }'
assert 55 'int main() { return fib(9); } int fib(int x) { if (x<=1) return 1; return fib(x-1) + fib(x-2); }'

assert 3 'int main() { int x[2]; int *y=&x; *y=3; return *x; }'

assert 3 'int main() { int x[3]; *x=3; *(x+1)=4; *(x+2)=5; return *x; }'
assert 4 'int main() { int x[3]; *x=3; *(x+1)=4; *(x+2)=5; return *(x+1); }'
assert 5 'int main() { int x[3]; *x=3; *(x+1)=4; *(x+2)=5; return *(x+2); }'

assert 0 'int main() { int x[2][3]; int *y=x; *y=0; return **x; }'
assert 1 'int main() { int x[2][3]; int *y=x; *(y+1)=1; return *(*x+1); }'
assert 2 'int main() { int x[2][3]; int *y=x; *(y+2)=2; return *(*x+2); }'
assert 3 'int main() { int x[2][3]; int *y=x; *(y+3)=3; return **(x+1); }'
assert 4 'int main() { int x[2][3]; int *y=x; *(y+4)=4; return *(*(x+1)+1); }'
assert 5 'int main() { int x[2][3]; int *y=x; *(y+5)=5; return *(*(x+1)+2); }'

assert 0 'int main() { int x[2][3]; int *y=x; y[0]=0; return x[0][0]; }'
assert 1 'int main() { int x[2][3]; int *y=x; y[1]=1; return x[0][1]; }'
assert 2 'int main() { int x[2][3]; int *y=x; y[2]=2; return x[0][2]; }'
assert 3 'int main() { int x[2][3]; int *y=x; y[3]=3; return x[1][0]; }'
assert 4 'int main() { int x[2][3]; int *y=x; y[4]=4; return x[1][1]; }'
assert 5 'int main() { int x[2][3]; int *y=x; y[5]=5; return x[1][2]; }'

echo OK

rm tmp2.o
