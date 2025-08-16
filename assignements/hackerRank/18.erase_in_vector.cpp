#include <cmath>
#include <cstdio>
#include <vector>
#include <iostream>
#include <algorithm>
using namespace std;

vector<string> split(string line){
    auto start = line.begin();
    auto stop = line.end();
    vector<string> v_s;
    while (start!=stop){
        auto found = find(start,stop,' ');
        if(found!=start){
            v_s.emplace_back(start, found);
        }
        if(found!=stop){
            start = found + 1;
        }
        else {
            break;
        }
    }
    return v_s;
}

/*
input:
6 => size of array
123 456 789 101112 131415 161718 => array of 6 elements
2 => query 1: remove element at index 2 in array
2 4 => query 2: remove elements from index 2 to index 4 (inclusive) in array

result:
3 => size of array after queries
123 131415 161718 => array of 3 elements

Exemple 2:
8 => size of array
1 4 6 2 8 9 => array of 8 elements
2 => query 1: remove element at index 2 in array
2 4 => query 2: remove elements from index 2 to index 4 (inclusive) in array
result:
3 => size of array after queries
1 8 9 => array of 3 elements

*/


int main() {
    int n; 
    cin >> n;
    vector<string> args(n);
    string line;
    for(int i=0; i<n; i++){
        cin >> line; 
        args[i] = line;
    }
    int r1;
    int f,t;
    cin >> r1;
    cin >> f >> t;
    args.erase(args.begin()+r1-1);
    args.erase(args.begin()+f-1,args.begin()+t-1);
    cout << args.size() << endl;
    for(auto i:args){
        cout << i << " ";
    }
    cout << endl;
    return 0;
}
