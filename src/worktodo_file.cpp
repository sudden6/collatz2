#include "worktodo_file.h"

#include <algorithm>
#include <list>
#include <string>
#include <vector>

const std::vector<std::string> COLUMNS = {
    "Rest class start",
    "Rest class end"
};

worktodo_file::worktodo_file()
{
    file = csv_file("worktodo.csv");
    if(!file.has_data())
    {
        return;
    }
    std::list<std::string> header = file.get_header();

    if(!std::equal(COLUMNS.begin(), COLUMNS.end(), header.begin()))
    {
        return;
    }

    // TODO: maybe this can be done better
    std::list<std::string> list_data = file.get_last_data();
    std::vector<std::string> data{list_data.begin(), list_data.end()};
    if(data.size() != COLUMNS.size())
    {
        file.clear();
        file.add_header(std::list<std::string>(COLUMNS.begin(), COLUMNS.end()));
        return;
    }
    rest_class_start = std::stoull(data[0]);
    rest_class_end = std::stoull(data[1]);
    // we have to search at least one rest class
    if(rest_class_end <= rest_class_start)
    {
        valid = false;
        return;
    }

    valid = true;
}

bool worktodo_file::is_valid()
{
    return valid;
}
