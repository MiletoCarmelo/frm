
// g++ -std=c++20 -Wall -Wextra -Wpedantic -O3 -march=native -DNDEBUG -I. cpp_basis.cpp  -o main

#include <iostream>
#include <map>
#include <string>
#include <iomanip>
#include <stdexcept>
using namespace std;

class PortfolioValue {
private :
    double aum_;  // Actif sous gestion (AUM) en millions de dollars
    map <string, double> positions_;  // Positions du portefeuille (sous-jacent, taille)

    public:
    PortfolioValue(double value) : aum_(value), positions_() {
        if (value < 0) {
            cout << "AUM cannot be negative. Setting to 0." << endl;
            aum_ = 0;
        }
    }

    double get_aum() const {
        return aum_;
    }

    void add_aum(double value){
        if (value < 0) {
            cout << "Cannot add negative value to AUM." << endl;
        } else {
            aum_ += value;
            cout << "AUM updated to: " << aum_ << " $." << endl;
        }
    }

    void subtract_aum(double value){
        if (value < 0) {
            cout << "Cannot subtract negative value from AUM." << endl;
        } else if (value > aum_) {
            cout << "Cannot subtract more than current AUM." << endl;
        } else {
            aum_ -= value;
            cout << "AUM updated to: " << aum_ << " $." << endl;
        }
    }
};

int main() {

    PortfolioValue portfolio(100);

    cout << "Initial AUM: " << portfolio.get_aum() << " $" << endl;
    portfolio.add_aum(50);
    portfolio.subtract_aum(20);
    portfolio.subtract_aum(2000);  // Test error case
    portfolio.add_aum(-1000);  // Test error case

}