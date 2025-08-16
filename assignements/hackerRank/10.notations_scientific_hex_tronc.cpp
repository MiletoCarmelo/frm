#include <iostream>
#include <iomanip>
using namespace std;

int main() {
    int k;
    cin >> k;
    
    for(int i = 0; i < k; i++) {
        double x, y, z;
        cin >> x >> y >> z;
        
        // 1. x: Tronquer et afficher en hexa lowercase
        cout << "0x" << hex << (int)x << endl;
    
        cout << int(x) << endl; // Afficher la valeur entière de x
        
        // 2. y: 2 décimales, signe, justifié à droite, padding avec '_', largeur 15
        cout << dec << fixed << setprecision(2) << showpos 
             << setfill('_') << setw(15) << right << y << endl;
        
        // 3. z: 9 décimales, notation scientifique, uppercase
        cout << scientific << uppercase << setprecision(9) << noshowpos << z << endl;
    }
    
    return 0;
}