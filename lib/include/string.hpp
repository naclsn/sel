#ifndef SEL_STRING_HPP
#define SEL_STRING_HPP

#include "value.hpp"

namespace sel {

  class Str : public Val {
  public:
    Str(): Val(new Type(Ty::STR)) { }

    Val* coerse(Type to) override;
  }; // class Str

  class StrBuffered : public Str {
  private:
    mutable std::string* buffer;

  protected:
    void eval() override;
    std::ostream& output(std::ostream& out) override;

  public:
    StrBuffered(): buffer(nullptr) { }
    StrBuffered(std::string* s): buffer(s) { }
    StrBuffered(std::string s): buffer(new std::string(s)) { }
    StrBuffered(char const* c): buffer(new std::string(c)) { }
    ~StrBuffered() {
      TRACE(~StrBuffered);
      delete buffer;
    }

    // Val* clone() const override;
    // Val* coerse(Type to) override;

    void setBuffer(std::string* s) { buffer = s; }
    bool hasBuffer() const { return nullptr != buffer; }
    std::string* getBuffer() const { return buffer; }
  }; // class StrBuffered

  class StrFromStream : public StrBuffered {
  private:
    mutable std::istream* stream;

  protected:
    void eval() override;

  public:
    StrFromStream(): stream(nullptr) { }
    StrFromStream(std::istream* in): stream(in) { }
    ~StrFromStream() {
      TRACE(~StrFromStream);
      delete stream;
    }

    // Val* clone() const override;

    void setStream(std::istream* in) { stream = in; }
  };

} // namespace sel

#endif // SEL_STRING_HPP
