#include "sel/visitors.hpp"
#include "sel/engine.hpp"

// well, this is frustrating :-(
void sel::Visitor::operator()(Val const& v) { v.accept(*this); }
