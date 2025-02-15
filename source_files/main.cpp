#pragma once
#define STB_IMAGE_IMPLEMENTATION

#include <iostream>
#include "ShaderApplication.h"



int main() {
    ShaderApplication app;

    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}