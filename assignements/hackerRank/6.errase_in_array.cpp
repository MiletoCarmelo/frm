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
    vector<vector<int>> arr;
    string line;
    while(getline(cin,line)){
        arr.push_back(split(line));
    } 
    /*  6 => size of array
     *  1 4 6 2 8 9  => array of 6 elements
     *  2 => query 1: remove element at index 2 in array
     *  2 4 => query 2: remove elements from index 2 to index 4 (inclusive) in array
     */
    vector<int> tab = arr[1];
    
    // errase position in array:
    int q1 = arr[2][0];
    tab.erase(tab.begin() + q1);
    
    // errase position in array:
    vector<int> q2 = arr[3];
    tab.erase(tab.begin() + q2[0] - 1, tab.begin() + q2[1] - 1);
    cout << tab.size() << endl;

    // result : 3
    
    for (int i : tab){
        cout << i << " " ; 
    }
    cout << endl;

    // result : 1 8 9
    
    return 0;
}
