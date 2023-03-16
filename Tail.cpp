    // Tail
    template <typename impl_, typename ret, typename arg_h, typename ...args>
    struct make_bin<impl_, pack<ret>, pack<arg_h, args...>> : ret {
      typedef ret super;

      typedef pack<ret> Params;
      typedef typename reverse<pack<arg_h, args...>>::type Args;
      typedef typename join<Args, Params>::type Pack;

      constexpr static unsigned paramsN = count<Params>::value;
      constexpr static unsigned argsN = count<Args>::value;

      typedef void Next;
      typedef make_bin<impl_, pack<arg_h, ret>, pack<args...>> Base;

      typedef make_bin Tail;
      typedef make_bin<impl_, Pack, pack<>> Head;

      make_bin(ref<make_bin> at, arg_tuple<Args> t)
        : super(at, super::make(impl_::name))
        , _args(t)
      { }
      make_bin(ref<make_bin> at, Type const& base_type, arg_tuple<typename Base::Args> base_args, ref<arg_h> arg)
        : super(at, done_applied(base_type, arg->type()))
        , _args(std::tuple_cat(base_args, std::make_tuple(arg)))
      { }
      static inline void make_at(ref<make_bin> at, Type const& base_type, arg_tuple<typename Base::Args> base_args, ref<arg_h> arg) {
        new impl_(at, base_type, base_args, arg);
      }

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

