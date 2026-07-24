module;
export module hooks;
import umka;
import std;

void umka_add(umka::slot_t *params, umka::slot_t *result)
{
    auto a = umka::get_param<umka::int_t>(params, 0);
    auto b = umka::get_param<umka::int_t>(params, 1);
    umka::set_result<umka::int_t>(result, a + b);
}

void umka_concat(umka::slot_t *params, umka::slot_t *result)
{
    auto s1 = umka::get_param<umka::str_t>(params, 0);
    auto s2 = umka::get_param<umka::str_t>(params, 1);
    std::string combined = std::string(s1) + s2;
    umka::set_result<umka::str_t>(result, combined.c_str());
}

void umka_mul_real(umka::slot_t *params, umka::slot_t *result)
{
    auto a = umka::get_param<umka::real_t>(params, 0);
    auto b = umka::get_param<umka::real_t>(params, 1);
    umka::set_result<umka::real_t>(result, a * b);
}

void umka_sum_arr(umka::slot_t *params, umka::slot_t *result)
{
    auto arr = umka::get_param<umka::int_t[3]>(params, 0);
    umka::int_t sum = 0;
    for (int i = 0; i < 3; ++i)
    {
        sum += arr[i];
    }
    umka::set_result<umka::int_t>(result, sum);
}

void umka_sum_dyn_arr(umka::slot_t *params, umka::slot_t *result)
{
    auto arr = umka::get_param<umka::arr_t<umka::int_t>>(params, 0);
    umka::int_t sum = 0;
    for (int i = 0; i < arr.len(); ++i)
    {
        sum += arr[i];
    }
    umka::set_result<umka::int_t>(result, sum);
}
constexpr char src[] = {
#embed "fn_wrap.um"
    , 0};

export umka::module_t hooks{
    "fn_wrap.um",
    src,
    {
        {"add", umka_add},
        {"concat", umka_concat},
        {"mul_real", umka_mul_real},
        {"sum_arr", umka_sum_arr},
        {"sum_dyn_arr", umka_sum_dyn_arr},
    }
};
