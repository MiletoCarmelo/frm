#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <map>

using namespace std;

struct Node{
    int key;
    int value;
    Node* next;
    Node* prev;
    Node(int k, int v, Node* n, Node* p) : key(k), value(v), next(n), prev(p) {};
    Node(int k, int v) : key(k), value(v), next(nullptr), prev(nullptr) {};
};

struct Cache{
    protected:
        map<int,Node*> mp;
        size_t cp;
        Node* head;
        Node* tail;
        virtual int get(int) = 0;
        virtual void set(int,int) = 0;
};

//                         AddToHead                  RemoveNode
//                             |     ____________________________________________
//                             V     |                                           |
// Situation exercide Head <-> . <-> nodeA <-> nodeB <-> nodeC <-> nodeD <-> nodeE <-> Tail


// head <-> A  : head -> next = A et A -> prev = head
// head <-> node <-> A 
//         => head -> next = node 
//         => node -> prev = head  
//         => node -> next = head -> next
//         => node -> prev = A -> prev;

class LRUCache : public Cache{
    private:
        void AddToHead(Node* node){
            node -> next = head -> next;    // |    | -> |
            node -> prev = head;            // | <- |    | 
            head->next->prev = node;        // |    | <- | :   car on ne sait pas (veux pas se faire chier à prendre un par un a chque
                                            //                 fois le pointeur du noeud 1), le head reste fixe !
            head -> next = node;            // | -> |    |
        }
        void RemoveNode(Node* node){
            node -> prev -> next = node -> next; 
            node -> next -> prev = node -> prev;   // | <- |   |  =  | deleted | -> + <- | 
        }
        void moveToHead(Node* node){
            RemoveNode(node); // on enlève le noeud existant
            AddToHead(node); // on lajouter en tete position.
            
        }
    public:
        LRUCache(int capacity){
            cp = capacity;
            head = new Node(0,0);
            tail = new Node(0,0); 
            head -> next = tail;
            tail -> prev = head;
        }
        int get(int key) override {
            if (mp.find(key) != mp.end()){
                Node* node = mp[key];
                moveToHead(node);
                return node -> value;
            }
            return -1;
        }
        void set(int k, int v) override {
            if (mp.find(k) != mp.end()){
                Node* node = mp[k];
                node -> value = v;
                moveToHead(node);
            }
            else{
                Node* nodeNew = new Node(k,v);
                // D'ABORD vérifier si plein AVANT d'ajouter
                if (mp.size() >= cp - 1){
                    Node* last = tail -> prev;
                    RemoveNode(last);
                    mp.erase(last -> key);
                    delete last;
                } 
                // PUIS ajouter le nouveau nœud
                AddToHead(nodeNew);
                mp[k] = nodeNew;
            };
        }

        void printCache() {
            cout << "Cache (most recent -> least recent): ";
            Node* current = head->next;
            while (current != tail) {
                cout << "[" << current->key << ":" << current->value << "] ";
                current = current->next;
            }
            cout << endl;
        }


};

int main() {
    int n, capacity;
    cout << "Enter a number :" << endl;
    cin >> n;
    cout << "Enter a capacity:" << endl;
    cin >> capacity;
    LRUCache l(capacity);
    for(int i=0; i<n;i++){
        cout << "two command aviable : set or get. Please choose one :" << endl;
        string command; 
        cin>> command;
        if(command=="set"){
            int key,value;
            cout << "=> Enter a key:" << endl;
            cin >> key;
            cout << "=> Enter a value:" << endl;
            cin >> value;
            l.set(key,value);
        }
        else if (command=="get"){
            int key;
            cout << "=> Enter an existing key:" << endl;
            cin >> key;
            l.get(key);
        }        
    }

    l.printCache();
    return 0;
}
