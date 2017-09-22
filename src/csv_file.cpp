#include "csv_file.h"

const char DELIMITER = ',';
const char EOL  = '\n';
const std::string EOL_STRING = std::string(1, EOL);
const std::string DELIMITER_STRING = std::string(1, DELIMITER);

/**
 * @brief csv_file::csv_file creates or opens a CSV file
 * @param filename
 */
csv_file::csv_file(const std::string &filename)
    : filename{filename}
{
    // open file as binary to avoid end of line problems between unix/windows/apple
    std::fstream* file = new std::fstream(filename.c_str(),
                         std::ios::in | std::ios::out | std::ios::binary);
    datafile = std::unique_ptr<std::fstream>(file);

    if(!datafile)
    {
        return;
    }

    open = true;

    std::streampos begin, end;
    begin = datafile->tellp();
    datafile->seekp(0, std::ios::end);
    end = datafile->tellp();

    if(end == begin)
    {
        return;
    }

    datafile->seekg(0, std::ios::beg);
    std::string line;
    std::getline(*datafile, line, EOL);

    header = parse_line(line);
    columns = header.size();
    if(columns > 0)
    {
        data = true;
    }
}

bool csv_file::is_open()
{
    return open;
}

bool csv_file::has_data()
{
    return data;
}

void csv_file::clear()
{
    open = false;
    data = false;
    datafile->close();
    datafile = std::unique_ptr<std::fstream>(
               new std::fstream(filename.c_str(),
               std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc));
    if(datafile && !datafile->is_open())
    {
        return;
    }
    //TODO: not sure if needed
    datafile->seekp(0, std::ios::beg);
    open = true;
}

uint_least32_t csv_file::get_column_count()
{
    return columns;
}

const std::list<std::string> &csv_file::get_header()
{
    return header;
}

const std::list<std::string> csv_file::get_data(unsigned int line)
{
    datafile->seekg(0, std::ios::beg);
    std::string cur_line;
    // skip all ignored lines
    for(unsigned int i = 0; i < line; i++)
    {
        // abort when reaching EOF
        if(!std::getline(*datafile, cur_line, EOL))
        {
            break;
        }
    }
    std::getline(*datafile, cur_line, EOL);
    // clear eofbit and failbit in case we reached EOF
    datafile->clear();
    return parse_line(cur_line);
}

const std::list<std::string> csv_file::get_last_data()
{
    datafile->seekg(0, std::ios::beg);
    std::string cur_line;
    std::list<std::string> last_data;

    // skip header
    std::getline(*datafile, cur_line, EOL);
    cur_line.clear();

    // skip all ignored lines
    while(std::getline(*datafile, cur_line, EOL))
    {
        std::list<std::string> cur_data = parse_line(cur_line);
        if(cur_data.size() == columns)
        {
            last_data = cur_data;
        }
        else
        {
            break;
        }
    }
    // since we read to EOF, clear failbit and eofbit
    datafile->clear();
    return last_data;
}

bool csv_file::add_header(const std::list<std::string> &header)
{
    if(header.size() == 0)
    {
        return false;
    }
    std::string header_line = write_line(header);
    datafile->write(header_line.c_str(), header_line.size());
    columns = header.size();
    datafile->flush();
    return true;
}

bool csv_file::append_data(const std::list<std::string> &data)
{
    if(data.size() != columns)
    {
        return false;
    }

    bool isgood = datafile->good();
    std::string line = write_line(data);
    datafile->seekp(0, std::ios::end);
    datafile->write(line.c_str(), line.size());
    datafile->flush();
    isgood = datafile->good();
    return true;
}

std::list<std::string> csv_file::parse_line(std::string line)
{
    std::list<std::string> result;
    size_t pos = 0;
    std::string token;
    while ((pos = line.find(DELIMITER)) != std::string::npos)
    {
        token = line.substr(0, pos);
        result.push_back(token);
        line.erase(0, pos + 1);
    }
    // remove end of line chars
    pos = line.find(EOL);
    result.push_back(line.substr(0, pos));
    return result;
}

std::string csv_file::write_line(std::list<std::string> data)
{
    std::string result = "";
    if(data.size() > 0)
    {
        for(const std::string& str: data)
        {
            result.append(str + DELIMITER);
        }
        // replace last DELIMITER with EOL char
        result.replace(result.end() - 1, result.end(), EOL_STRING);
    }
    return result;
}
