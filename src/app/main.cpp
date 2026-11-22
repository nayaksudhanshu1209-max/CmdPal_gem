#include "bootstrap/Bootstrap.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    return cmdpal::bootstrap::runApplication(0, nullptr);
}
#else
int main(int argc, char** argv) {
    return cmdpal::bootstrap::runApplication(argc, argv);
}
#endif
