#ifndef WORKTODO_FILE_H
#define WORKTODO_FILE_H

#include "csv_file.h"

class worktodo_file
{
public:
    worktodo_file();
    bool is_valid();

private:
    csv_file file;
    bool valid = false;
public:
    uint64_t rest_class_start = 0;
    uint64_t rest_class_end = 0;
};

#endif // WORKTODO_FILE_H
