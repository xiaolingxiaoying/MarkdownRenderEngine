#include "support/json.hpp"

#include <cctype>
#include <cstdint>
#include <cmath>
#include <cstdlib>

namespace mwrender::detail {
namespace {

void appendUtf8(std::string& output, std::uint32_t codePoint) {
    if (codePoint <= 0x7FU) {
        output += static_cast<char>(codePoint);
    } else if (codePoint <= 0x7FFU) {
        output += static_cast<char>(0xC0U | (codePoint >> 6U));
        output += static_cast<char>(0x80U | (codePoint & 0x3FU));
    } else if (codePoint <= 0xFFFFU) {
        output += static_cast<char>(0xE0U | (codePoint >> 12U));
        output += static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU));
        output += static_cast<char>(0x80U | (codePoint & 0x3FU));
    } else {
        output += static_cast<char>(0xF0U | (codePoint >> 18U));
        output += static_cast<char>(0x80U | ((codePoint >> 12U) & 0x3FU));
        output += static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU));
        output += static_cast<char>(0x80U | (codePoint & 0x3FU));
    }
}

class JsonParser {
public:
    explicit JsonParser(std::string_view source)
        : source_(source) {}

    JsonParseResult parse() {
        skipWhitespace();
        auto value = parseValue();
        if (!value) {
            return {std::nullopt, error_};
        }
        skipWhitespace();
        if (position_ != source_.size()) {
            fail("unexpected trailing content");
            return {std::nullopt, error_};
        }
        return {std::move(value), {}};
    }

private:
    std::optional<JsonValue> parseValue() {
        skipWhitespace();
        if (position_ >= source_.size()) {
            fail("unexpected end of input");
            return std::nullopt;
        }
        switch (source_[position_]) {
        case '{':
            return parseObject();
        case '[':
            return parseArray();
        case '"': {
            auto value = parseString();
            if (!value) {
                return std::nullopt;
            }
            return std::optional<JsonValue>(
                std::in_place,
                std::move(*value));
        }
        case 't':
            return parseLiteral("true", true);
        case 'f':
            return parseLiteral("false", false);
        case 'n':
            return parseLiteral("null", std::monostate{});
        default:
            if (source_[position_] == '-' ||
                std::isdigit(
                    static_cast<unsigned char>(source_[position_])) != 0) {
                return parseNumber();
            }
            fail("unexpected token");
            return std::nullopt;
        }
    }

    std::optional<JsonValue> parseObject() {
        ++position_;
        JsonValue::Object object;
        skipWhitespace();
        if (consume('}')) {
            return std::optional<JsonValue>(
                std::in_place,
                std::move(object));
        }

        while (position_ < source_.size()) {
            auto key = parseString();
            if (!key) {
                return std::nullopt;
            }
            skipWhitespace();
            if (!consume(':')) {
                fail("expected ':' after object key");
                return std::nullopt;
            }
            auto value = parseValue();
            if (!value) {
                return std::nullopt;
            }
            object.insert_or_assign(std::move(*key), std::move(*value));
            skipWhitespace();
            if (consume('}')) {
                return std::optional<JsonValue>(
                    std::in_place,
                    std::move(object));
            }
            if (!consume(',')) {
                fail("expected ',' or '}' in object");
                return std::nullopt;
            }
            skipWhitespace();
        }
        fail("unterminated object");
        return std::nullopt;
    }

    std::optional<JsonValue> parseArray() {
        ++position_;
        JsonValue::Array array;
        skipWhitespace();
        if (consume(']')) {
            return std::optional<JsonValue>(
                std::in_place,
                std::move(array));
        }
        while (position_ < source_.size()) {
            auto value = parseValue();
            if (!value) {
                return std::nullopt;
            }
            array.push_back(std::move(*value));
            skipWhitespace();
            if (consume(']')) {
                return std::optional<JsonValue>(
                    std::in_place,
                    std::move(array));
            }
            if (!consume(',')) {
                fail("expected ',' or ']' in array");
                return std::nullopt;
            }
            skipWhitespace();
        }
        fail("unterminated array");
        return std::nullopt;
    }

    std::optional<std::string> parseString() {
        skipWhitespace();
        if (!consume('"')) {
            fail("expected string");
            return std::nullopt;
        }
        std::string result;
        while (position_ < source_.size()) {
            const char character = source_[position_++];
            if (character == '"') {
                return result;
            }
            if (static_cast<unsigned char>(character) < 0x20U) {
                fail("control character in string");
                return std::nullopt;
            }
            if (character != '\\') {
                result += character;
                continue;
            }
            if (position_ >= source_.size()) {
                fail("unterminated escape sequence");
                return std::nullopt;
            }
            const char escape = source_[position_++];
            switch (escape) {
            case '"': result += '"'; break;
            case '\\': result += '\\'; break;
            case '/': result += '/'; break;
            case 'b': result += '\b'; break;
            case 'f': result += '\f'; break;
            case 'n': result += '\n'; break;
            case 'r': result += '\r'; break;
            case 't': result += '\t'; break;
            case 'u': {
                const auto first = parseHexCodeUnit();
                if (!first) {
                    return std::nullopt;
                }
                std::uint32_t codePoint = *first;
                if (codePoint >= 0xD800U && codePoint <= 0xDBFFU) {
                    if (position_ + 2 > source_.size() ||
                        source_[position_] != '\\' ||
                        source_[position_ + 1] != 'u') {
                        fail("missing low unicode surrogate");
                        return std::nullopt;
                    }
                    position_ += 2;
                    const auto second = parseHexCodeUnit();
                    if (!second || *second < 0xDC00U || *second > 0xDFFFU) {
                        fail("invalid low unicode surrogate");
                        return std::nullopt;
                    }
                    codePoint = 0x10000U +
                        ((codePoint - 0xD800U) << 10U) +
                        (*second - 0xDC00U);
                } else if (codePoint >= 0xDC00U && codePoint <= 0xDFFFU) {
                    fail("unexpected low unicode surrogate");
                    return std::nullopt;
                }
                appendUtf8(result, codePoint);
                break;
            }
            default:
                fail("invalid string escape");
                return std::nullopt;
            }
        }
        fail("unterminated string");
        return std::nullopt;
    }

