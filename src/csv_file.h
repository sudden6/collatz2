#ifndef CSV_FILE_H
#define CSV_FILE_H

#include <string>
#include <iostream>
#include <fstream>
#include <list>
#include <memory>

class csv_file
{
public:
    csv_file() = default;
    csv_file(const std::string& filename);
    bool is_open();
    bool has_data();
    void clear();
    uint_least32_t get_column_count();
    const std::list<std::string> &get_header();
    const std::list<std::string> get_data(unsigned int line);
    const std::list<std::string> get_last_data();
    bool add_header(const std::list<std::string> &header);
    bool append_data(const std::list<std::string> &data);

private:
    std::list<std::string> parse_line(std::string line);
    std::string write_line(std::list<std::string> data);

private:
    std::string filename = "";
    std::unique_ptr<std::fstream> datafile;
    std::list<std::string> header;
    bool open = false;
    bool data = false;
    uint_least32_t columns = 0;
};

#endif // CSV_FILE_H
