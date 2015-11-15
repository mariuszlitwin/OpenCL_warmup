#ifndef PZ_FILE_H
#define PZ_FILE_H

#include <stdio.h>

typedef struct {
    FILE *fp;                                                                   // C file pointer to randomness source
    unsigned long len;                                                          // randomness source length
    size_t readed;
} pz_fp;

extern pz_fp pz_wrap_fp(FILE *fp,
                        unsigned long force_len);

#endif /* MLCL_H */
