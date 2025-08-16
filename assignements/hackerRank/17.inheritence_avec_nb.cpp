#include <cmath>
#include <cstdio>
#include <vector>
#include <iostream>
#include <algorithm>
#include <numeric>
using namespace std;

class Person{
protected:
    int age;
    string name;
public:
    Person() = default;
    Person(string n, int a) : age(a), name(n) {}
    
    virtual void getdata() {
        cin >> name >> age;
    }
    
    virtual void putdata() {
        cout << name << " " << age << endl;
    }
    
    virtual ~Person() {}
};

class Professor : public Person{
private:
    int publications;
    int cur_id;
    static int id_counter;
    
public:
    Professor() : Person(), publications{0}, cur_id{++id_counter} {}
    
    void getdata() override {
        cin >> name >> age >> publications;
    }
    
    void putdata() override {
        cout << name << " " << age << " " << publications << " " << cur_id << endl;
    }
};

class Student : public Person{
private:
    vector<int> marks;  // CORRECTION: déclaration correcte du vector
    int cur_id;
    static int id_counter;
    
public:
    Student() : Person(), marks(6), cur_id{++id_counter} {}  // CORRECTION: initialise vector avec 6 éléments
    
    void getdata() override {
        cin >> name >> age;
        for(int i = 0; i < 6; i++){
            cin >> marks[i];  // CORRECTION: maintenant marks[i] est valide
        }
    }
    
    void putdata() override {
        int total = accumulate(marks.begin(), marks.end(), 0);
        cout << name << " " << age << " " << total << " " << cur_id << endl;
    }
};

// CORRECTION: Définition obligatoire des variables statiques
int Professor::id_counter = 0;
int Student::id_counter = 0;

int main(){
    int n, val;
    cin >> n;
    Person *per[n];
    
    for(int i = 0; i < n; i++){
        cin >> val;
        if(val == 1){
            per[i] = new Professor;
        }
        else {
            per[i] = new Student;
        }
        per[i]->getdata();
    }
    
    for(int i = 0; i < n; i++){
        per[i]->putdata();
    }
    
    // Libération mémoire (optionnel pour HackerRank)
    for(int i = 0; i < n; i++){
        delete per[i];
    }
    
    return 0;
}