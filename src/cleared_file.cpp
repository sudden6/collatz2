#include "cleared_file.h"
#include <algorithm>
#include <list>
#include <string>
#include <vector>

const std::vector<std::string> COLUMNS = {
    "RestClass",
    "RestClass mod 2^32",
    "Multistep Calls",
    "Candidates"
};

cleared_file::cleared_file()
{
    file = csv_file("cleared.csv");
    if(!file.has_data())
    {
        file.clear();
        file.add_header(std::list<std::string>(COLUMNS.begin(), COLUMNS.end()));
        return;
    }
    std::list<std::string> header = file.get_header();

    if(!std::equal(COLUMNS.begin(), COLUMNS.end(), header.begin()))
    {
        file.clear();
        file.add_header(std::list<std::string>(COLUMNS.begin(), COLUMNS.end()));
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
    last_rest_class = std::stoull(data[0]);
    last_rest_class_mod = std::stoull(data[1]);
    last_multistep_calls = std::stoull(data[2]);
    last_candidate_count = std::stoull(data[3]);


    valid = true;
}

bool cleared_file::is_valid()
{
    return valid;
}

void cleared_file::append(uint64_t rescl, uint64_t rescl_mod, uint64_t ms_calls, uint64_t cand)
{
    std::list<std::string> line;
    line.push_back(std::to_string(rescl));
    line.push_back(std::to_string(rescl_mod));
    line.push_back(std::to_string(ms_calls));
    line.push_back(std::to_string(cand));
    file.append_data(line);
}
