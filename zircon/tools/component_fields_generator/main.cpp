#include "zircon_defines.h"
#include <iostream>

class MyClass {

public:

int age;

double salary;

private:

std::string m_name;

};

int main() {
    std::cout << "Age field: " << ZIRCON_DEF_MyClass_AGE << "\n";
    std::cout << "Salary field: " << ZIRCON_DEF_MyClass_SALARY << "\n";
    return 0;
}