#include <cmath>
#include <cstdio>
#include <vector>
#include <iostream>
#include <algorithm>
using namespace std;

void update(int *a, int *b) {
    int temp_a = *a;  // Sauvegarder la valeur originale de a
    int temp_b = *b;  // Sauvegarder la valeur originale de b
    
    *a = temp_a + temp_b;        // a devient la somme
    *b = abs(temp_a - temp_b);   // b devient la différence absolue
}

// attention pas le droit de faire en deux functions car la première aurait dejé modifié la valeur de a:
// increase(int *a, int *b){*v = *a + *b;}
// decrease(int *a, int *b){*v = abs(*a - *b);}
// => sa ne marcherait pas car a aurait été modifié avant la deuxième fonction
// donc on doit faire les deux dans la même fonction update

int main() {
    int a, b;
    cin >> a >> b;
    update(&a, &b);
    cout << a << endl;
    cout << b << endl;
    return 0;
}