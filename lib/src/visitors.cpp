#include "sel/visitors.hpp"
#include "sel/engine.hpp"

void sel::Visitor::operator()(Val const& v) { v.accept(*this); }
