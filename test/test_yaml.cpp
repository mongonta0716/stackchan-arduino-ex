#include "scex_test.h"

#include "SCEX_Yaml.h"

using namespace SCEX;

void test_parses_axes_list_and_scalars() {
    std::string doc =
        "servo:\n"
        "  axes:\n"
        "    - name: x\n"
        "      driver: pwm\n"
        "      pin_tx: 7\n"
        "      lower_limit: 0\n"
        "      upper_limit: 300\n"
        "      easing: quad_in_out\n"
        "    - name: y\n"
        "      driver: m5_scs\n"
        "      pin_tx: 6\n"
        "takao_base: false\n"
        "servo_type: \"M5_SCS\" # trailing comment\n"
        "balloon:\n"
        "  font_language: \"JA\"\n"
        "  lyrics:\n"
        "    - \"hello\"\n"
        "    - \"world\"\n";

    YamlValue root;
    std::string error;
    SCEX_ASSERT_TRUE(YamlParser::parse(doc, &root, &error));

    SCEX_ASSERT_EQ_INT(2, root["servo"]["axes"].size());
    SCEX_ASSERT_EQ_STR("x", root["servo"]["axes"][static_cast<size_t>(0)]["name"].asString());
    SCEX_ASSERT_EQ_STR("pwm", root["servo"]["axes"][static_cast<size_t>(0)]["driver"].asString());
    SCEX_ASSERT_EQ_INT(7, root["servo"]["axes"][static_cast<size_t>(0)]["pin_tx"].asInt());
    SCEX_ASSERT_EQ_STR("y", root["servo"]["axes"][static_cast<size_t>(1)]["name"].asString());
    SCEX_ASSERT_EQ_STR("m5_scs", root["servo"]["axes"][static_cast<size_t>(1)]["driver"].asString());

    SCEX_ASSERT_FALSE(root["takao_base"].asBool(true));
    SCEX_ASSERT_EQ_STR("M5_SCS", root["servo_type"].asString());
    SCEX_ASSERT_EQ_STR("JA", root["balloon"]["font_language"].asString());
    SCEX_ASSERT_EQ_INT(2, root["balloon"]["lyrics"].size());
    SCEX_ASSERT_EQ_STR("hello", root["balloon"]["lyrics"][static_cast<size_t>(0)].asString());
}

void test_missing_key_returns_null_not_crash() {
    YamlValue root;
    std::string error;
    SCEX_ASSERT_TRUE(YamlParser::parse("a: 1\n", &root, &error));
    SCEX_ASSERT_TRUE(root["does_not_exist"].isNull());
    SCEX_ASSERT_TRUE(root["does_not_exist"]["nested"].isNull());
    SCEX_ASSERT_EQ_INT(0, root["does_not_exist"].size());
}

void test_scalar_types() {
    YamlValue root;
    std::string error;
    std::string doc =
        "int_val: 42\n"
        "float_val: 3.5\n"
        "bool_val: true\n"
        "null_val: ~\n"
        "str_val: hello world\n";
    SCEX_ASSERT_TRUE(YamlParser::parse(doc, &root, &error));
    SCEX_ASSERT_EQ_INT(42, root["int_val"].asInt());
    SCEX_ASSERT_NEAR(3.5, root["float_val"].asFloat(), 0.0001);
    SCEX_ASSERT_TRUE(root["bool_val"].asBool());
    SCEX_ASSERT_TRUE(root["null_val"].isNull());
    SCEX_ASSERT_EQ_STR("hello world", root["str_val"].asString());
}

int main() {
    SCEX_RUN(test_parses_axes_list_and_scalars);
    SCEX_RUN(test_missing_key_returns_null_not_crash);
    SCEX_RUN(test_scalar_types);
    return scex_test::finish();
}
