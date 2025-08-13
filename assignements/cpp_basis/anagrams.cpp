#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <utility>
using namespace std;

class Singleton {
private:
    Singleton() {};
    static Singleton *instance;
public:
    static Singleton* getInstance() {
        if (instance == nullptr) {
            instance = new Singleton();
        }
        return instance;
    }
    void demonstrate() {
        cout << "Singleton instance address: " << this << endl;
    }
};

// Définition de la variable statique
Singleton* Singleton::instance = nullptr;

// Fonction pour trouver les anagrammes
vector<vector<string>> findAnagrams(const vector<string>& words) {
    unordered_map<string, vector<string>> anagramMap;
    
    // Grouper les mots par leur version triée
    for (const string& word : words) {
        string sortedWord = word;
        sort(sortedWord.begin(), sortedWord.end());
        anagramMap[sortedWord].push_back(word);
    }
    
    // Collecter les groupes d'anagrammes (2 ou plus)
    vector<vector<string>> result;
    for (const auto& entry : anagramMap) {
        if (entry.second.size() >= 2) {
            result.push_back(entry.second);
        }
    }
    
    return result;
}


// Fonction pour trouver la plus proche température à zéro
double findClosestToZero(const vector<double>& temperatures) {
    if (temperatures.empty()) return 0.0;
    if (temperatures.size() == 1) return temperatures[0];
    // Initialiser avec la première température
    double closest = temperatures[0];
    for (double temp : temperatures) {
        // Vérifier si la température actuelle est plus proche de zéro
        if (abs(temp) < abs(closest)) {
            closest = temp;
        }
    }
    return closest;
}

int main() {
    /*
     * EXERCICE : ANAGRAMS
     * ====================
     */
    cout << "=== Anagrams Exercise ===\n" << endl;
    
    vector<string> words = {"CREATED", "CATERED", "REACTED", "hello", "world", "llohe", "act", "cat", "tac"};
    
    vector<vector<string>> anagramGroups = findAnagrams(words);
    
    cout << "Anagram groups found:" << endl;
    for (size_t i = 0; i < anagramGroups.size(); i++) {
        cout << "Group " << (i + 1) << ": ";
        for (size_t j = 0; j < anagramGroups[i].size(); j++) {
            cout << anagramGroups[i][j];
            if (j < anagramGroups[i].size() - 1) cout << ", ";
        }
        cout << endl;
    }

    /*
     * EXERCICE : BOOLEAN EXPRESSION
     * ==============================
     */
    cout << "\n=== Boolean Expression Exercise ===\n" << endl;
    bool b = true;
    bool c = b ? !b : b;
    cout << "boolean expression: b ? !b : b" << endl;
    cout << "b = " << b << endl;
    cout << "c = " << c << endl;

    /*
     * EXERCICE : Singleton
     * ====================
     */
    cout << "\n=== Singleton Demonstration ===\n" << endl;
    Singleton* instance1 = Singleton::getInstance();
    Singleton* instance2 = Singleton::getInstance();
    Singleton* instance3 = Singleton::getInstance();
    
    instance1->demonstrate(); // Même adresse
    instance2->demonstrate(); // Même adresse
    instance3->demonstrate(); // Même adresse
    
    // Vérification d'égalité
    if (instance1 == instance2 && instance2 == instance3) {
        cout << "All instances are the same!" << endl;
    }

    /* 
     * EXERCICE : Find the temperature 
     * =================================
     * Your company builds temperature captors for freezers. These captors records temperature periodically and put the last values in a vector. You have to develop the algorithm displaying the unique temperature that is supposed to sum up these values.
     * You know the captors are not reliable at all, so you decide to display the most expected temperature among the ones in the vector, which is the one closest to zero.
     */ 
    cout << "\n=== Closest Temperature to Zero Exercise ===\n" << endl;
    auto start = std::chrono::high_resolution_clock::now();

    vector<double> temperatures = {2.5, -1.0, 3.0, -2.5, 0.5, -0.1, 1.0, -3.0, 2.0, 0.01};
    double closestTemp = findClosestToZero(temperatures);
    cout << "Closest temperature to zero: " << closestTemp << endl;
    
    auto end = std::chrono::high_resolution_clock::now();
    auto bs_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    cout << "Duration: " << bs_duration.count() << " microseconds. " << endl;

    return 0;
}