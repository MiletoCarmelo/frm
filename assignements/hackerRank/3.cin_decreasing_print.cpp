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
    int n; 
    cin >> n;
    cin.ignore(); 
    string line;
    getline(cin, line); 

    auto vec_num = split(line);
    
    for (int i=0; i<=n; i++){
        cout << vec_num[n-i] ;
    } 
    cout << endl;
    return 0;
}
