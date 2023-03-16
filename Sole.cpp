    // Head, non-function values
    template <typename impl_, typename single>
    struct make_bin<impl_, pack<single>, pack<>> : single {
      typedef single super;

      typedef pack<single> Params;
      typedef pack<> Args;
      typedef typename join<Args, Params>::type Pack;

      constexpr static unsigned paramsN = count<Params>::value;
      constexpr static unsigned argsN = count<Args>::value;

      typedef void Next;
      typedef void Base;

      typedef make_bin Tail;
      typedef impl_ Head;

      make_bin(ref<make_bin> at, arg_tuple<Args> t)
        : super(at, super::make(impl_::name))
        , _args(t)
      { }

      make_bin(ref<make_bin> at, make_bin const& other)
        : super(at, Type(other.ty))
        , _args(copy_arg_tuple(arg_unpack<argsN>(), other._args))
      { }
      ref<Val> copy() const override { return ref<impl_>(this->h.app(), *this); }

    protected:
      VisitTable visit_table() const override {
        return make_visit_table<decltype(this)>::function();
      }

      arg_tuple<Args> _args;
    };

