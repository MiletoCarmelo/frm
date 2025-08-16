#include <iostream>
#include <algorithm>
#include <vector>
#include <string>
#include <random>
#include <iomanip>
#include <sstream>
#include <utility>
#include <set>
#include <cstdlib>

using namespace std;

class Message {
private:
    string msg;
    int pos;
public: 
    Message() {}
    Message(string val, int p) : msg{val}, pos(p){};
    const string& get_text() {
       return msg; 
    }
    bool operator<(Message& other){
        if(this->pos < other.pos){
            return true;
        }
        else {
            return false;
        }
    }
};

class MessageFactory {
private:
    vector<Message> queue;
    int size = 0;
public:
    MessageFactory() {}
    Message create_message(const string& text) {
        size +=1;
        Message message(text, size);
        return message;
    }
};

class Recipient {
public:
    Recipient() {}
    void receive(const Message& msg) {
        messages_.push_back(msg);
    }
    void print_messages() {
        fix_order();
        for (auto& msg : messages_) {
            cout << msg.get_text() << endl;
        }
        messages_.clear();
    }
private:
    void fix_order() {
        sort(messages_.begin(), messages_.end());
    }
    vector<Message> messages_;
};

class Network {
public:
    static void send_messages(vector<Message> messages, Recipient& recipient) {
    // simulates the unpredictable network, where sent messages might arrive in unspecified order
        random_shuffle(messages.begin(), messages.end());         
        for (auto msg : messages) {
            recipient.receive(msg);
        }
    }
};



int main() {
    MessageFactory message_factory;
    Recipient recipient;
    vector<Message> messages;
    string text;
    while (getline(cin, text)) {
        messages.push_back(message_factory.create_message(text));
    }
    Network::send_messages(messages, recipient);
    recipient.print_messages();
}


/*
 Exemple Input:
 --------------
Alex 
Hello Monique! 
What'up? 
Not much :()

 Expected Output:
 ---------------
Alex => message 1 position 1
Hello Monique! => message 2 position 2
What'up? => message 3 position 3
Not much :( => message 4 position 4

 Wrong Output:
 --------------
What'up? => message 3 position 1
Hello Monique! => message 2 position 2
Not much :( => message 4 position 3
Alex => message 1 position 4
*/