    std::optional<std::uint32_t> parseHexCodeUnit() {
        if (position_ + 4 > source_.size()) {
            fail("incomplete unicode escape");
            return std::nullopt;
        }
        std::uint32_t value = 0;
        for (int index = 0; index < 4; ++index) {
            const char character = source_[position_++];
            value <<= 4U;
            if (character >= '0' && character <= '9') {
                value |= static_cast<std::uint32_t>(character - '0');
            } else if (character >= 'a' && character <= 'f') {
                value |= static_cast<std::uint32_t>(character - 'a' + 10);
            } else if (character >= 'A' && character <= 'F') {
                value |= static_cast<std::uint32_t>(character - 'A' + 10);
            } else {
                fail("invalid unicode escape");
                return std::nullopt;
            }
        }
        return value;
    }

    std::optional<JsonValue> parseNumber() {
        const std::size_t start = position_;
        if (source_[position_] == '-') {
            ++position_;
        }
        if (position_ >= source_.size()) {
            fail("invalid number");
            return std::nullopt;
        }
        if (source_[position_] == '0') {
            ++position_;
        } else {
            if (std::isdigit(
                    static_cast<unsigned char>(source_[position_])) == 0) {
                fail("invalid number");
                return std::nullopt;
            }
            while (position_ < source_.size() &&
                   std::isdigit(
                       static_cast<unsigned char>(source_[position_])) != 0) {
                ++position_;
            }
        }
        if (position_ < source_.size() && source_[position_] == '.') {
            ++position_;
            const auto fractionStart = position_;
            while (position_ < source_.size() &&
                   std::isdigit(
                       static_cast<unsigned char>(source_[position_])) != 0) {
                ++position_;
            }
            if (fractionStart == position_) {
                fail("invalid number fraction");
                return std::nullopt;
            }
        }
        if (position_ < source_.size() &&
            (source_[position_] == 'e' || source_[position_] == 'E')) {
            ++position_;
            if (position_ < source_.size() &&
                (source_[position_] == '+' || source_[position_] == '-')) {
                ++position_;
            }
            const auto exponentStart = position_;
            while (position_ < source_.size() &&
                   std::isdigit(
                       static_cast<unsigned char>(source_[position_])) != 0) {
                ++position_;
            }
            if (exponentStart == position_) {
                fail("invalid number exponent");
                return std::nullopt;
            }
        }

        const std::string number(source_.substr(start, position_ - start));
        char* end = nullptr;
        const double value = std::strtod(number.c_str(), &end);
        if (!end || *end != '\0' || !std::isfinite(value)) {
            fail("invalid number");
            return std::nullopt;
        }
        return std::optional<JsonValue>(std::in_place, value);
    }

    template <typename Value>
    std::optional<JsonValue> parseLiteral(
        std::string_view literal,
        Value value) {
        if (source_.substr(position_, literal.size()) != literal) {
            fail("invalid literal");
            return std::nullopt;
        }
        position_ += literal.size();
        return std::optional<JsonValue>(
            std::in_place,
            std::move(value));
    }

    void skipWhitespace() {
        while (position_ < source_.size() &&
               std::isspace(
                   static_cast<unsigned char>(source_[position_])) != 0) {
            ++position_;
        }
    }

    bool consume(char expected) {
        if (position_ < source_.size() && source_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    void fail(std::string_view message) {
        if (error_.empty()) {
            error_ = std::string(message) +
                " at byte " + std::to_string(position_);
        }
    }

    std::string_view source_;
    std::size_t position_ = 0;
    std::string error_;
};

} // namespace

JsonValue::JsonValue()
    : storage_(std::monostate{}) {}

JsonValue::JsonValue(std::monostate)
    : storage_(std::monostate{}) {}

JsonValue::JsonValue(bool value)
    : storage_(value) {}

JsonValue::JsonValue(double value)
    : storage_(value) {}

JsonValue::JsonValue(std::string value)
    : storage_(std::move(value)) {}

JsonValue::JsonValue(Array value)
    : storage_(std::move(value)) {}

JsonValue::JsonValue(Object value)
    : storage_(std::move(value)) {}

const bool* JsonValue::asBool() const {
    return std::get_if<bool>(&storage_);
}

const double* JsonValue::asNumber() const {
    return std::get_if<double>(&storage_);
}

const std::string* JsonValue::asString() const {
    return std::get_if<std::string>(&storage_);
}

const JsonValue::Array* JsonValue::asArray() const {
    return std::get_if<Array>(&storage_);
}

const JsonValue::Object* JsonValue::asObject() const {
    return std::get_if<Object>(&storage_);
}

const JsonValue* JsonValue::get(std::string_view key) const {
    const auto* object = asObject();
    if (!object) {
        return nullptr;
    }
    const auto value = object->find(key);
    return value == object->end() ? nullptr : &value->second;
}

JsonParseResult parseJson(std::string_view source) {
    return JsonParser(source).parse();
}

} // namespace mwrender::detail
