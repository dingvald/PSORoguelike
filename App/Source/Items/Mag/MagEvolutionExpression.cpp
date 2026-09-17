#include "Items/Mag/MagEvolutionExpression.h"

#include "Components/MagComponent.h"

#include <cctype>
#include <optional>
#include <vector>

namespace psr {

namespace {

    enum class TokenKind
    {
        Number,
        Identifier,
        Plus,
        Minus,
        Star,
        Slash,
        LParen,
        RParen,
        Lt,
        Gt,
        Le,
        Ge,
        Eq,
        Ne,
        End
    };

    struct Token
    {
        TokenKind kind = TokenKind::End;
        double number = 0.0;
        std::string identifier;
    };

    // Turns condition into a flat token stream; an unrecognized character
    // yields a single End token, which ParseCondition below treats as a
    // parse failure (see EvaluateMagEvolutionCondition's "false on malformed
    // input" contract).
    std::vector<Token> Tokenize(const std::string& condition)
    {
        std::vector<Token> tokens;
        std::size_t i = 0;
        const std::size_t n = condition.size();

        while (i < n)
        {
            const char c = condition[i];
            if (std::isspace(static_cast<unsigned char>(c)))
            {
                ++i;
                continue;
            }

            if (std::isdigit(static_cast<unsigned char>(c)) || c == '.')
            {
                std::size_t start = i;
                while (i < n && (std::isdigit(static_cast<unsigned char>(condition[i])) || condition[i] == '.'))
                    ++i;
                Token token;
                token.kind = TokenKind::Number;
                token.number = std::stod(condition.substr(start, i - start));
                tokens.push_back(token);
                continue;
            }

            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_')
            {
                std::size_t start = i;
                while (i < n && (std::isalnum(static_cast<unsigned char>(condition[i])) || condition[i] == '_'))
                    ++i;
                Token token;
                token.kind = TokenKind::Identifier;
                token.identifier = condition.substr(start, i - start);
                for (char& ch : token.identifier)
                    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                tokens.push_back(token);
                continue;
            }

            switch (c)
            {
            case '+':
                tokens.push_back({TokenKind::Plus});
                ++i;
                continue;
            case '-':
                tokens.push_back({TokenKind::Minus});
                ++i;
                continue;
            case '*':
                tokens.push_back({TokenKind::Star});
                ++i;
                continue;
            case '/':
                tokens.push_back({TokenKind::Slash});
                ++i;
                continue;
            case '(':
                tokens.push_back({TokenKind::LParen});
                ++i;
                continue;
            case ')':
                tokens.push_back({TokenKind::RParen});
                ++i;
                continue;
            case '<':
                if (i + 1 < n && condition[i + 1] == '=')
                {
                    tokens.push_back({TokenKind::Le});
                    i += 2;
                }
                else
                {
                    tokens.push_back({TokenKind::Lt});
                    ++i;
                }
                continue;
            case '>':
                if (i + 1 < n && condition[i + 1] == '=')
                {
                    tokens.push_back({TokenKind::Ge});
                    i += 2;
                }
                else
                {
                    tokens.push_back({TokenKind::Gt});
                    ++i;
                }
                continue;
            case '=':
                if (i + 1 < n && condition[i + 1] == '=')
                {
                    tokens.push_back({TokenKind::Eq});
                    i += 2;
                    continue;
                }
                return {}; // a bare '=' is not valid -- caller sees an empty/End stream and fails to parse
            case '!':
                if (i + 1 < n && condition[i + 1] == '=')
                {
                    tokens.push_back({TokenKind::Ne});
                    i += 2;
                    continue;
                }
                return {};
            default:
                return {};
            }
        }

        tokens.push_back({TokenKind::End});
        return tokens;
    }

    class Parser
    {
    public:
        Parser(const std::vector<Token>& tokens, const MagComponent& mag) : m_tokens(tokens), m_mag(mag) {}

