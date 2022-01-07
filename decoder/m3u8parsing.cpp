#include "m3u8parsing.h"
#include <string>
#include <regex>
#include <QDebug>

using namespace std;

M3u8Parsing::M3u8Parsing()
{

}

M3u8Parsing::EXTM3UINFO M3u8Parsing::parse(const char* bytes, int len)
{
    vector<string> lines;

    string str(bytes, len);
    string regex_str("(.*)[\n|\r]+");
    regex pattern(regex_str,regex::icase);

    smatch result;
    string::const_iterator iter = str.begin();
    string::const_iterator iterEnd = str.end();
    while (std::regex_search(iter,iterEnd,result,pattern)) {
        lines.push_back(result[1]);
        //更新搜索起始位置
        iter = result[0].second;
    }

    EXTM3UINFO info;

    { //version
        smatch m;
        regex extinf("#EXT-X-VERSION:(\\d+)");
        if(regex_search(str, m, extinf)) {
            sscanf(m.str(1).c_str(), "%d", &(info.version));
        }
    }

    string prev;
    string line;
    for(int i=0; i<lines.size(); i++) {
        if(i>0) prev = lines[i-1];
        else prev = "";
        line = lines[i];
        if(line.size()>1&&line.at(0)!='#') {
            // this is url
            EXTFILE file;
            file.extinfo = prev;
            file.url = line;
            parseExtinfo(file);
            info.files.push_back(file);
        }
    }

    return info;
}

void M3u8Parsing::parseExtinfo(EXTFILE &file)
{
    smatch m;
    regex extinf("#EXTINF:([\\d\\.]+)");
    if(regex_search(file.extinfo, m, extinf)) {
        sscanf(m.str(1).c_str(), "%f", &(file.inf));
    }
}

void M3u8Parsing::test()
{
    char buf[20480];
    memset(buf, 0, sizeof(buf));

    FILE *fp = fopen("C:\\Users\\lilu\\Downloads\\969DB99A-535D-42C5-B2C3-6A93926435751283.m3u8", "r");
    int rlen=0;
    while(!feof(fp)) {
        rlen += fread(buf+rlen, 1, 2048, fp);
    }
    fclose(fp);

    qDebug()<<"m3u8:"<<buf;

    EXTM3UINFO info = parse(buf, rlen);

    qDebug()<<"P: version"<<info.version;
    list<EXTFILE>::iterator iter;
    for(iter = info.files.begin(); iter!=info.files.end(); iter++) {
        EXTFILE file = *iter;
        qDebug()<<"P: time:"<<QString::number(file.inf)<<"url:"<<QString::fromStdString(file.url);
    }
}
