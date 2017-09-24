#ifndef CANDIDATES_FILE_H
#define CANDIDATES_FILE_H

#include "csv_file.h"
#include "math256.h"

class candidates_file
{
public:
    candidates_file();
    bool is_valid();
    void append(const uint256_t &start, const uint256_t &record, uint64_t bits, uint64_t restclass);

public:
    uint256_t last_start;
    uint256_t last_record;
    uint64_t last_record_bits = 0;
    uint64_t last_rest_class = 0;
private:
    csv_file file;
    bool valid = false;
};

#endif // CANDIDATES_FILE_H
