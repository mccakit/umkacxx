import std;
import umkacxx;
import cppfuncs;
import ut;

int main()
{
    using namespace ut;
    umkacxx::umka umka{"./main.um", 4096, {cppfuncs}};

    "add func"_test = [&umka] {
        int r = static_cast<int>(umka.call<umkacxx::types::int_t>("run"));
        expect(r == 3_i);
    };
}
