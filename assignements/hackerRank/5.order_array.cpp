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
    /* Enter your code here. Read input from STDIN. Print output to STDOUT */   
    vector<vector<int>> arr;
    string line;
    while(getline(cin, line)){
        arr.push_back(split(line));
    }
    int n = arr[0][0];
    vector<int> vec = arr[1];
    sort(vec.begin(),vec.end());
    for (int i : vec){
        cout << i << " "; 
    }
    cout << endl;
}
