#include <cmath>
#include <cstdio>
#include <vector>
#include <iostream>
#include <algorithm>
using namespace std;


int main() {
    string line;
    getline(cin, line); 
    
    vector<string> numbers;
    auto start = line.begin();
    auto end = line.end();
    
    while (start != end) {
        auto found = find(start, end, ' ');
        if (start != found) { 
            numbers.emplace_back(start, found);
        }
        
        if (found != end) {
            start = found + 1;
        } else {
            break;
        }
    }
    
    int sum = 0;
    for (const auto& num : numbers) {
        sum += stoi(num);
    }
    
    cout << sum << endl;
    
    return 0;
}
