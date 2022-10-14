#define TODO(__msg) std::cerr << "TODO: " << __msg << std::endl;

#define VERB_DTOR(__cname, __body) ~__cname() { std::cerr << "VERB_DTOR: ~" #__cname "()" << std::endl; __body }
