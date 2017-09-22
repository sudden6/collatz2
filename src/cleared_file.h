#ifndef CLEARED_FILE_H
#define CLEARED_FILE_H

#include "csv_file.h"

class cleared_file
{
public:
    cleared_file();
    bool is_valid();
    void append(uint64_t rescl, uint64_t rescl_mod, uint64_t ms_calls, uint64_t cand);

public:
    uint64_t last_rest_class;
    uint64_t last_rest_class_mod;
    uint64_t last_multistep_calls;
    uint64_t last_candidate_count;
private:
    csv_file file;
    bool valid = false;
};

#endif // CLEARED_FILE_H
