#ifndef STRFUNCS_H_
#define STRFUNCS_H_

#include <algorithm>
#include <vector>
#include <sstream>

using namespace std;

inline vector<string> &split(const string &s, char delim, vector<string> &elems){
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

inline vector<string> split(const string &s, char delim){
    vector<string> elems;
    split(s, delim, elems);
    return elems;
}

#endif //STRFUNCS_H_
