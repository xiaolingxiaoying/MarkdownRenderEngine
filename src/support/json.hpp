#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace mwrender::detail {

class JsonValue {
public:
    using Array = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue, std::less<>>;

    JsonValue();
    explicit JsonValue(std::monostate);
    explicit JsonValue(bool value);
    explicit JsonValue(double value);
    explicit JsonValue(std::string value);
    explicit JsonValue(Array value);
    explicit JsonValue(Object value);

    [[nodiscard]] const bool* asBool() const;
    [[nodiscard]] const double* asNumber() const;
    [[nodiscard]] const std::string* asString() const;
    [[nodiscard]] const Array* asArray() const;
    [[nodiscard]] const Object* asObject() const;
    [[nodiscard]] const JsonValue* get(std::string_view key) const;

private:
    using Storage =
        std::variant<std::monostate, bool, double, std::string, Array, Object>;
    Storage storage_;
};

struct JsonParseResult {
    std::optional<JsonValue> value;
    std::string error;
};

[[nodiscard]] JsonParseResult parseJson(std::string_view source);

} // namespace mwrender::detail
