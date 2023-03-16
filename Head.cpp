    // Head
    template <typename impl_, typename param_h, typename ...params>
    struct make_bin<impl_, pack<param_h, params...>, pack<>> : fun<param_h, ty_from_pack<params...>> {
      typedef fun<param_h, ty_from_pack<params...>> super;

      typedef pack<param_h, params...> Params;
      typedef pack<> Args;
      typedef typename join<Args, Params>::type Pack;

      constexpr static unsigned paramsN = count<Params>::value;
      constexpr static unsigned argsN = count<Args>::value;

      typedef make_bin<impl_, pack<params...>, pack<param_h>> Next;
      typedef void Base;

      typedef typename Next::Tail Tail;
      typedef make_bin Head;

      make_bin(ref<make_bin> at, arg_tuple<Args> t)
        : super(at, super::make(impl_::name))
        , _args(t)
      { }

      ref<Val> operator()(ref<Val> arg) override {
        ref<param_h> ok = coerse<typename param_h::base_type>(this->h.app(), arg, this->ty.from());
        auto copy = this->h;
        Next::make_at(this->h, this->ty, _args, ok);
        return copy; // this->h would be accessing into deleted object
      }

      make_bin(ref<make_bin> at, make_bin const& other)
        : super(at, Type(other.ty))
        , _args(copy_arg_tuple(arg_unpack<argsN>(), other._args))
      { }
      ref<Val> copy() const override { return ref<make_bin>(this->h.app(), *this); }

    protected:
      VisitTable visit_table() const override {
        return make_visit_table<decltype(this)>::function();
      }

      arg_tuple<Args> _args;
    };

