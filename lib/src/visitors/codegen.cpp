#include "sel/visitors.hpp"

namespace sel {

    void visitNumLiteral(Type const& type, double n) {
    }

    void visitStrLiteral(Type const& type, std::string const& s) {
    }

    void visitLstLiteral(Type const& type, std::vector<Val*> const& v) {
    }

    void visitStrChunks(Type const& type, std::vector<std::string> const& vs) {
    }

    void visitFunChain(Type const& type, std::vector<Fun*> const& f) {
    }

    void visitInput(Type const& type) {
    }

    void visitOutput(Type const& type) {
    }

#define _depth(__depth) _depth_ ## __depth
#define _depth_0 arg
#define _depth_1 base->_depth_0
#define _depth_2 base->_depth_1
#define _depth_3 base->_depth_2

#define _bind_some(__count) _bind_some_ ## __count
#define _bind_some_1(a)          _bind_one(a, 0)
#define _bind_some_2(a, b)       _bind_one(a, 1); _bind_some_1(b)
#define _bind_some_3(a, b, c)    _bind_one(a, 2); _bind_some_2(b, c)
#define _bind_some_4(a, b, c, d) _bind_one(a, 3); _bind_some_3(b, c, d)

#define _bind_count(__count, ...) _bind_some(__count)(__VA_ARGS__)
#define _bind_one(__name, __depth) auto& __name = *node._depth(__depth); (void)__name
#define bind_args(...) _bind_count(__VA_COUNT(__VA_ARGS__), __VA_ARGS__)

  void VisCodegen::visit(bins::abs_::Base const& node) {
  }
  void VisCodegen::visit(bins::abs_ const& node) {
    this->operator()(*node.arg);
  }

  void VisCodegen::visit(bins::add_::Base::Base const& node) {
  }
  void VisCodegen::visit(bins::add_::Base const& node) {
  }
  void VisCodegen::visit(bins::add_ const& node) {
    bind_args(a, b);
    this->operator()(a);
    this->operator()(b);
  }

} // namespace sel
