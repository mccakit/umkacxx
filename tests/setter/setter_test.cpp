import std;
import umkacxx;
import boost.ut;

int main()
{
    using namespace boost::ut;

    constexpr char mod_src[] = {
    #embed "mod.um"
        , 0};

    umkacxx::types::module_t mod{"mod.um", mod_src, {}};
    umkacxx::umka umka{"./main.um", 4096, {mod}};

    "set and get int"_test = [&umka]
    {
        umka.call<void>("setValue", "mod.um", {umkacxx::types::int_t(3)});
        int r = static_cast<int>(umka.call<umkacxx::types::int_t>("run", "./main.um", {}));
        expect(r == 3_i);
    };

    "overwrite int"_test = [&umka]
    {
        umka.call<void>("setValue", "mod.um", {umkacxx::types::int_t(3)});
        umka.call<void>("setValue", "mod.um", {umkacxx::types::int_t(99)});
        int r = static_cast<int>(umka.call<umkacxx::types::int_t>("run", "./main.um", {}));
        expect(r == 99_i);
    };

    "set and get str"_test = [&umka]
    {
        umka.call<void>("setName", "mod.um", {umka.make_str("Umka")});
        std::string result{umka.call<umkacxx::types::str_t>("runName", "./main.um", {})};
        expect(result == "Umka");
    };

    "overwrite str"_test = [&umka]
    {
        umka.call<void>("setName", "mod.um", {umka.make_str("First")});
        umka.call<void>("setName", "mod.um", {umka.make_str("Second")});
        std::string result{umka.call<umkacxx::types::str_t>("runName", "./main.um", {})};
        expect(result == "Second");
    };

    "set and get real"_test = [&umka]
    {
        umka.call<void>("setScore", "mod.um", {double(3.14)});
        double r = umka.call<umkacxx::types::real_t>("runScore", "./main.um", {});
        expect(r == 3.14_d);
    };

    "set and get bool"_test = [&umka]
    {
        umka.call<void>("setActive", "mod.um", {umkacxx::types::int_t(1)});
        bool r = static_cast<bool>(umka.call<umkacxx::types::bool_t>("runActive", "./main.um", {}));
        expect(r == true);
    };

    "toggle bool"_test = [&umka]
    {
        umka.call<void>("setActive", "mod.um", {umkacxx::types::int_t(1)});
        umka.call<void>("setActive", "mod.um", {umkacxx::types::int_t(0)});
        bool r = static_cast<bool>(umka.call<umkacxx::types::bool_t>("runActive", "./main.um", {}));
        expect(r == false);
    };

    "set two params and get sum"_test = [&umka]
    {
        umka.call<void>("setValueAndOffset", "mod.um", {umkacxx::types::int_t(10), umkacxx::types::int_t(5)});
        int r = static_cast<int>(umka.call<umkacxx::types::int_t>("runSum", "./main.um", {}));
        expect(r == 15_i);
    };

    "negative int"_test = [&umka]
    {
        umka.call<void>("setValue", "mod.um", {umkacxx::types::int_t(-7)});
        int r = static_cast<int>(umka.call<umkacxx::types::int_t>("run", "./main.um", {}));
        expect(r == -7_i);
    };

    "zero values"_test = [&umka]
    {
        umka.call<void>("setValue", "mod.um", {umkacxx::types::int_t(0)});
        umka.call<void>("setScore", "mod.um", {double(0.0)});
        umka.call<void>("setActive", "mod.um", {umkacxx::types::int_t(0)});
        int ri = static_cast<int>(umka.call<umkacxx::types::int_t>("run", "./main.um", {}));
        double rd = umka.call<umkacxx::types::real_t>("runScore", "./main.um", {});
        bool rb = static_cast<bool>(umka.call<umkacxx::types::bool_t>("runActive", "./main.um", {}));
        expect(ri == 0_i);
        expect(rd == 0.0_d);
        expect(rb == false);
    };

    "set point x and y"_test = [&umka]
    {
        umka.call<void>("setPoint", "mod.um", {umkacxx::types::int_t(4), umkacxx::types::int_t(7)});
        int x = static_cast<int>(umka.call<umkacxx::types::int_t>("runPointX", "./main.um", {}));
        int y = static_cast<int>(umka.call<umkacxx::types::int_t>("runPointY", "./main.um", {}));
        expect(x == 4_i);
        expect(y == 7_i);
    };

    "point sum"_test = [&umka]
    {
        umka.call<void>("setPoint", "mod.um", {umkacxx::types::int_t(3), umkacxx::types::int_t(9)});
        int r = static_cast<int>(umka.call<umkacxx::types::int_t>("runPointSum", "./main.um", {}));
        expect(r == 12_i);
    };

    "overwrite point"_test = [&umka]
    {
        umka.call<void>("setPoint", "mod.um", {umkacxx::types::int_t(1), umkacxx::types::int_t(2)});
        umka.call<void>("setPoint", "mod.um", {umkacxx::types::int_t(10), umkacxx::types::int_t(20)});
        int x = static_cast<int>(umka.call<umkacxx::types::int_t>("runPointX", "./main.um", {}));
        int y = static_cast<int>(umka.call<umkacxx::types::int_t>("runPointY", "./main.um", {}));
        expect(x == 10_i);
        expect(y == 20_i);
    };
}
