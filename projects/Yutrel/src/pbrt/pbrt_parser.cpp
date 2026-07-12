#include "pbrt_parser.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace Yutrel
{
namespace
{

enum class TokenKind
{
    Word,
    String,
    LBracket,
    RBracket,
    End,
};

struct Token
{
    TokenKind kind{TokenKind::End};
    luisa::string text;
    SourceLocation loc;
};

[[noreturn]] void fail(const SourceLocation& loc, luisa::string_view message)
{
    auto s = luisa::format("{}: {}", format_source_location(loc), message);
    throw std::runtime_error{s.c_str()};
}

[[noreturn]] void fail(const Token& token, luisa::string_view message)
{
    fail(token.loc, message);
}

[[nodiscard]] bool is_space(char c) noexcept
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

class Tokenizer
{
private:
    std::filesystem::path m_path;
    luisa::string m_source;
    size_t m_pos{};
    uint m_line{1u};
    uint m_column{1u};

public:
    Tokenizer(std::filesystem::path path, luisa::string source) noexcept
        : m_path{std::move(path)}, m_source{std::move(source)} {}

    [[nodiscard]] luisa::vector<Token> tokenize()
    {
        luisa::vector<Token> tokens;
        while (true)
        {
            auto token = next();
            tokens.emplace_back(token);
            if (token.kind == TokenKind::End)
            {
                break;
            }
        }
        return tokens;
    }

private:
    [[nodiscard]] SourceLocation loc() const
    {
        return SourceLocation{m_path, m_line, m_column};
    }

    [[nodiscard]] char peek() const noexcept
    {
        return m_pos < m_source.size() ? m_source[m_pos] : '\0';
    }

    [[nodiscard]] char get() noexcept
    {
        auto c = peek();
        if (c == '\0')
        {
            return c;
        }
        m_pos++;
        if (c == '\n')
        {
            m_line++;
            m_column = 1u;
        }
        else
        {
            m_column++;
        }
        return c;
    }

    void skip_trivia() noexcept
    {
        while (true)
        {
            while (is_space(peek()))
            {
                (void)get();
            }
            if (peek() != '#')
            {
                break;
            }
            while (peek() != '\0' && peek() != '\n')
            {
                (void)get();
            }
        }
    }

    [[nodiscard]] Token next()
    {
        skip_trivia();
        auto start = loc();
        auto c     = peek();
        if (c == '\0')
        {
            return Token{TokenKind::End, {}, start};
        }
        if (c == '[')
        {
            (void)get();
            return Token{TokenKind::LBracket, "[", start};
        }
        if (c == ']')
        {
            (void)get();
            return Token{TokenKind::RBracket, "]", start};
        }
        if (c == '"')
        {
            (void)get();
            luisa::string text;
            while (true)
            {
                c = get();
                if (c == '\0' || c == '\n')
                {
                    fail(start, "unterminated string literal");
                }
                if (c == '"')
                {
                    break;
                }
                if (c == '\\')
                {
                    auto escaped = get();
                    switch (escaped)
                    {
                    case '\\':
                    case '"':
                        text.push_back(escaped);
                        break;
                    case 'n':
                        text.push_back('\n');
                        break;
                    case 't':
                        text.push_back('\t');
                        break;
                    default:
                        fail(start, luisa::format("unsupported string escape '\\{}'", escaped));
                    }
                }
                else
                {
                    text.push_back(c);
                }
            }
            return Token{TokenKind::String, std::move(text), start};
        }

        luisa::string text;
        while (true)
        {
            c = peek();
            if (c == '\0' || is_space(c) || c == '[' || c == ']' || c == '"' || c == '#')
            {
                break;
            }
            text.push_back(get());
        }
        return Token{TokenKind::Word, std::move(text), start};
    }
};

[[nodiscard]] float& matrix_at(Matrix4& m, uint32_t row, uint32_t column) noexcept { return m[row * 4u + column]; }
[[nodiscard]] float matrix_at(const Matrix4& m, uint32_t row, uint32_t column) noexcept { return m[row * 4u + column]; }

[[nodiscard]] Matrix4 multiply(const Matrix4& lhs, const Matrix4& rhs) noexcept
{
    Matrix4 result{};
    for (auto row = 0u; row < 4u; row++)
    {
        for (auto column = 0u; column < 4u; column++)
        {
            for (auto i = 0u; i < 4u; i++)
            {
                matrix_at(result, row, column) += matrix_at(lhs, row, i) * matrix_at(rhs, i, column);
            }
        }
    }
    return result;
}

[[nodiscard]] float dot_host(float3 a, float3 b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }
[[nodiscard]] float3 cross_host(float3 a, float3 b) noexcept { return make_float3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }

[[nodiscard]] float3 normalize_host(float3 v, const Token& command, luisa::string_view what)
{
    auto length_squared = dot_host(v, v);
    if (length_squared < 1e-16f)
    {
        fail(command, luisa::format("{} must be non-zero", what));
    }
    auto inv_length = 1.0f / std::sqrt(length_squared);
    return v * inv_length;
}

class Parser
{
private:
    PbrtScene m_desc;
    luisa::vector<Token> m_tokens;
    size_t m_cursor{};

    enum class Block
    {
        Options,
        World,
    };

    struct AttributeState
    {
        MaterialBinding material;
        luisa::optional<AreaLightDesc> area_light;
        Matrix4 transform;
    };

    Block m_block{Block::Options};
    Matrix4 m_current_transform{identity_matrix4};
    MaterialBinding m_current_material;
    luisa::optional<AreaLightDesc> m_current_area_light;
    luisa::vector<AttributeState> m_attribute_stack;

public:
    Parser(PbrtScene desc, luisa::vector<Token> tokens) noexcept
        : m_desc{std::move(desc)}, m_tokens{std::move(tokens)} {}

    [[nodiscard]] PbrtScene parse()
    {
        while (!peek(TokenKind::End))
        {
            auto command = expect(TokenKind::Word, "expected PBRT command");
            parse_command(command);
        }
        if (m_block != Block::World)
        {
            fail(m_tokens[m_cursor], "End of file before WorldBegin");
        }
        if (!m_attribute_stack.empty())
        {
            fail(m_tokens[m_cursor], "missing AttributeEnd");
        }
        return std::move(m_desc);
    }

private:
    [[nodiscard]] const Token& current() const noexcept
    {
        return m_tokens[m_cursor];
    }

    [[nodiscard]] bool peek(TokenKind kind) const noexcept
    {
        return current().kind == kind;
    }

    [[nodiscard]] Token advance() noexcept
    {
        return m_tokens[m_cursor++];
    }

    [[nodiscard]] Token expect(TokenKind kind, luisa::string_view message)
    {
        if (!peek(kind))
        {
            fail(current(), message);
        }
        return advance();
    }

    [[nodiscard]] luisa::string expect_string(luisa::string_view context)
    {
        return expect(TokenKind::String, luisa::format("expected quoted string for {}", context)).text;
    }

    void expect_options(const Token& command)
    {
        if (m_block != Block::Options)
        {
            fail(command, luisa::format("'{}' is only supported before WorldBegin", command.text));
        }
    }

    void expect_world(const Token& command)
    {
        if (m_block != Block::World)
        {
            fail(command, luisa::format("'{}' is only supported after WorldBegin", command.text));
        }
    }

    [[nodiscard]] luisa::vector<RawParameter> parse_parameters()
    {
        luisa::vector<RawParameter> params;
        while (peek(TokenKind::String))
        {
            auto decl  = advance();
            auto split = decl.text.find_first_of(" \t");
            if (split == luisa::string::npos)
            {
                fail(decl, luisa::format("invalid parameter declaration '{}'", decl.text));
            }
            auto type     = decl.text.substr(0u, split);
            auto name_pos = decl.text.find_first_not_of(" \t", split);
            if (name_pos == luisa::string::npos)
            {
                fail(decl, luisa::format("missing parameter name in '{}'", decl.text));
            }
            auto name = decl.text.substr(name_pos);
            luisa::vector<RawValue> values;
            auto bracketed = peek(TokenKind::LBracket);
            if (bracketed)
            {
                (void)advance();
                while (!peek(TokenKind::RBracket))
                {
                    if (peek(TokenKind::End))
                    {
                        fail(current(), luisa::format("unterminated value list for parameter '{}'", decl.text));
                    }
                    if (peek(TokenKind::LBracket))
                    {
                        fail(current(), luisa::format("unexpected '[' inside parameter '{}'", decl.text));
                    }
                    auto value = advance();
                    values.emplace_back(RawValue{.source = value.loc, .text = std::move(value.text), .quoted = value.kind == TokenKind::String});
                }
                (void)expect(TokenKind::RBracket, "expected ']'");
            }
            else
            {
                if (!peek(TokenKind::Word) && !peek(TokenKind::String))
                {
                    fail(current(), luisa::format("expected value for parameter '{}'", decl.text));
                }
                auto value = advance();
                values.emplace_back(RawValue{.source = value.loc, .text = std::move(value.text), .quoted = value.kind == TokenKind::String});
            }
            params.emplace_back(RawParameter{
                .source    = decl.loc,
                .type      = std::move(type),
                .name      = std::move(name),
                .values    = std::move(values),
                .bracketed = bracketed,
            });
        }
        return params;
    }

    [[nodiscard]] const RawParameter* find_param(luisa::span<const RawParameter> params, luisa::string_view type, luisa::string_view name) const noexcept
    {
        for (auto&& p : params)
        {
            if (p.type == type && p.name == name)
            {
                return &p;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const RawParameter& require_param(luisa::span<const RawParameter> params, luisa::string_view type, luisa::string_view name, const Token& command) const
    {
        if (auto p = find_param(params, type, name))
        {
            return *p;
        }
        fail(command, luisa::format("missing parameter '\"{} {}\"'", type, name));
    }

    [[nodiscard]] float parse_float_token(const RawValue& token) const
    {
        if (token.quoted)
        {
            fail(token.source, luisa::format("expected float, got string '{}'", token.text));
        }
        try
        {
            size_t parsed_chars = 0u;
            auto v              = std::stof(std::string{token.text}, &parsed_chars);
            if (parsed_chars != token.text.size())
            {
                fail(token.source, luisa::format("invalid float '{}'", token.text));
            }
            return v;
        }
        catch (const std::exception&)
        {
            fail(token.source, luisa::format("invalid float '{}'", token.text));
        }
    }

    [[nodiscard]] float parse_float_token(const Token& token) const
    {
        return parse_float_token(RawValue{.source = token.loc, .text = token.text, .quoted = token.kind == TokenKind::String});
    }

    [[nodiscard]] float next_float(const Token& command, luisa::string_view context)
    {
        if (!peek(TokenKind::Word))
        {
            fail(command, luisa::format("{} expects numeric arguments", context));
        }
        return parse_float_token(advance());
    }

    [[nodiscard]] float3 next_float3(const Token& command, luisa::string_view context)
    {
        auto x = next_float(command, context);
        auto y = next_float(command, context);
        auto z = next_float(command, context);
        return make_float3(x, y, z);
    }

    void concat_transform(const Matrix4& transform) noexcept
    {
        m_current_transform = multiply(m_current_transform, transform);
    }

    [[nodiscard]] int parse_int_token(const RawValue& token) const
    {
        if (token.quoted)
        {
            fail(token.source, luisa::format("expected integer, got string '{}'", token.text));
        }
        int value   = 0;
        auto begin  = token.text.data();
        auto end    = begin + token.text.size();
        auto result = std::from_chars(begin, end, value);
        if (result.ec != std::errc{} || result.ptr != end)
        {
            fail(token.source, luisa::format("invalid integer '{}'", token.text));
        }
        return value;
    }

    [[nodiscard]] luisa::string parse_string_token(const RawValue& token) const
    {
        if (!token.quoted)
        {
            fail(token.source, luisa::format("expected string, got '{}'", token.text));
        }
        return token.text;
    }

    [[nodiscard]] uint one_uint(luisa::span<const RawParameter> params, luisa::string_view name, const Token& command, uint default_value) const
    {
        auto p = find_param(params, "integer", name);
        if (p == nullptr)
        {
            return default_value;
        }
        if (p->values.size() != 1u)
        {
            fail(p->source, luisa::format("'integer {}' expects exactly one value", name));
        }
        auto v = parse_int_token(p->values.front());
        if (v < 0)
        {
            fail(p->values.front().source, luisa::format("'integer {}' must be non-negative", name));
        }
        return static_cast<uint>(v);
    }

    [[nodiscard]] float one_float(luisa::span<const RawParameter> params, luisa::string_view name, const Token& command, float default_value) const
    {
        auto p = find_param(params, "float", name);
        if (p == nullptr)
        {
            return default_value;
        }
        if (p->values.size() != 1u)
        {
            fail(p->source, luisa::format("'float {}' expects exactly one value", name));
        }
        return parse_float_token(p->values.front());
    }

    [[nodiscard]] luisa::string one_string(luisa::span<const RawParameter> params, luisa::string_view name, const Token& command, luisa::string default_value) const
    {
        auto p = find_param(params, "string", name);
        if (p == nullptr)
        {
            return default_value;
        }
        if (p->values.size() != 1u)
        {
            fail(p->source, luisa::format("'string {}' expects exactly one value", name));
        }
        return parse_string_token(p->values.front());
    }

    [[nodiscard]] luisa::optional<luisa::string> optional_texture(
        luisa::span<const RawParameter> params, luisa::string_view name) const
    {
        auto p = find_param(params, "texture", name);
        if (p == nullptr)
        {
            return luisa::nullopt;
        }
        if (p->values.size() != 1u)
        {
            fail(p->source, luisa::format("'texture {}' expects exactly one value", name));
        }
        return parse_string_token(p->values.front());
    }

    [[nodiscard]] float3 rgb(luisa::span<const RawParameter> params, luisa::string_view name, const Token& command) const
    {
        auto&& p = require_param(params, "rgb", name, command);
        if (p.values.size() != 3u)
        {
            fail(p.source, luisa::format("'rgb {}' expects exactly three values", name));
        }
        return make_float3(parse_float_token(p.values[0u]),
                           parse_float_token(p.values[1u]),
                           parse_float_token(p.values[2u]));
    }

    [[nodiscard]] luisa::vector<float3> float3_array(luisa::span<const RawParameter> params, luisa::string_view type, luisa::string_view name, const Token& command) const
    {
        auto&& p = require_param(params, type, name, command);
        if (p.values.size() % 3u != 0u)
        {
            fail(p.source, luisa::format("'{} {}' value count must be a multiple of 3", type, name));
        }
        luisa::vector<float3> values;
        values.reserve(p.values.size() / 3u);
        for (auto i = 0u; i < p.values.size(); i += 3u)
        {
            values.emplace_back(make_float3(parse_float_token(p.values[i]),
                                            parse_float_token(p.values[i + 1u]),
                                            parse_float_token(p.values[i + 2u])));
        }
        return values;
    }

    [[nodiscard]] luisa::vector<float2> optional_float2_array(luisa::span<const RawParameter> params, luisa::string_view type, luisa::string_view name) const
    {
        auto p = find_param(params, type, name);
        if (p == nullptr)
        {
            return {};
        }
        if (p->values.size() % 2u != 0u)
        {
            fail(p->source, luisa::format("'{} {}' value count must be a multiple of 2", type, name));
        }
        luisa::vector<float2> values;
        values.reserve(p->values.size() / 2u);
        for (auto i = 0u; i < p->values.size(); i += 2u)
        {
            values.emplace_back(make_float2(parse_float_token(p->values[i]),
                                            parse_float_token(p->values[i + 1u])));
        }
        return values;
    }

    [[nodiscard]] luisa::vector<uint3> triangle_indices(luisa::span<const RawParameter> params, const Token& command, size_t vertex_count) const
    {
        auto&& p = require_param(params, "integer", "indices", command);
        if (p.values.size() % 3u != 0u)
        {
            fail(p.source, "'integer indices' value count must be a multiple of 3");
        }
        luisa::vector<uint3> values;
        values.reserve(p.values.size() / 3u);
        for (auto i = 0u; i < p.values.size(); i += 3u)
        {
            auto i0 = parse_int_token(p.values[i]);
            auto i1 = parse_int_token(p.values[i + 1u]);
            auto i2 = parse_int_token(p.values[i + 2u]);
            if (i0 < 0 || i1 < 0 || i2 < 0 ||
                static_cast<size_t>(i0) >= vertex_count ||
                static_cast<size_t>(i1) >= vertex_count ||
                static_cast<size_t>(i2) >= vertex_count)
            {
                fail(p.values[i].source, "triangle index out of bounds");
            }
            values.emplace_back(make_uint3(static_cast<uint>(i0),
                                           static_cast<uint>(i1),
                                           static_cast<uint>(i2)));
        }
        return values;
    }

    void parse_command(const Token& command)
    {
        if (command.text == "Integrator")
        {
            parse_integrator(command);
        }
        else if (command.text == "Transform")
        {
            parse_transform(command);
        }
        else if (command.text == "Scale")
        {
            parse_scale(command);
        }
        else if (command.text == "Translate")
        {
            parse_translate(command);
        }
        else if (command.text == "Rotate")
        {
            parse_rotate(command);
        }
        else if (command.text == "LookAt")
        {
            parse_look_at(command);
        }
        else if (command.text == "Sampler")
        {
            parse_sampler(command);
        }
        else if (command.text == "PixelFilter")
        {
            parse_filter(command);
        }
        else if (command.text == "Film")
        {
            parse_film(command);
        }
        else if (command.text == "Camera")
        {
            parse_camera(command);
        }
        else if (command.text == "WorldBegin")
        {
            parse_world_begin(command);
        }
        else if (command.text == "MakeNamedMaterial")
        {
            parse_make_named_material(command);
        }
        else if (command.text == "Material")
        {
            parse_material(command);
        }
        else if (command.text == "Texture")
        {
            parse_texture(command);
        }
        else if (command.text == "NamedMaterial")
        {
            parse_named_material(command);
        }
        else if (command.text == "Shape")
        {
            parse_shape(command);
        }
        else if (command.text == "AttributeBegin")
        {
            parse_attribute_begin(command);
        }
        else if (command.text == "AttributeEnd")
        {
            parse_attribute_end(command);
        }
        else if (command.text == "AreaLightSource")
        {
            parse_area_light_source(command);
        }
        else
        {
            fail(command, luisa::format("unsupported PBRT command '{}'", command.text));
        }
    }

    void parse_integrator(const Token& command)
    {
        expect_options(command);
        auto type = expect_string("Integrator type");
        if (type != "path")
        {
            fail(command, luisa::format("unsupported Integrator '{}'", type));
        }
        auto params                  = parse_parameters();
        m_desc.integrator.source     = command.loc;
        m_desc.integrator.type       = IntegratorDesc::Type::Path;
        m_desc.integrator.max_depth  = one_uint(params, "maxdepth", command, 10u);
        m_desc.integrator.parameters = std::move(params);
    }

    void parse_transform(const Token& command)
    {
        Matrix4 transform{};
        (void)expect(TokenKind::LBracket, "expected '[' after Transform");
        for (auto i = 0u; i < 16u; i++)
        {
            if (peek(TokenKind::RBracket) || peek(TokenKind::End))
            {
                fail(command, "Transform expects exactly 16 floats");
            }
            auto row                          = i % 4u;
            auto column                       = i / 4u;
            matrix_at(transform, row, column) = parse_float_token(advance());
        }
        (void)expect(TokenKind::RBracket, "Transform expects exactly 16 floats");
        m_current_transform = transform;
    }

    void parse_scale(const Token& command)
    {
        Matrix4 transform{identity_matrix4};
        matrix_at(transform, 0u, 0u) = next_float(command, "Scale");
        matrix_at(transform, 1u, 1u) = next_float(command, "Scale");
        matrix_at(transform, 2u, 2u) = next_float(command, "Scale");
        concat_transform(transform);
    }

    void parse_translate(const Token& command)
    {
        Matrix4 transform{identity_matrix4};
        matrix_at(transform, 0u, 3u) = next_float(command, "Translate");
        matrix_at(transform, 1u, 3u) = next_float(command, "Translate");
        matrix_at(transform, 2u, 3u) = next_float(command, "Translate");
        concat_transform(transform);
    }

    void parse_rotate(const Token& command)
    {
        constexpr auto pi  = 3.14159265358979323846f;
        auto angle         = next_float(command, "Rotate") * (pi / 180.0f);
        auto axis          = normalize_host(next_float3(command, "Rotate"), command, "Rotate axis");
        auto sin_angle     = std::sin(angle);
        auto cos_angle     = std::cos(angle);
        auto one_minus_cos = 1.0f - cos_angle;
        Matrix4 transform{identity_matrix4};
        matrix_at(transform, 0u, 0u) = axis.x * axis.x * one_minus_cos + cos_angle;
        matrix_at(transform, 0u, 1u) = axis.x * axis.y * one_minus_cos - axis.z * sin_angle;
        matrix_at(transform, 0u, 2u) = axis.x * axis.z * one_minus_cos + axis.y * sin_angle;
        matrix_at(transform, 1u, 0u) = axis.y * axis.x * one_minus_cos + axis.z * sin_angle;
        matrix_at(transform, 1u, 1u) = axis.y * axis.y * one_minus_cos + cos_angle;
        matrix_at(transform, 1u, 2u) = axis.y * axis.z * one_minus_cos - axis.x * sin_angle;
        matrix_at(transform, 2u, 0u) = axis.z * axis.x * one_minus_cos - axis.y * sin_angle;
        matrix_at(transform, 2u, 1u) = axis.z * axis.y * one_minus_cos + axis.x * sin_angle;
        matrix_at(transform, 2u, 2u) = axis.z * axis.z * one_minus_cos + cos_angle;
        concat_transform(transform);
    }

    void parse_look_at(const Token& command)
    {
        auto eye       = next_float3(command, "LookAt");
        auto target    = next_float3(command, "LookAt");
        auto up        = normalize_host(next_float3(command, "LookAt"), command, "LookAt up vector");
        auto direction = normalize_host(target - eye, command, "LookAt direction");
        auto right     = normalize_host(cross_host(up, direction), command, "LookAt right vector");
        auto new_up    = cross_host(direction, right);
        Matrix4 camera_from_world{identity_matrix4};
        matrix_at(camera_from_world, 0u, 0u) = right.x;
        matrix_at(camera_from_world, 0u, 1u) = right.y;
        matrix_at(camera_from_world, 0u, 2u) = right.z;
        matrix_at(camera_from_world, 0u, 3u) = -dot_host(right, eye);
        matrix_at(camera_from_world, 1u, 0u) = new_up.x;
        matrix_at(camera_from_world, 1u, 1u) = new_up.y;
        matrix_at(camera_from_world, 1u, 2u) = new_up.z;
        matrix_at(camera_from_world, 1u, 3u) = -dot_host(new_up, eye);
        matrix_at(camera_from_world, 2u, 0u) = direction.x;
        matrix_at(camera_from_world, 2u, 1u) = direction.y;
        matrix_at(camera_from_world, 2u, 2u) = direction.z;
        matrix_at(camera_from_world, 2u, 3u) = -dot_host(direction, eye);
        concat_transform(camera_from_world);
    }

    void parse_sampler(const Token& command)
    {
        expect_options(command);
        auto type = expect_string("Sampler type");
        if (type == "independent")
        {
            m_desc.sampler.type = SamplerDesc::Type::Independent;
        }
        else if (type == "halton")
        {
            m_desc.sampler.type = SamplerDesc::Type::Halton;
        }
        else
        {
            fail(command, luisa::format("unknown Sampler '{}'", type));
        }
        auto params                  = parse_parameters();
        m_desc.sampler.source        = command.loc;
        m_desc.sampler.pixel_samples = one_uint(params, "pixelsamples", command, 1u);
        m_desc.sampler.parameters    = std::move(params);
    }

    void parse_filter(const Token& command)
    {
        expect_options(command);
        m_desc.filter.source = command.loc;
        auto type            = expect_string("PixelFilter type");
        if (type == "triangle")
        {
            m_desc.filter.type = FilterDesc::Type::Triangle;
        }
        else if (type == "gaussian")
        {
            m_desc.filter.type = FilterDesc::Type::Gaussian;
        }
        else
        {
            fail(command, luisa::format("unsupported PixelFilter '{}'", type));
        }
        auto params              = parse_parameters();
        m_desc.filter.radius     = make_float2(one_float(params, "xradius", command, 1.0f),
                                               one_float(params, "yradius", command, 1.0f));
        m_desc.filter.parameters = std::move(params);
    }

    void parse_film(const Token& command)
    {
        expect_options(command);
        auto type = expect_string("Film type");
        if (type != "rgb")
        {
            fail(command, luisa::format("unsupported Film '{}'", type));
        }
        auto params            = parse_parameters();
        m_desc.film.source     = command.loc;
        m_desc.film.type       = FilmDesc::Type::RGB;
        m_desc.film.resolution = make_uint2(one_uint(params, "xresolution", command, 1024u),
                                            one_uint(params, "yresolution", command, 1024u));
        auto filename          = one_string(params, "filename", command, {});
        if (!filename.empty())
        {
            m_desc.film.filename = std::filesystem::path{filename};
        }
        m_desc.film.parameters = std::move(params);
    }

    void parse_camera(const Token& command)
    {
        expect_options(command);
        auto type = expect_string("Camera type");
        if (type != "perspective")
        {
            fail(command, luisa::format("unsupported Camera '{}'", type));
        }
        auto params                  = parse_parameters();
        m_desc.camera.source         = command.loc;
        m_desc.camera.type           = CameraDesc::Type::Perspective;
        m_desc.camera.fov            = one_float(params, "fov", command, 45.0f);
        m_desc.camera.pbrt_transform = m_current_transform;
        m_desc.camera.parameters     = std::move(params);
    }

    void parse_world_begin(const Token& command)
    {
        expect_options(command);
        if (!parse_parameters().empty())
        {
            fail(command, "WorldBegin does not take parameters");
        }
        m_block             = Block::World;
        m_current_transform = identity_matrix4;
    }

    void parse_make_named_material(const Token& command)
    {
        expect_world(command);
        auto name   = expect_string("named material name");
        auto params = parse_parameters();
        if (m_desc.named_materials.find(name) != m_desc.named_materials.end())
        {
            fail(command, luisa::format("named material '{}' is redefined", name));
        }
        auto type = one_string(params, "type", command, {});
        MaterialDesc::Type material_type;
        if (type == "diffuse")
        {
            material_type = MaterialDesc::Type::Diffuse;
        }
        else if (type == "coateddiffuse")
        {
            material_type = MaterialDesc::Type::CoatedDiffuse;
        }
        else
        {
            fail(command, luisa::format("unknown named material type '{}'", type));
        }
        auto reflectance = make_float3(0.0f);
        auto reflectance_rgb = find_param(params, "rgb", "reflectance");
        auto reflectance_texture = optional_texture(params, "reflectance");
        if (reflectance_rgb != nullptr && reflectance_texture)
        {
            fail(reflectance_rgb->source, "material reflectance cannot specify both rgb and texture values");
        }
        if (reflectance_rgb != nullptr)
        {
            reflectance = rgb(params, "reflectance", command);
        }
        m_desc.named_materials.emplace(std::move(name), MaterialDesc{
                                                            .source              = command.loc,
                                                            .type                = material_type,
                                                            .reflectance         = reflectance,
                                                            .reflectance_texture = std::move(reflectance_texture),
                                                            .parameters          = std::move(params),
                                                        });
    }

    void parse_material(const Token& command)
    {
        expect_world(command);
        auto type   = expect_string("Material type");
        auto params = parse_parameters();
        MaterialDesc::Type material_type;
        if (type == "diffuse")
        {
            material_type = MaterialDesc::Type::Diffuse;
        }
        else if (type == "coateddiffuse")
        {
            material_type = MaterialDesc::Type::CoatedDiffuse;
        }
        else
        {
            fail(command, luisa::format("unknown Material '{}'", type));
        }
        auto reflectance = make_float3(0.0f);
        auto reflectance_rgb = find_param(params, "rgb", "reflectance");
        auto reflectance_texture = optional_texture(params, "reflectance");
        if (reflectance_rgb != nullptr && reflectance_texture)
        {
            fail(reflectance_rgb->source, "material reflectance cannot specify both rgb and texture values");
        }
        if (reflectance_rgb != nullptr)
        {
            reflectance = rgb(params, "reflectance", command);
        }
        auto index = static_cast<uint>(m_desc.materials.size());
        m_desc.materials.emplace_back(MaterialDesc{
            .source              = command.loc,
            .type                = material_type,
            .reflectance         = reflectance,
            .reflectance_texture = std::move(reflectance_texture),
            .parameters          = std::move(params),
        });
        m_current_material = MaterialBinding{.inline_index = index};
    }

    void parse_texture(const Token& command)
    {
        expect_world(command);
        auto name       = expect_string("Texture name");
        auto value_type = expect_string("Texture value type");
        auto type       = expect_string("Texture implementation");
        auto params     = parse_parameters();
        TextureDesc desc{.source = command.loc, .name = std::move(name), .parameters = std::move(params)};
        if (value_type == "float")
        {
            desc.value_type = TextureDesc::ValueType::Float;
        }
        else if (value_type == "spectrum")
        {
            desc.value_type = TextureDesc::ValueType::Spectrum;
        }
        else
        {
            fail(command, luisa::format("unknown Texture value type '{}'", value_type));
        }
        if (type == "imagemap")
        {
            desc.type = TextureDesc::Type::ImageMap;
        }
        else if (type == "constant")
        {
            desc.type = TextureDesc::Type::Constant;
        }
        else if (type == "scale")
        {
            desc.type = TextureDesc::Type::Scale;
        }
        else
        {
            fail(command, luisa::format("unknown Texture '{}'", type));
        }
        if (desc.type == TextureDesc::Type::ImageMap)
        {
            auto filename = one_string(desc.parameters, "filename", command, {});
            if (filename.empty())
            {
                fail(command, "imagemap texture requires a non-empty 'string filename' parameter");
            }
            desc.filename = std::filesystem::path{filename};
            desc.uv_scale = make_float2(
                one_float(desc.parameters, "uscale", command, 1.0f),
                one_float(desc.parameters, "vscale", command, 1.0f));
        }
        else if (desc.type == TextureDesc::Type::Constant)
        {
            if (desc.value_type != TextureDesc::ValueType::Float)
            {
                fail(command, "only float constant textures are currently supported");
            }
            (void)require_param(desc.parameters, "float", "value", command);
            desc.constant_value = one_float(desc.parameters, "value", command, 0.0f);
        }
        else if (desc.type == TextureDesc::Type::Scale)
        {
            if (desc.value_type != TextureDesc::ValueType::Float)
            {
                fail(command, "only float scale textures are currently supported");
            }
            auto tex = optional_texture(desc.parameters, "tex");
            auto scale = optional_texture(desc.parameters, "scale");
            if (!tex || !scale)
            {
                fail(command, "scale texture requires 'texture tex' and 'texture scale' parameters");
            }
            desc.tex   = std::move(*tex);
            desc.scale = std::move(*scale);
        }

        for (auto i = 0u; i < desc.parameters.size(); i++)
        {
            auto&& p = desc.parameters[i];
            auto supported = desc.type == TextureDesc::Type::ImageMap
                                 ? (p.type == "string" && p.name == "filename") ||
                                       (p.type == "float" && (p.name == "uscale" || p.name == "vscale"))
                                 : desc.type == TextureDesc::Type::Constant
                                       ? p.type == "float" && p.name == "value"
                                       : p.type == "texture" && (p.name == "tex" || p.name == "scale");
            if (!supported)
            {
                fail(p.source, luisa::format("unsupported parameter '\"{} {}\"' for this texture", p.type, p.name));
            }
            for (auto j = 0u; j < i; j++)
            {
                if (desc.parameters[j].type == p.type && desc.parameters[j].name == p.name)
                {
                    fail(p.source, luisa::format("duplicate texture parameter '\"{} {}\"'", p.type, p.name));
                }
            }
        }
        if (desc.type != TextureDesc::Type::ImageMap &&
            !std::isfinite(desc.constant_value))
        {
            fail(command, "texture constant value must be finite");
        }
        if (!std::isfinite(desc.uv_scale.x) || !std::isfinite(desc.uv_scale.y))
        {
            fail(command, "imagemap texture scale must be finite");
        }
        m_desc.textures.emplace_back(std::move(desc));
    }

    void parse_named_material(const Token& command)
    {
        expect_world(command);
        auto name = expect_string("named material reference");
        if (!parse_parameters().empty())
        {
            fail(command, "NamedMaterial does not take parameters");
        }
        m_current_material = MaterialBinding{.named = std::move(name)};
    }

    void parse_area_light_source(const Token& command)
    {
        expect_world(command);
        auto type = expect_string("AreaLightSource type");
        if (type != "diffuse")
        {
            fail(command, luisa::format("unsupported AreaLightSource '{}'", type));
        }
        auto params = parse_parameters();
        m_current_area_light.emplace(AreaLightDesc{
            .source     = command.loc,
            .type       = AreaLightDesc::Type::Diffuse,
            .emission   = rgb(params, "L", command),
            .parameters = std::move(params),
        });
    }

    void parse_attribute_begin(const Token& command)
    {
        expect_world(command);
        if (!parse_parameters().empty())
        {
            fail(command, "AttributeBegin does not take parameters");
        }
        m_attribute_stack.emplace_back(AttributeState{
            .material   = m_current_material,
            .area_light = m_current_area_light,
            .transform  = m_current_transform,
        });
    }

    void parse_attribute_end(const Token& command)
    {
        expect_world(command);
        if (!parse_parameters().empty())
        {
            fail(command, "AttributeEnd does not take parameters");
        }
        if (m_attribute_stack.empty())
        {
            fail(command, "unmatched AttributeEnd");
        }
        auto state = std::move(m_attribute_stack.back());
        m_attribute_stack.pop_back();
        m_current_material   = std::move(state.material);
        m_current_area_light = std::move(state.area_light);
        m_current_transform  = state.transform;
    }

    void parse_shape(const Token& command)
    {
        expect_world(command);
        auto type   = expect_string("Shape type");
        auto params = parse_parameters();
        ShapeDesc shape{
            .source         = command.loc,
            .parameters     = params,
            .material       = m_current_material,
            .area_light     = m_current_area_light,
            .pbrt_transform = m_current_transform,
        };
        if (type == "sphere")
        {
            shape.type = ShapeDesc::Type::Sphere;
            auto radius_count = 0u;
            auto subdivision_count = 0u;
            for (auto&& param : params)
            {
                if (param.name == "zmin" || param.name == "zmax" || param.name == "phimax")
                {
                    fail(param.source, luisa::format("PBRT sphere clipping parameter '{}' is not supported", param.name));
                }
                if (param.name == "radius")
                {
                    if (param.type != "float")
                    {
                        fail(param.source, "sphere parameter 'radius' must have type 'float'");
                    }
                    if (radius_count++ != 0u)
                    {
                        fail(param.source, "duplicate parameter 'float radius'");
                    }
                }
                else if (param.name == "subdivision")
                {
                    if (param.type != "integer")
                    {
                        fail(param.source, "sphere parameter 'subdivision' must have type 'integer'");
                    }
                    if (subdivision_count++ != 0u)
                    {
                        fail(param.source, "duplicate parameter 'integer subdivision'");
                    }
                }
                else
                {
                    fail(param.source, luisa::format("unsupported sphere parameter '{} {}'", param.type, param.name));
                }
            }
            shape.radius             = one_float(params, "radius", command, 1.0f);
            shape.sphere_subdivision = one_uint(params, "subdivision", command, ShapeDesc::sphere_default_subdivision);
            if (!std::isfinite(shape.radius) || shape.radius <= 0.0f)
            {
                fail(command, "sphere radius must be finite and positive");
            }
            if (shape.sphere_subdivision > ShapeDesc::sphere_max_subdivision)
            {
                fail(command, luisa::format("sphere subdivision level must not exceed {}", ShapeDesc::sphere_max_subdivision));
            }
        }
        else if (type == "plymesh")
        {
            shape.type = ShapeDesc::Type::PlyMesh;
            const RawParameter* filename_param = nullptr;
            for (auto&& param : params)
            {
                if (param.type == "string" && param.name == "filename")
                {
                    if (filename_param != nullptr)
                    {
                        fail(param.source, "duplicate parameter 'string filename'");
                    }
                    filename_param = &param;
                }
            }
            auto filename = one_string(params, "filename", command, {});
            if (filename.empty())
            {
                fail(command, "plymesh requires a non-empty 'string filename'");
            }
            shape.filename = std::filesystem::path{std::move(filename)};
        }
        else if (type == "trianglemesh")
        {
            shape.type     = ShapeDesc::Type::TriangleMesh;
            auto positions = float3_array(params, "point3", "P", command);
            auto normals   = float3_array(params, "normal", "N", command);
            auto uvs       = optional_float2_array(params, "point2", "uv");
            if (!normals.empty() && normals.size() != positions.size())
            {
                fail(command, "'normal N' count must match 'point3 P' count");
            }
            if (!uvs.empty() && uvs.size() != positions.size())
            {
                fail(command, "'point2 uv' count must match 'point3 P' count");
            }
            auto indices = triangle_indices(params, command, positions.size());

            shape.mesh_index = static_cast<uint>(m_desc.meshes.size());
            m_desc.meshes.emplace_back(MeshDesc{
                .source    = command.loc,
                .positions = std::move(positions),
                .normals   = std::move(normals),
                .uvs       = std::move(uvs),
                .indices   = std::move(indices),
            });
        }
        else
        {
            fail(command, luisa::format("unknown Shape '{}'", type));
        }
        m_desc.shapes.emplace_back(std::move(shape));
    }
};

[[nodiscard]] luisa::string read_file(const std::filesystem::path& path)
{
    std::ifstream input{path};
    if (!input)
    {
        auto s = luisa::format("Failed to open PBRT scene '{}'.", path.string());
        throw std::runtime_error{s.c_str()};
    }
    std::ostringstream ss;
    ss << input.rdbuf();
    auto source = ss.str();
    return luisa::string{source.c_str()};
}

} // namespace

PbrtScene PbrtParser::parse(const std::filesystem::path& path)
{
    auto source_path = std::filesystem::absolute(path);
    auto source      = read_file(source_path);
    Tokenizer tokenizer{source_path, std::move(source)};
    PbrtScene desc{};
    desc.source_path       = source_path;
    desc.camera.source     = SourceLocation{source_path};
    desc.film.source       = SourceLocation{source_path};
    desc.integrator.source = SourceLocation{source_path};
    desc.sampler.source    = SourceLocation{source_path};
    desc.filter.source     = SourceLocation{source_path};
    Parser parser{std::move(desc), tokenizer.tokenize()};
    return parser.parse();
}

} // namespace Yutrel
