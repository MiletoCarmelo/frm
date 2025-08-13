#include <cmath>
#include <cstdio>
#include <vector>
#include <iostream>
#include <algorithm>
using namespace std;

vector<int> split(string text) {
    vector<string> numbers;
    auto start = text.begin();
    auto stop = text.end();
    
    while (start != stop) {
        auto found = find(start, stop, ' ');
        if (start != found) {
            numbers.emplace_back(start, found);
        }
        if (found != stop) {
            start = found + 1;
        } else {
            break;
        }
    }
    
    vector<int> result;
    for (const auto& num : numbers) {
        result.push_back(stoi(num));
    }
    return result;
}

int main() {
    vector<string> arr;
    string line;
    while(getline(cin, line)){
        arr.push_back(line);
    }
    vector<int> structure = split(arr[0]);
    vector<vector<int>> table; 
    for (int i = 0; i<structure[0]; i++){
        table.push_back(split(arr[i+1]));
    }
    vector<vector<int>> reqs;
    for(size_t i=structure[1]+1; i < arr.size(); i++){
       reqs.push_back(split(arr[i]));
    }
    
    for(vector<int> i : reqs){
        cout << table[i[0]][i[1]+1] << endl;
    }

    return 0;
}
