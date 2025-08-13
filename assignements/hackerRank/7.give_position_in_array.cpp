#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int main() {
    int n;
    cin >> n;
    
    vector<int> arr(n);
    for (int i = 0; i < n; i++) {
        cin >> arr[i];
    }

    // 8 => size of array
    // 1 1 2 2 6 9 9 15 => array of 8 elements
    // 1 => query 1: find element 1 in array
    // 4 => query 2: find element 4 in array
    // 9 => query 3: find element 9 in array
    // 1 => query 4: find element 1 in array
    // 15 => query 5: find element 15 in array
    
    int q;
    cin >> q;
    
    for (int i = 0; i < q; i++) {
        int target;
        cin >> target;
        
        if (binary_search(arr.begin(), arr.end(), target)) {
            auto it = lower_bound(arr.begin(), arr.end(), target);
            cout << "Yes " << (it - arr.begin() + 1) << endl;
        } else {
            auto it = lower_bound(arr.begin(), arr.end(), target);
            cout << "No " << (it - arr.begin() + 1) << endl;
        }
    }

    // Yes 1
    // No 5
    // Yes 6
    // Yes 8
    
    return 0;
}