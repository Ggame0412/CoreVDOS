#pragma once
// dsl/lexer.h
// Лексер VDOSL. Только токенизация — ничего про AST.

#include <vector>
#include <string>
#include <string_view>
#include <cctype>

// ----------------------------------------------------------------
//  Типы токенов
// ----------------------------------------------------------------
enum class TokenType {
    IDENT,      // foo, object, cube, …
    STRING,     // "text"
    NUMBER,     // 1.5, -3, 0
    COLON,      // :
    SEMICOLON,  // ;
    EQUAL,      // =
    LBRACE,     // {
    RBRACE,     // }
    SECTION,    // @shape, @color, …
    END         // конец потока
};

inline std::string_view TokenTypeName(TokenType t) {
    switch(t) {
        case TokenType::IDENT:     return "IDENT";
        case TokenType::STRING:    return "STRING";
        case TokenType::NUMBER:    return "NUMBER";
        case TokenType::COLON:     return "COLON";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::EQUAL:     return "EQUAL";
        case TokenType::LBRACE:    return "LBRACE";
        case TokenType::RBRACE:    return "RBRACE";
        case TokenType::SECTION:   return "SECTION";
        case TokenType::END:       return "END";
        default:                   return "UNKNOWN";
    }
}

struct Token {
    TokenType        type;
    std::string_view value;
};

// ----------------------------------------------------------------
//  Лексер
// ----------------------------------------------------------------
class Lexer {
public:
    std::vector<std::string> errors;

    explicit Lexer(std::string_view input) : src(input) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        tokens.reserve(src.size() / 4);
        pos = 0;

        while (pos < src.size()) {
            skipWhitespace();
            if (pos >= src.size()) break;

            char c = src[pos];

            // Однострочный комментарий //
            if (c == '/' && pos + 1 < src.size() && src[pos+1] == '/') {
                while (pos < src.size() && src[pos] != '\n') pos++;
                continue;
            }

            switch (c) {
                case ':': tokens.push_back({TokenType::COLON,     src.substr(pos++, 1)}); break;
                case ';': tokens.push_back({TokenType::SEMICOLON,  src.substr(pos++, 1)}); break;
                case '{': tokens.push_back({TokenType::LBRACE,     src.substr(pos++, 1)}); break;
                case '}': tokens.push_back({TokenType::RBRACE,     src.substr(pos++, 1)}); break;
                case '=': tokens.push_back({TokenType::EQUAL,      src.substr(pos++, 1)}); break;

                case '"': tokens.push_back(readString()); break;
                case '@': tokens.push_back(readSection()); break;
                case '#': tokens.push_back(readHash());   break;

                default: {
                    if (isDigit(c) || (c == '-' && isDigitAhead())) {
                        tokens.push_back(readNumber());
                    } else if (isAlpha(c)) {
                        tokens.push_back(readIdent());
                    } else {
                        errors.push_back(
                            "Lexer Error: Unknown char '" + std::string(1, c) + "' at pos " + std::to_string(pos)
                        );
                        pos++;
                    }
                    break;
                }
            }
        }

        tokens.push_back({TokenType::END, ""});
        return tokens;
    }

private:
    std::string_view src;
    size_t           pos = 0;

    void skipWhitespace() {
        while (pos < src.size() &&
               (src[pos] == ' ' || src[pos] == '\n' ||
                src[pos] == '\r' || src[pos] == '\t')) pos++;
    }

    bool isDigit(char c) { return std::isdigit(static_cast<unsigned char>(c)); }
    bool isAlpha(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
    bool isAlNum(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.'; }

    // '-' является числом только если за ним идёт цифра (а не оператор)
    bool isDigitAhead() {
        return pos + 1 < src.size() && isDigit(src[pos+1]);
    }

    Token readString() {
        pos++; // пропустить открывающую "
        size_t start = pos;
        while (pos < src.size() && src[pos] != '"') pos++;
        if (pos >= src.size())
            errors.push_back("Lexer Error: Unclosed string literal");
        Token t{TokenType::STRING, src.substr(start, pos - start)};
        if (pos < src.size()) pos++; // пропустить закрывающую "
        return t;
    }

    Token readSection() {
        pos++; // пропустить '@'
        size_t start = pos;
        while (pos < src.size() && std::isalpha(static_cast<unsigned char>(src[pos]))) pos++;
        if (start == pos)
            errors.push_back("Lexer Error: Expected section name after '@'");
        return {TokenType::SECTION, src.substr(start, pos - start)};
    }

    Token readHash() {
        size_t start = pos++;
        while (pos < src.size() && std::isalnum(static_cast<unsigned char>(src[pos]))) pos++;
        return {TokenType::IDENT, src.substr(start, pos - start)};
    }

    Token readNumber() {
        size_t start = pos;
        if (src[pos] == '-') pos++;
        while (pos < src.size() && (isDigit(src[pos]) || src[pos] == '.')) pos++;
        return {TokenType::NUMBER, src.substr(start, pos - start)};
    }

    Token readIdent() {
        size_t start = pos;
        while (pos < src.size() && isAlNum(src[pos])) pos++;
        return {TokenType::IDENT, src.substr(start, pos - start)};
    }
};