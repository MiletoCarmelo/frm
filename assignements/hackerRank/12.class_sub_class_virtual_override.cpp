#include <iostream>
using namespace std;

class Rectangle{
protected:
    int height;
    int width;
public:
    Rectangle() : height{0}, width{0} {}
    Rectangle(int x, int y) : height{x}, width{y} {}
    
    // AJOUT: virtual pour permettre l'override
    virtual void display(){
        cout << height << " " << width << endl;
    }
};

class RectangleArea: public Rectangle{
public:
    RectangleArea() {}
    
    void read_input(){
        cin >> width >> height;
    }
    
    // Maintenant override fonctionne
    void display() override {
        cout << width * height << endl;
    }
};


int main()
{
    /*
     * Declare a RectangleArea object
     */
    RectangleArea r_area;
    
    /*
     * Read the width and height
     */
    r_area.read_input();
    
    /*
     * Print the width and height
     */
    r_area.Rectangle::display();
    
    /*
     * Print the area
     */
    r_area.display();
    
    return 0;
}