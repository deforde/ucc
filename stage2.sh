#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

OUTPUT_FILE=$1
shift

printf "%s\n" \
"typedef signed char int8_t;" \
"typedef short int16_t;" \
"typedef int int32_t;" \
"typedef long int64_t;" \
"typedef unsigned long size_t;" \
"typedef long ssize_t;" \
"" \
"typedef unsigned char uint8_t;" \
"typedef unsigned short uint16_t;" \
"typedef unsigned int uint32_t;" \
"typedef unsigned long uint64_t;" \
"" \
"typedef struct FILE FILE;" \
"extern FILE *stdin;" \
"extern FILE *stdout;" \
"extern FILE *stderr;" \
"" \
"typedef struct {" \
"  int gp_offset;" \
"  int fp_offset;" \
"  void *overflow_arg_area;" \
"  void *reg_save_area;" \
"} __va_elem;" \
"" \
"struct option" \
"{" \
"  const char *name;" \
"  int has_arg;" \
"  int *flag;" \
"  int val;" \
"};" \
"" \
"typedef __va_elem va_list[1];" \
"" \
"struct stat {" \
"  char _[512];" \
"};" \
"" \
"extern char *optarg;" \
"extern int optind;" \
"" \
"void *malloc(long size);" \
"void *calloc(long nmemb, long size);" \
"void *realloc(void *buf, long size);" \
"int *__errno_location();" \
"char *strerror(int errnum);" \
"FILE *fopen(char *pathname, char *mode);" \
"FILE *open_memstream(char **ptr, size_t *sizeloc);" \
"long fread(void *ptr, long size, long nmemb, FILE *stream);" \
"size_t fwrite(void *ptr, size_t size, size_t nmemb, FILE *stream);" \
"int fflush(FILE *stream);" \
"int fclose(FILE *fp);" \
"int fputc(int c, FILE *stream);" \
"int feof(FILE *stream);" \
"static void assert() {}" \
"int strcmp(char *s1, char *s2);" \
"int strncasecmp(char *s1, char *s2, long n);" \
"int memcmp(char *s1, char *s2, long n);" \
"int printf(char *fmt, ...);" \
"int puts(char *str);" \
"int sprintf(char *buf, char *fmt, ...);" \
"int fprintf(FILE *fp, char *fmt, ...);" \
"int vfprintf(FILE *fp, char *fmt, va_list ap);" \
"long strlen(char *p);" \
"int strncmp(char *p, char *q, long n);" \
"void *memcpy(char *dst, char *src, long n);" \
"char *strdup(char *p);" \
"char *strndup(char *p, long n);" \
"int isspace(int c);" \
"int ispunct(int c);" \
"int isdigit(int c);" \
"int isxdigit(int c);" \
"char *strstr(char *haystack, char *needle);" \
"char *strchr(char *s, int c);" \
"double strtod(char *nptr, char **endptr);" \
"static void va_end(va_list ap) {}" \
"long strtoul(char *nptr, char **endptr, int base);" \
"int getopt_long (int argc, char* argv[], const char * opts, struct option *lopts, );" \
"char *strncpy (char *dst, const char *src, size_t n);" \
"char *strrchr (const char *__s, int __c);" \
"char *strncat (char *__restrict __dest, const char *__restrict __src, size_t __n);" \
"int mkstemp (char *__template);" \
"int close (int __fd);" \
"int fork (void);" \
"int execvp (const char *__file, char *const __argv[]);" \
"int wait (int *__stat_loc);" \
"void exit(int code);" > $OUTPUT_FILE

for f in $@; do
  LCL_INC=$(sed -n 's/^#include "\(.*\)"$/\1/p' $f)
  for inc in $LCL_INC; do
    cat $(dirname $OUTPUT_FILE)/$inc >> $OUTPUT_FILE
  done
  cat $f >> $OUTPUT_FILE
done

sed -i 's/PATH_MAX/4096/g' $OUTPUT_FILE
sed -i 's/EXIT_SUCCESS/0/g' $OUTPUT_FILE
sed -i 's/EXIT_FAILURE/1/g' $OUTPUT_FILE
sed -i 's/no_argument/0/g' $OUTPUT_FILE
sed -i 's/required_argument/1/g' $OUTPUT_FILE
sed -i '/^#define COMP_ERR_BODY/! s/^\s*#.*//g' $OUTPUT_FILE
sed -i 's/"\n\s*"//g' $OUTPUT_FILE
sed -i 's/\bbool\b/_Bool/g' $OUTPUT_FILE
sed -i 's/\berrno\b/*__errno_location()/g' $OUTPUT_FILE
sed -i 's/\btrue\b/1/g' $OUTPUT_FILE
sed -i 's/\bfalse\b/0/g' $OUTPUT_FILE
sed -i 's/\bNULL\b/0/g' $OUTPUT_FILE
sed -i 's/\bva_start(\([^)]*\),\([^)]*\))/*(\1)=*(__va_elem*)__va_area__/g' $OUTPUT_FILE
sed -i 's/\bunreachable\(\)/error("unreachable")/g' $OUTPUT_FILE
sed -i 's/\bMIN\(([^)]*),([^)]*)\)/((\\1)<(\\2)?(\\1):(\\2))/g' $OUTPUT_FILE
