#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <map>
using namespace std;


struct Node{
    // pointeur "next" vide (il va pointer vers le prochain élément de la liste)
    Node* next;
    // pointeur "prev" vide (il va pointer vers l'élément précédent de la liste)
    Node* prev;
    // clé de l'élément (identifiant unique)
    int key; 
    // valeur de l'élément (valeur stockée)
    int value; 
    // constructeur uniquement les paramètres
    Node(int k, int val) : next(NULL), prev(NULL), key(k), value(val) {}; 
    // constructeur avec les paramètres et les pointeurs next et prev
    Node(Node* n, Node* p, int k, int val) : prev(p), next(n), key(k), value(val) {}; 
};

class Cache{

   protected: 
   // map pour stocker les éléments de la cache avec la clé comme identifiant
   map<int,Node*> mp; 
   // taille maximale de la cache
   int cp;
   // pointeur vers le dernier élément de la cache
   Node* tail; 
   // pointeur vers le premier élément de la cache 
   Node* head; 
   // function pour ajouter un élément à la cache
   virtual void set(int, int) = 0;
    // function pour obtenir un élément de la cache
   virtual int get(int) = 0; 

};


// pour implementer la class LRUCache faut comprendre ce qui est demandé : 
// on a une liste de taille fixe (cp) qui stocke les éléments tappé sur le clavier (ex A, B, C)
// ceux si sont représenté par des noeuds de la classe Node comme ceci :

// head ↔ [A] ↔ [B] ↔ [C] ↔ tail
//      ←      ←      ←
//   prev   prev   prev
//      →      →      →  
//    next   next   next

// chaque noeud a une clé (A, B, C) et une valeur (1, 2, 3) et deux pointeurs :
// next et prev qui pointent vers le noeud suivant et le noeud précédent

// quand on appuie sur une nouvelle touche on va créer un nouveau noeud et modifier les connections
// pointeurs (prev et next) pour intégréer le nouveau à la chaine

//  "least recently used" (LRU) key on keyboard 
class LRUCache : public Cache {
    private: 
        void addToHead(Node* node){
            // pointeur du noeud previous pointant vers le head
            node->prev = head;
            // 
            node->next = head->next;
            head->next->prev = node;
            head->next = node;
        }
        void removeNode(Node* node) {
            node->prev->next = node->next;
            node->next->prev = node->prev;
        }
        void moveToHead(Node* node) {
            removeNode(node);
            addToHead(node);
        }
    public:
        LRUCache(int c){
            cp = c;
            head = new Node(0, 0);
            tail = new Node(0, 0);
            head->next = tail;
            tail->prev = head;
        }
};