#pragma once

#include "Settings.h"
#include "Utils.h"



#include <iostream>

class Manager {
public:
    void Init() {
        // Initialization code here
        std::cout << "Manager Initialized" << std::endl;
    }

    // Constructor to call Init()
    Manager() { Init(); }

    // Static method to get the singleton instance
    static Manager* GetSingleton() {
        static Manager singleton;
        return &singleton;
    }
};