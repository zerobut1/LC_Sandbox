#include "pbrt_scene_loader.h"

#include <charconv>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace Yutrel
{
namespace
{

struct SourceLocation
{
    std::filesystem::path file;
    uint line{1u};
    uint column{1u};

    [[nodiscard]] luisa::string string() const
    {
        return luisa::format("{}:{}:{}", file.string(), line, column);
    }
};

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
    auto s = luisa::format("{}: {}", loc.string(), message);
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

struct RawParameter
{
    luisa::string type;
    luisa::string name;
    luisa::vector<Token> values;
    SourceLocation loc;
};

class Parser
{
private:
    SceneDescription m_desc;
    luisa::vector<Token> m_tokens;
    size_t m_cursor{};

    enum class Block
    {
        Options,
        World,
    };

    struct AttributeState
    {
        luisa::string material_name;
        luisa::optional<AreaLightDesc> area_light;
    };

    Block m_block{Block::Options};
    std::array<float, 16u> m_current_transform{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f};
    luisa::string m_current_material;
    luisa::optional<AreaLightDesc> m_current_area_light;
    luisa::vector<AttributeState> m_attribute_stack;

public:
    Parser(SceneDescription desc, luisa::vector<Token> tokens) noexcept
        : m_desc{std::move(desc)}, m_tokens{std::move(tokens)} {}

    [[nodiscard]] SceneDescription parse()
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
            auto decl = advance();
            auto split = decl.text.find_first_of(" \t");
            if (split == luisa::string::npos)
            {
                fail(decl, luisa::format("invalid parameter declaration '{}'", decl.text));
            }
            auto type = decl.text.substr(0u, split);
            auto name_pos = decl.text.find_first_not_of(" \t", split);
            if (name_pos == luisa::string::npos)
            {
                fail(decl, luisa::format("missing parameter name in '{}'", decl.text));
            }
            auto name = decl.text.substr(name_pos);
            (void)expect(TokenKind::LBracket, luisa::format("expected '[' after parameter '{}'", decl.text));
            luisa::vector<Token> values;
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
                values.emplace_back(advance());
            }
            (void)expect(TokenKind::RBracket, "expected ']'");
            params.emplace_back(RawParameter{
                .type = std::move(type),
                .name = std::move(name),
                .values = std::move(values),
                .loc = decl.loc,
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

    [[nodiscard]] float parse_float_token(const Token& token) const
    {
        if (token.kind != TokenKind::Word)
        {
            fail(token, luisa::format("expected float, got string '{}'", token.text));
        }
        try
        {
            size_t parsed_chars = 0u;
            auto v = std::stof(std::string{token.text}, &parsed_chars);
            if (parsed_chars != token.text.size())
            {
                fail(token, luisa::format("invalid float '{}'", token.text));
            }
            return v;
        }
        catch (const std::exception&)
        {
            fail(token, luisa::format("invalid float '{}'", token.text));
        }
    }

    [[nodiscard]] int parse_int_token(const Token& token) const
    {
        if (token.kind != TokenKind::Word)
        {
            fail(token, luisa::format("expected integer, got string '{}'", token.text));
        }
        int value = 0;
        auto begin = token.text.data();
        auto end = begin + token.text.size();
        auto result = std::from_chars(begin, end, value);
        if (result.ec != std::errc{} || result.ptr != end)
        {
            fail(token, luisa::format("invalid integer '{}'", token.text));
        }
        return value;
    }

    [[nodiscard]] luisa::string parse_string_token(const Token& token) const
    {
        if (token.kind != TokenKind::String)
        {
            fail(token, luisa::format("expected string, got '{}'", token.text));
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
            fail(p->loc, luisa::format("'integer {}' expects exactly one value", name));
        }
        auto v = parse_int_token(p->values.front());
        if (v < 0)
        {
            fail(p->values.front(), luisa::format("'integer {}' must be non-negative", name));
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
            fail(p->loc, luisa::format("'float {}' expects exactly one value", name));
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
            fail(p->loc, luisa::format("'string {}' expects exactly one value", name));
        }
        return parse_string_token(p->values.front());
    }

    [[nodiscard]] float3 rgb(luisa::span<const RawParameter> params, luisa::string_view name, const Token& command) const
    {
        auto&& p = require_param(params, "rgb", name, command);
        if (p.values.size() != 3u)
        {
            fail(p.loc, luisa::format("'rgb {}' expects exactly three values", name));
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
            fail(p.loc, luisa::format("'{} {}' value count must be a multiple of 3", type, name));
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
            fail(p->loc, luisa::format("'{} {}' value count must be a multiple of 2", type, name));
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
            fail(p.loc, "'integer indices' value count must be a multiple of 3");
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
                fail(p.values[i], "triangle index out of bounds");
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
        auto params = parse_parameters();
        m_desc.integrator.type = IntegratorDesc::Type::Path;
        m_desc.integrator.max_depth = one_uint(params, "maxdepth", command, 10u);
    }

    void parse_transform(const Token& command)
    {
        expect_options(command);
        (void)expect(TokenKind::LBracket, "expected '[' after Transform");
        for (auto i = 0u; i < 16u; i++)
        {
            if (peek(TokenKind::RBracket) || peek(TokenKind::End))
            {
                fail(command, "Transform expects exactly 16 floats");
            }
            m_current_transform[i] = parse_float_token(advance());
        }
        (void)expect(TokenKind::RBracket, "Transform expects exactly 16 floats");
    }

    void parse_sampler(const Token& command)
    {
        expect_options(command);
        auto type = expect_string("Sampler type");
        if (type != "sobol")
        {
            fail(command, luisa::format("unsupported Sampler '{}'", type));
        }
        auto params = parse_parameters();
        m_desc.sampler.type = SamplerDesc::Type::Sobol;
        m_desc.sampler.pixel_samples = one_uint(params, "pixelsamples", command, 1u);
    }

    void parse_filter(const Token& command)
    {
        expect_options(command);
        auto type = expect_string("PixelFilter type");
        if (type != "triangle")
        {
            fail(command, luisa::format("unsupported PixelFilter '{}'", type));
        }
        auto params = parse_parameters();
        m_desc.filter.type = FilterDesc::Type::Triangle;
        m_desc.filter.radius = make_float2(one_float(params, "xradius", command, 1.0f),
                                           one_float(params, "yradius", command, 1.0f));
    }

    void parse_film(const Token& command)
    {
        expect_options(command);
        auto type = expect_string("Film type");
        if (type != "rgb")
        {
            fail(command, luisa::format("unsupported Film '{}'", type));
        }
        auto params = parse_parameters();
        m_desc.film.type = FilmDesc::Type::RGB;
        m_desc.film.resolution = make_uint2(one_uint(params, "xresolution", command, 1024u),
                                            one_uint(params, "yresolution", command, 1024u));
        auto filename = one_string(params, "filename", command, {});
        if (!filename.empty())
        {
            m_desc.film.filename = std::filesystem::path{filename};
        }
    }

    void parse_camera(const Token& command)
    {
        expect_options(command);
        auto type = expect_string("Camera type");
        if (type != "perspective")
        {
            fail(command, luisa::format("unsupported Camera '{}'", type));
        }
        auto params = parse_parameters();
        m_desc.camera.type = CameraDesc::Type::Perspective;
        m_desc.camera.fov = one_float(params, "fov", command, 45.0f);
        m_desc.camera.pbrt_transform = m_current_transform;
    }

    void parse_world_begin(const Token& command)
    {
        expect_options(command);
        if (!parse_parameters().empty())
        {
            fail(command, "WorldBegin does not take parameters");
        }
        m_block = Block::World;
        m_current_transform = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f};
    }

    void parse_make_named_material(const Token& command)
    {
        expect_world(command);
        auto name = expect_string("named material name");
        auto params = parse_parameters();
        if (m_desc.named_materials.find(name) != m_desc.named_materials.end())
        {
            fail(command, luisa::format("named material '{}' is redefined", name));
        }
        auto type = one_string(params, "type", command, {});
        if (type != "diffuse")
        {
            fail(command, luisa::format("unsupported named material type '{}'", type));
        }
        m_desc.named_materials.emplace(std::move(name), MaterialDesc{
            .type = MaterialDesc::Type::Diffuse,
            .reflectance = rgb(params, "reflectance", command),
        });
    }

    void parse_named_material(const Token& command)
    {
        expect_world(command);
        auto name = expect_string("named material reference");
        if (!parse_parameters().empty())
        {
            fail(command, "NamedMaterial does not take parameters");
        }
        if (m_desc.named_materials.find(name) == m_desc.named_materials.end())
        {
            fail(command, luisa::format("NamedMaterial references undefined material '{}'", name));
        }
        m_current_material = std::move(name);
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
            .type = AreaLightDesc::Type::Diffuse,
            .emission = rgb(params, "L", command),
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
            .material_name = m_current_material,
            .area_light = m_current_area_light,
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
        m_current_material = std::move(state.material_name);
        m_current_area_light = std::move(state.area_light);
    }

    void parse_shape(const Token& command)
    {
        expect_world(command);
        auto type = expect_string("Shape type");
        if (type != "trianglemesh")
        {
            fail(command, luisa::format("unsupported Shape '{}'", type));
        }
        if (m_current_material.empty())
        {
            fail(command, "Shape has no current NamedMaterial");
        }
        if (m_desc.named_materials.find(m_current_material) == m_desc.named_materials.end())
        {
            fail(command, luisa::format("Shape references undefined material '{}'", m_current_material));
        }

        auto params = parse_parameters();
        auto positions = float3_array(params, "point3", "P", command);
        auto normals = float3_array(params, "normal", "N", command);
        auto uvs = optional_float2_array(params, "point2", "uv");
        if (!normals.empty() && normals.size() != positions.size())
        {
            fail(command, "'normal N' count must match 'point3 P' count");
        }
        if (!uvs.empty() && uvs.size() != positions.size())
        {
            fail(command, "'point2 uv' count must match 'point3 P' count");
        }
        auto indices = triangle_indices(params, command, positions.size());

        auto mesh_index = static_cast<uint>(m_desc.meshes.size());
        m_desc.meshes.emplace_back(MeshDesc{
            .positions = std::move(positions),
            .normals = std::move(normals),
            .uvs = std::move(uvs),
            .indices = std::move(indices),
        });
        m_desc.shapes.emplace_back(ShapeDesc{
            .mesh_index = mesh_index,
            .material_name = m_current_material,
            .area_light = m_current_area_light,
        });
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

SceneDescription PbrtSceneLoader::load(const std::filesystem::path& path)
{
    auto source_path = std::filesystem::absolute(path);
    auto source = read_file(source_path);
    Tokenizer tokenizer{source_path, std::move(source)};
    SceneDescription desc{};
    desc.source_path = source_path;
    Parser parser{std::move(desc), tokenizer.tokenize()};
    return parser.parse();
}

} // namespace Yutrel
