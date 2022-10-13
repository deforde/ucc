#define ASSERT(x, y) assert(x, y, #y)

int memcmp(char *p, char *q, long n);
int printf();
int sprintf();
int strcmp(char *p, char *q);
void assert(int expected, int actual, char *code);
