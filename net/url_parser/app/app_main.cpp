#include "url_parser.h"

#include <iostream>
#include <vector>
using namespace std;

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cout << "Usage: url_parser http://www.baidu.com/" << endl;
        return 0;
    }

    URLParser::HTTP_URL url = URLParser::Parse(argv[1]);
    cout << "scheme: " + url.scheme << endl;
    cout << "host name: " + url.host << endl;
    cout << "port: " + url.port << endl;
    for (string path : url.path)
    {
        std::cout << "path:[" << path << "]" << std::endl;
    }

    cout << "param: " + url.query_string << endl;
    for (std::pair<std::string, std::string> pair : url.query)
    {
        std::cout << "Query:[" << pair.first << "]:[" << pair.second << "]" << std::endl;
    }

}
