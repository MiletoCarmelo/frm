#include <iostream>
#include <map>
#include <string>
using namespace std;

int main() {
    int q;
    cin >> q;
    
    map<string, int> marks;
    
    for (int i = 0; i < q; i++) {
        int operation;
        cin >> operation;
        
        if (operation == 1) {
            string name;
            int score;
            cin >> name >> score;
            auto it = marks.find(name);
            if (it != marks.end()) {
              marks[name] = marks[name] + score; 
            }  else {
                marks[name] = score;
            }
        }
        else if (operation == 2) {
            string name;
            cin >> name;
            marks.erase(name);
        }
        else if (operation == 3) {
            // Afficher score
            string name;
            cin >> name;
            
            auto it = marks.find(name);
            if (it != marks.end()) {
                cout << it->second << endl;
            } else {
                cout << "0" << endl;
            }
        }
    }
    
    return 0;
}