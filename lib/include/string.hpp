#ifndef SEL_STRING_HPP
#define SEL_STRING_HPP

#include "value.hpp"

namespace sel {

  class Str : public Val {
  private:
    mutable std::string* buffer;

  protected:
    void eval() override {
      if (!hasBuffer()) buffer = new std::string("");
    }
    std::ostream& output(std::ostream& out) override {
      eval();
      return out << *buffer;
    }

  public:
    Str(): Val(Ty::STR) { }
    Str(std::string): Val(Ty::STR) { }
    Str(char*): Val(Ty::STR) { }
    VERB_DTOR(Str, {
      delete buffer;
    });

    Val* coerse(Type to) override;

    void setBuffer(std::string* s) { buffer = s; }
    bool hasBuffer() { return nullptr != buffer; }
    std::string* getBuffer() { return buffer; }
  }; // class Str

  class StrFromStream : public Str {
  private:
    mutable std::istream* stream;

  protected:
    void eval() override;

  public:
    StrFromStream(): stream(nullptr) { }
    StrFromStream(std::istream* in): stream(in) { TODO("StrFromStream ctor"); }
    VERB_DTOR(StrFromStream, {
      delete stream;
    });

    void setStream(std::istream* in) { stream = in; }
  };

} // namespace sel

#endif // SEL_STRING_HPP
