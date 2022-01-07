#ifndef M3U8PARSING_H
#define M3U8PARSING_H

#include <list>

class M3u8Parsing
{
public:
    struct EXTFILE
    {
        std::string url;
        float inf;
        std::string extinfo;
        EXTFILE() {
            inf = 0;
        }
    };
    struct EXTM3UINFO
    {
        int version;
        std::list<EXTFILE> files;
        EXTM3UINFO() {
            version = 0;
        }
    };
    M3u8Parsing();

    void test();

    static EXTM3UINFO parse(const char* bytes, int len);

private:
    static void parseExtinfo(EXTFILE &file);
};

#endif // M3U8PARSING_H
