#include <sstream>
#include <vector>
#include <iostream>
using namespace std;

vector<int> parseInts(string st) {
    stringstream ss(st);
	char ch;
    int n;
    vector<int> vect;
    while(ss){
        ss >> n >> ch; 
        vect.push_back(n);
    }
    return vect;
}

int main() {
    string str;
    cin >> str;
    vector<int> integers = parseInts(str);
    for(int i = 0; i < integers.size(); i++) {
        cout << integers[i] << "\n";
    }
    
    return 0;
}