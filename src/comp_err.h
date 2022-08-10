#ifndef COMP_ERR_H
#define COMP_ERR_H

void compError(const char *fmt, ...);
void compErrorToken(const char *loc, const char *fmt, ...);

#endif // COMP_ERR_H
