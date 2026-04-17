module;
export module cppfuncs;
import umkacxx;

void umka_add(umkacxx::types::umka_slot *params, umkacxx::types::umka_slot *result)
{
    auto a = umkacxx::get_param<umkacxx::types::int_t>(params, 0);
    auto b = umkacxx::get_param<umkacxx::types::int_t>(params, 1);
    umkacxx::set_result<umkacxx::types::int_t>(result, a + b);
}

constexpr char src[] = {
#embed "fn_wrap.um"
    , 0};

export umkacxx::types::module_t cppfuncs{"fn_wrap.um", src, {{"add", umka_add}}};
