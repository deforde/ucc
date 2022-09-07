#!/usr/bin/env bash
set -u
IFS=$'\n\t'

DIR=${0%/*}

assert() {
  expected=$1
  input="$2"

  echo "$input" > tmp.c
  $DIR/../build/ucc -o tmp.s tmp.c
  cc -o tmp tmp.s
  ./tmp
  actual=$?

  if [[ $actual == $expected ]]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi

  rm tmp.c tmp.s tmp
}

# arithmetic
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

# variables
assert 3 'int main() { return ({ int a; a=3; a; }); }'
assert 3 'int main() { return ({ int a=3; a; }); }'
assert 8 'int main() { return ({ int a=3; int z=5; a+z; }); }'
assert 3 'int main() { return ({ int a=3; a; }); }'
assert 8 'int main() { return ({ int a=3; int z=5; a+z; }); }'
assert 6 'int main() { return ({ int a; int b; a=b=3; a+b; }); }'
assert 3 'int main() { return ({ int foo=3; foo; }); }'
assert 8 'int main() { return ({ int foo123=3; int bar=5; foo123+bar; }); }'
assert 8 'int main() { return ({ int x; sizeof(x); }); }'
assert 8 'int main() { return ({ int x; sizeof x; }); }'
assert 8 'int main() { return ({ int *x; sizeof(x); }); }'
assert 32 'int main() { return ({ int x[4]; sizeof(x); }); }'
assert 96 'int main() { return ({ int x[3][4]; sizeof(x); }); }'
assert 32 'int main() { return ({ int x[3][4]; sizeof(*x); }); }'
assert 8 'int main() { return ({ int x[3][4]; sizeof(**x); }); }'
assert 9 'int main() { return ({ int x[3][4]; sizeof(**x) + 1; }); }'
assert 9 'int main() { return ({ int x[3][4]; sizeof **x + 1; }); }'
assert 8 'int main() { return ({ int x[3][4]; sizeof(**x + 1); }); }'
assert 8 'int main() { return ({ int x=1; sizeof(x=2); }); }'
assert 1 'int main() { return ({ int x=1; sizeof(x=2); x; }); }'
assert 0 'int g1, g2[4]; int main() { return g1; }'
assert 3 'int g1, g2[4]; int main() { return ({ g1=3; g1; }); }'
assert 0 'int g1, g2[4]; int main() { return ({ g2[0]=0; g2[1]=1; g2[2]=2; g2[3]=3; g2[0]; }); }'
assert 1 'int g1, g2[4]; int main() { return ({ g2[0]=0; g2[1]=1; g2[2]=2; g2[3]=3; g2[1]; }); }'
assert 2 'int g1, g2[4]; int main() { return ({ g2[0]=0; g2[1]=1; g2[2]=2; g2[3]=3; g2[2]; }); }'
assert 3 'int g1, g2[4]; int main() { return ({ g2[0]=0; g2[1]=1; g2[2]=2; g2[3]=3; g2[3]; }); }'
assert 8 'int g1, g2[4]; int main() { return sizeof(g1); }'
assert 32 'int g1, g2[4]; int main() { return sizeof(g2); }'
assert 1 'int main() { return ({ char x=1; x; }); }'
assert 1 'int main() { return ({ char x=1; char y=2; x; }); }'
assert 2 'int main() { return ({ char x=1; char y=2; y; }); }'
assert 1 'int main() { return ({ char x; sizeof(x); }); }'
assert 10 'int main() { return ({ char x[10]; sizeof(x); }); }'
assert 2 'int main() { return ({ int x=2; { int x=3; } x; }); }'
assert 2 'int main() { return ({ int x=2; { int x=3; } int y=4; x; }); }'
assert 3 'int main() { return ({ int x=2; { x=3; } x; }); }'

# control
assert 3 'int main() { return ({ int x; if (0) x=2; else x=3; x; }); }'
assert 3 'int main() { return ({ int x; if (1-1) x=2; else x=3; x; }); }'
assert 2 'int main() { return ({ int x; if (1) x=2; else x=3; x; }); }'
assert 2 'int main() { return ({ int x; if (2-1) x=2; else x=3; x; }); }'
assert 55 'int main() { return ({ int i=0; int j=0; for (i=0; i<=10; i=i+1) j=i+j; j; }); }'
assert 10 'int main() { return ({ int i=0; while(i<10) i=i+1; i; }); }'
assert 3 'int main() { return ({ 1; {2;} 3; }); }'
assert 5 'int main() { return ({ ;;; 5; }); }'
assert 10 'int main() { return ({ int i=0; while(i<10) i=i+1; i; }); }'
assert 55 'int main() { return ({ int i=0; int j=0; while(i<=10) {j=i+j; i=i+1;} j; }); }'

# pointers
assert 3 'int main() { return ({ int x=3; *&x; }); }'
assert 3 'int main() { return ({ int x=3; int *y=&x; int **z=&y; **z; }); }'
assert 5 'int main() { return ({ int x=3; int y=5; *(&x-1); }); }'
assert 3 'int main() { return ({ int x=3; int y=5; *(&y+1); }); }'
assert 5 'int main() { return ({ int x=3; int y=5; *(&x+(-1)); }); }'
assert 5 'int main() { return ({ int x=3; int *y=&x; *y=5; x; }); }'
assert 7 'int main() { return ({ int x=3; int y=5; *(&x-1)=7; y; }); }'
assert 7 'int main() { return ({ int x=3; int y=5; *(&y+2-1)=7; x; }); }'
assert 5 'int main() { return ({ int x=3; (&x+2)-&x+3; }); }'
assert 8 'int main() { return ({ int x, y; x=3; y=5; x+y; }); }'
assert 8 'int main() { return ({ int x=3, y=5; x+y; }); }'
assert 3 'int main() { return ({ int x[2]; int *y=&x; *y=3; *x; }); }'
assert 3 'int main() { return ({ int x[3]; *x=3; *(x+1)=4; *(x+2)=5; *x; }); }'
assert 4 'int main() { return ({ int x[3]; *x=3; *(x+1)=4; *(x+2)=5; *(x+1); }); }'
assert 5 'int main() { return ({ int x[3]; *x=3; *(x+1)=4; *(x+2)=5; *(x+2); }); }'
assert 0 'int main() { return ({ int x[2][3]; int *y=x; *y=0; **x; }); }'
assert 1 'int main() { return ({ int x[2][3]; int *y=x; *(y+1)=1; *(*x+1); }); }'
assert 2 'int main() { return ({ int x[2][3]; int *y=x; *(y+2)=2; *(*x+2); }); }'
assert 3 'int main() { return ({ int x[2][3]; int *y=x; *(y+3)=3; **(x+1); }); }'
assert 4 'int main() { return ({ int x[2][3]; int *y=x; *(y+4)=4; *(*(x+1)+1); }); }'
assert 5 'int main() { return ({ int x[2][3]; int *y=x; *(y+5)=5; *(*(x+1)+2); }); }'
assert 3 'int main() { return ({ int x[3]; *x=3; x[1]=4; x[2]=5; *x; }); }'
assert 4 'int main() { return ({ int x[3]; *x=3; x[1]=4; x[2]=5; *(x+1); }); }'
assert 5 'int main() { return ({ int x[3]; *x=3; x[1]=4; x[2]=5; *(x+2); }); }'
assert 5 'int main() { return ({ int x[3]; *x=3; x[1]=4; x[2]=5; *(x+2); }); }'
assert 5 'int main() { return ({ int x[3]; *x=3; x[1]=4; 2[x]=5; *(x+2); }); }'
assert 0 'int main() { return ({ int x[2][3]; int *y=x; y[0]=0; x[0][0]; }); }'
assert 1 'int main() { return ({ int x[2][3]; int *y=x; y[1]=1; x[0][1]; }); }'
assert 2 'int main() { return ({ int x[2][3]; int *y=x; y[2]=2; x[0][2]; }); }'
assert 3 'int main() { return ({ int x[2][3]; int *y=x; y[3]=3; x[1][0]; }); }'
assert 4 'int main() { return ({ int x[2][3]; int *y=x; y[4]=4; x[1][1]; }); }'
assert 5 'int main() { return ({ int x[2][3]; int *y=x; y[5]=5; x[1][2]; }); }'

# functions
assert 3 'int ret3() {  return 3;  return 5;}int add2(int x, int y) {  return x + y;}int sub2(int x, int y) {  return x - y;}int add6(int a, int b, int c, int d, int e, int f) {  return a + b + c + d + e + f;}int addx(int *x, int y) {  return *x + y;}int sub_char(char a, char b, char c) {  return a - b - c;}int fib(int x) {  if (x<=1)    return 1;  return fib(x-1) + fib(x-2);} int main() { return ret3(); }'
assert 8 'int ret3() {  return 3;  return 5;}int add2(int x, int y) {  return x + y;}int sub2(int x, int y) {  return x - y;}int add6(int a, int b, int c, int d, int e, int f) {  return a + b + c + d + e + f;}int addx(int *x, int y) {  return *x + y;}int sub_char(char a, char b, char c) {  return a - b - c;}int fib(int x) {  if (x<=1)    return 1;  return fib(x-1) + fib(x-2);} int main() { return add2(3, 5); }'
assert 2 'int ret3() {  return 3;  return 5;}int add2(int x, int y) {  return x + y;}int sub2(int x, int y) {  return x - y;}int add6(int a, int b, int c, int d, int e, int f) {  return a + b + c + d + e + f;}int addx(int *x, int y) {  return *x + y;}int sub_char(char a, char b, char c) {  return a - b - c;}int fib(int x) {  if (x<=1)    return 1;  return fib(x-1) + fib(x-2);} int main() { return sub2(5, 3); }'
assert 21 'int ret3() {  return 3;  return 5;}int add2(int x, int y) {  return x + y;}int sub2(int x, int y) {  return x - y;}int add6(int a, int b, int c, int d, int e, int f) {  return a + b + c + d + e + f;}int addx(int *x, int y) {  return *x + y;}int sub_char(char a, char b, char c) {  return a - b - c;}int fib(int x) {  if (x<=1)    return 1;  return fib(x-1) + fib(x-2);} int main() { return add6(1,2,3,4,5,6); }'
assert 66 'int ret3() {  return 3;  return 5;}int add2(int x, int y) {  return x + y;}int sub2(int x, int y) {  return x - y;}int add6(int a, int b, int c, int d, int e, int f) {  return a + b + c + d + e + f;}int addx(int *x, int y) {  return *x + y;}int sub_char(char a, char b, char c) {  return a - b - c;}int fib(int x) {  if (x<=1)    return 1;  return fib(x-1) + fib(x-2);} int main() { return add6(1,2,add6(3,4,5,6,7,8),9,10,11); }'
assert 136 'int ret3() {  return 3;  return 5;}int add2(int x, int y) {  return x + y;}int sub2(int x, int y) {  return x - y;}int add6(int a, int b, int c, int d, int e, int f) {  return a + b + c + d + e + f;}int addx(int *x, int y) {  return *x + y;}int sub_char(char a, char b, char c) {  return a - b - c;}int fib(int x) {  if (x<=1)    return 1;  return fib(x-1) + fib(x-2);} int main() { return add6(1,2,add6(3,add6(4,5,6,7,8,9),10,11,12,13),14,15,16); }'
assert 7 'int ret3() {  return 3;  return 5;}int add2(int x, int y) {  return x + y;}int sub2(int x, int y) {  return x - y;}int add6(int a, int b, int c, int d, int e, int f) {  return a + b + c + d + e + f;}int addx(int *x, int y) {  return *x + y;}int sub_char(char a, char b, char c) {  return a - b - c;}int fib(int x) {  if (x<=1)    return 1;  return fib(x-1) + fib(x-2);} int main() { return add2(3,4); }'
assert 1 'int ret3() {  return 3;  return 5;}int add2(int x, int y) {  return x + y;}int sub2(int x, int y) {  return x - y;}int add6(int a, int b, int c, int d, int e, int f) {  return a + b + c + d + e + f;}int addx(int *x, int y) {  return *x + y;}int sub_char(char a, char b, char c) {  return a - b - c;}int fib(int x) {  if (x<=1)    return 1;  return fib(x-1) + fib(x-2);} int main() { return sub2(4,3); }'
assert 55 'int ret3() {  return 3;  return 5;}int add2(int x, int y) {  return x + y;}int sub2(int x, int y) {  return x - y;}int add6(int a, int b, int c, int d, int e, int f) {  return a + b + c + d + e + f;}int addx(int *x, int y) {  return *x + y;}int sub_char(char a, char b, char c) {  return a - b - c;}int fib(int x) {  if (x<=1)    return 1;  return fib(x-1) + fib(x-2);} int main() { return fib(9); }'
assert 1 'int ret3() {  return 3;  return 5;}int add2(int x, int y) {  return x + y;}int sub2(int x, int y) {  return x - y;}int add6(int a, int b, int c, int d, int e, int f) {  return a + b + c + d + e + f;}int addx(int *x, int y) {  return *x + y;}int sub_char(char a, char b, char c) {  return a - b - c;}int fib(int x) {  if (x<=1)    return 1;  return fib(x-1) + fib(x-2);} int main() { return ({ sub_char(7, 3, 3); }); }'

# strings
assert 0 'int main() { return ""[0]; }'
assert 1 'int main() { return sizeof(""); }'
assert 97 'int main() { return "abc"[0]; }'
assert 98 'int main() { return "abc"[1]; }'
assert 99 'int main() { return "abc"[2]; }'
assert 0 'int main() { return "abc"[3]; }'
assert 4 'int main() { return sizeof("abc"); }'
assert 7 'int main() { return "\a"[0]; }'
assert 8 'int main() { return "\b"[0]; }'
assert 9 'int main() { return "\t"[0]; }'
assert 10 'int main() { return "\n"[0]; }'
assert 11 'int main() { return "\v"[0]; }'
assert 12 'int main() { return "\f"[0]; }'
assert 13 'int main() { return "\r"[0]; }'
assert 27 'int main() { return "\e"[0]; }'
assert 106 'int main() { return "\j"[0]; }'
assert 107 'int main() { return "\k"[0]; }'
assert 108 'int main() { return "\l"[0]; }'
assert 7 'int main() { return "\ax\ny"[0]; }'
assert 120 'int main() { return "\ax\ny"[1]; }'
assert 10 'int main() { return "\ax\ny"[2]; }'
assert 121 'int main() { return "\ax\ny"[3]; }'
assert 0 'int main() { return "\0"[0]; }'
assert 16 'int main() { return "\20"[0]; }'
assert 65 'int main() { return "\101"[0]; }'
assert 104 'int main() { return "\1500"[0]; }'
assert 0 'int main() { return "\x00"[0]; }'
assert 119 'int main() { return "\x77"[0]; }'

echo OK
