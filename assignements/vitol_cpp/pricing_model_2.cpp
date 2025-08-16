#include <unordered_map> 
#include <string>
#include <cmath>
#include <map>
#include <iostream>
#include <utility>
using namespace std;


struct option{
    double S;
    double K;
    double T;
    double R;
    double V;
    string cp;
    double price;
    option(double s, double k, double t, double r, double v, string cp) : S(s), K(k), T(t), R(r), V(v), cp(cp), price(0.0) {};
};


struct Cache{
    protected:
        map<string,option*> mp_options;
        size_t nb_op;
        // Fonctions virtuelles pures = 0
        virtual option get(int) = 0;
        virtual void set(int, option*) = 0;
        
        virtual ~Cache() = default;  // Destructeur virtuel obligatoire
};

class maths{
    public:
        static double norm_cdf(double x) {
            return 0.5 * (1.0 + std::erf(x / std::sqrt(2.0)));
        };
};


class BSM : public Cache{
    private:
        string cache_key(option* op){
            return to_string(op->S) + "_" +
                    to_string(op->K) + "_" +
                    to_string(op->T) + "_" +
                    to_string(op->R) + "_" +
                    to_string(op->V) + "_" +
                    op->cp ;
        };
        pair<double, double> d1_d2_(option* op){
            if (op->T<=0 || op->V<=0) return {0.0,0.0};
            double d1 = ( log(op->S/op->K) + (op->R+(op->V * op->V)/2) ) / (op->V*sqrt(op->T));
            double d2 = d1 - op->V * sqrt(op->T);
            return {d1,d2};

        };
        double price_(option* op){
            auto [d1,d2] = d1_d2_(op);
            if(op->cp == "c"){
                return op->S * maths::norm_cdf(d1) - op->K*exp(-op->R*sqrt(op->T))*maths::norm_cdf(d2);
            }
            else {
                return op->K * maths::norm_cdf(d2) - op->S * exp(-op->R*sqrt(op->T))*maths::norm_cdf(d1);
            };
        }
    public:
        BSM() {
            nb_op = 0;
            mp_options = {};
        };
        void add(option* op){
            string key = cache_key(op);
            if(mp_options.find(key) != mp_options.end()){
                mp_options[key] = op;
                nb_op += 1;
            } else {
                cout << "option already present in portfolio" << endl;
            }
        };
        void price(){
            for(auto& i : mp_options){
                // second pointe sur le 2ème arg i = [key, option_ptr]
                i.second->price = price_(i.second); 
            }
        };
        void print(){
            for(auto i : mp_options){
                cout << i.first << " : " ;
                cout << "S=" << i.second->S ;
                cout << "K=" << i.second->K ;
                cout << "R=" << i.second->R ;
                cout << "T=" << i.second->T ;
                cout << "V=" << i.second->V ;
                cout << "Type=" << i.second->cp << endl ;
            }
        }
};

int main() {
    cout << " hello " <<endl;
    option* o1 = new option(100,103,0.2,0.03,0.1,"p");
    BSM model;
    //model.add(o1);
   //  model.price();
   // model.print();
}