        std::optional<bool> ParseCondition()
        {
            const std::optional<double> left = ParseArithmetic();
            if (!left || m_error)
                return std::nullopt;

            const TokenKind op = Peek().kind;
            switch (op)
            {
            case TokenKind::Lt:
            case TokenKind::Gt:
            case TokenKind::Le:
            case TokenKind::Ge:
            case TokenKind::Eq:
            case TokenKind::Ne:
                Advance();
                break;
            default:
                return std::nullopt; // a condition must be a comparison, not a bare expression
            }

            const std::optional<double> right = ParseArithmetic();
            if (!right || m_error || Peek().kind != TokenKind::End)
                return std::nullopt;

            switch (op)
            {
            case TokenKind::Lt:
                return *left < *right;
            case TokenKind::Gt:
                return *left > *right;
            case TokenKind::Le:
                return *left <= *right;
            case TokenKind::Ge:
                return *left >= *right;
            case TokenKind::Eq:
                return *left == *right;
            case TokenKind::Ne:
                return *left != *right;
            default:
                return std::nullopt; // unreachable
            }
        }

    private:
        const Token& Peek() const { return m_tokens[m_pos]; }
        Token Advance() { return m_tokens[m_pos++]; }

        std::optional<double> ParseArithmetic()
        {
            std::optional<double> value = ParseTerm();
            if (!value)
                return std::nullopt;

            while (Peek().kind == TokenKind::Plus || Peek().kind == TokenKind::Minus)
            {
                const bool is_add = Peek().kind == TokenKind::Plus;
                Advance();
                const std::optional<double> rhs = ParseTerm();
                if (!rhs)
                    return std::nullopt;
                value = is_add ? *value + *rhs : *value - *rhs;
            }
            return value;
        }

        std::optional<double> ParseTerm()
        {
            std::optional<double> value = ParseFactor();
            if (!value)
                return std::nullopt;

            while (Peek().kind == TokenKind::Star || Peek().kind == TokenKind::Slash)
            {
                const bool is_mul = Peek().kind == TokenKind::Star;
                Advance();
                const std::optional<double> rhs = ParseFactor();
                if (!rhs)
                    return std::nullopt;
                if (!is_mul && *rhs == 0.0)
                {
                    m_error = true;
                    return std::nullopt;
                }
                value = is_mul ? *value * *rhs : *value / *rhs;
            }
            return value;
        }

        std::optional<double> ParseFactor()
        {
            if (Peek().kind == TokenKind::Minus)
            {
                Advance();
                const std::optional<double> value = ParseFactor();
                return value ? std::optional<double>(-*value) : std::nullopt;
            }

            if (Peek().kind == TokenKind::LParen)
            {
                Advance();
                const std::optional<double> value = ParseArithmetic();
                if (!value || Peek().kind != TokenKind::RParen)
                {
                    m_error = true;
                    return std::nullopt;
                }
                Advance();
                return value;
            }

            if (Peek().kind == TokenKind::Number)
                return Advance().number;

            if (Peek().kind == TokenKind::Identifier)
                return ResolveIdentifier(Advance().identifier);

            m_error = true;
            return std::nullopt;
        }

        std::optional<double> ResolveIdentifier(const std::string& name)
        {
            if (name == "POW")
                return static_cast<double>(m_mag.pow_level);
            if (name == "DEF")
                return static_cast<double>(m_mag.def_level);
            if (name == "DEX")
                return static_cast<double>(m_mag.dex_level);
            if (name == "MIND")
                return static_cast<double>(m_mag.mind_level);
            if (name == "LEVEL")
                return static_cast<double>(MagLevel(m_mag));
            if (name == "IQ")
                return static_cast<double>(m_mag.iq);

            m_error = true;
            return std::nullopt;
        }

        const std::vector<Token>& m_tokens;
        const MagComponent& m_mag;
        std::size_t m_pos = 0;
        bool m_error = false;
    };

} // namespace

bool EvaluateMagEvolutionCondition(const std::string& condition, const MagComponent& mag)
{
    const std::vector<Token> tokens = Tokenize(condition);
    if (tokens.empty())
        return false;

    Parser parser(tokens, mag);
    return parser.ParseCondition().value_or(false);
}

} // namespace psr
