#define TODO(__msg) { std::cerr << "TODO: " << __msg << std::endl; }
#define PANIC(__msg) { std::cerr << "PANIC: " << __msg << std::endl; throw std::exception(); }
