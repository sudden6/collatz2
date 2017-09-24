#include "candidates_file.h"
#include "math256.h"

#include <algorithm>
#include <list>
#include <string>
#include <vector>

const std::vector<std::string> COLUMNS = {
    "Start Number",
    "Path length",
    "Path length bits",
    "Rest class"
};

candidates_file::candidates_file()
{
    file = csv_file("candidates.csv");
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
    last_start = uint256_t::from_string(data[0]);
    last_record = uint256_t::from_string(data[1]);
    last_record_bits = std::stoull(data[2]);
    last_rest_class = std::stoull(data[3]);


    valid = true;
}

bool candidates_file::is_valid()
{
    return valid;
}

void candidates_file::append(const uint256_t& start, const uint256_t& record, uint64_t bits, uint64_t restclass)
{
    std::list<std::string> line;
    line.push_back(start.to_string());
    line.push_back(record.to_string());
    line.push_back(std::to_string(bits));
    line.push_back(std::to_string(restclass));
    file.append_data(line);
}
