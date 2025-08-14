#include <cmath>
#include <cstdio>
#include <vector>
#include <iostream>
#include <algorithm>
#include <set>
using namespace std;


vector<int> split(string line){
    auto start = line.begin();
    auto stop = line.end();
    vector<string> s_v;
    while(start!=stop){
        auto found = find(start, stop, ' ');
        if (found!=start){
            s_v.emplace_back(start, found);
        }
        if (found!=stop){
            start = found + 1;
        } else {
            break;
        }
    }
    vector<int> i_v;
    for(const auto& i : s_v){
        i_v.push_back(stoi(i));
    }
    return i_v;
}
int main() {
    string line;
    vector<vector<int>> arr;
    while(getline(cin,line)){
        arr.push_back(split(line));
    } 

    /*
     *   8 => size of array
     *   1 9 => add element 9 in set
     *   1 6 => add element 6 in set
     *   1 10 => add element 10 in set
     *   1 4 => add element 4 in set
     *   3 6 => check if element 6 is in set
     *   3 14 => check if element 14 is in set
     *   2 6 => remove element 6 from set
     *   3 6 => check if element 6 is in set
     */
    set<int> s;
    for(size_t i = 1; i<arr.size(); i++){
        if(arr[i][0]==1){
            s.insert(arr[i][1]);
        }
        else if(arr[i][0] == 2) {
            s.erase(arr[i][1]);  
        }
        else if(arr[i][0] == 3) {
            if (s.find(arr[i][1]) != s.end()){
                cout << "Yes" << endl;
            }
            else {
                cout << "No" << endl;
            }
        }
    }

    /*
     * OUTPUT :
     * Yes
     * No
     * No
     */
    
    return 0;
}
