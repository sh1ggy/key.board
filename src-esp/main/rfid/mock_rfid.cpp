#include "rfid.h"
#include <string>
#include <iostream>

std::string test_str;
void setup_rfid_reader()
{
    test_str = "Mock tester";
    std::cout << "Running setup for " << test_str << '\n';
}
