#include <stdlib.h>

#include "pz_file.h"

pz_fp pz_wrap_fp(FILE *fp,
                 unsigned long force_len) {
    pz_fp rfp;
    rfp.fp = fp;

    // check file size or set forced length
    if (force_len == 0) {
        fseek(fp, 0, SEEK_END);
        rfp.len = ftell(fp);
        fseek(fp, 0, SEEK_SET);
    }

    rfp.readed = 0;

    return rfp;
}
