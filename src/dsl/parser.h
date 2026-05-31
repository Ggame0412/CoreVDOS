#pragma once
// dsl/parser.h
// Парсер VDOSL. Строит AST из потока токенов.
// Зависит только от lexer.h — не от raylib, не от rp3d.

#include "lexer.h"
#include <unordered_map>

// ----------------------------------------------------------------
//  AST узлы
// ----------------------------------------------------------------

// Один @section внутри object {}
struct SectionNode {
    std::string_view              name;
    std::vector<std::string_view> params;
};

// Один визуальный/физический элемент (начинается с @shape)
struct ElementNode {
    std::string_view              shape       = "cube";
    std::vector<std::string_view> shapeParams;
    std::string_view              colorName   = "gray";
    std::string_view              texturePath = "";
    std::vector<std::string_view> localPos;
    std::vector<std::string_view> localRot;
    std::unordered_map<std::string_view, std::string_view> namedParams;
};

// Корневой узел — один object {}
struct ObjectNode {
    std::string_view         name;
    std::vector<SectionNode> sections;  // все @секции (для общего доступа)
    std::vector<ElementNode> elements;  // элементы сгруппированные по @shape
};

// ----------------------------------------------------------------
//  Парсер
// ----------------------------------------------------------------
class Parser {
public:
    std::vector<std::string> errors;

    explicit Parser(const std::vector<Token>& tokens) : tokens(tokens) {}

    // Парсим один object {} — точка входа для одиночного DSL блока
    ObjectNode parse() {
        ObjectNode obj;
        if (!expectIdent("object", "object declaration")) return obj;
        pos++;

        if (!expect(TokenType::IDENT, "object name")) return obj;
        obj.name = current().value;
        pos++;

        if (!expect(TokenType::LBRACE, "object body '{'")) return obj;
        pos++;

        parseBody(obj);

        expect(TokenType::RBRACE, "object body '}'");
        if (current().type == TokenType::RBRACE) pos++;

        return obj;
    }

    // Парсим весь файл — несколько object {} подряд
    std::vector<ObjectNode> parseScene() {
        std::vector<ObjectNode> scene;
        while (!atEnd()) {
            if (current().value == "object") {
                scene.push_back(parse());
            } else {
                errors.push_back(
                    "Parser Error: Expected 'object', got '" +
                    std::string(current().value) + "'"
                );
                pos++;
            }
        }
        return scene;
    }

private:
    const std::vector<Token>& tokens;
    size_t                    pos = 0;

    Token current() const {
        return (pos < tokens.size()) ? tokens[pos] : Token{TokenType::END, ""};
    }
    Token peek() const {
        return (pos + 1 < tokens.size()) ? tokens[pos+1] : Token{TokenType::END, ""};
    }
    bool atEnd() const {
        return pos >= tokens.size() || tokens[pos].type == TokenType::END;
    }

    bool expect(TokenType type, std::string_view ctx) {
        if (current().type != type) {
            errors.push_back(
                "Parser Error: Expected " + std::string(TokenTypeName(type)) +
                " in '" + std::string(ctx) +
                "', got '" + std::string(current().value) + "'"
            );
            return false;
        }
        return true;
    }

    bool expectIdent(std::string_view val, std::string_view ctx) {
        if (current().type != TokenType::IDENT || current().value != val) {
            errors.push_back(
                "Parser Error: Expected '" + std::string(val) +
                "' in '" + std::string(ctx) +
                "', got '" + std::string(current().value) + "'"
            );
            return false;
        }
        return true;
    }

    // Читаем параметры секции до следующей секции или }
    std::vector<std::string_view> readParams(ElementNode& el) {
        std::vector<std::string_view> params;
        params.reserve(8);

        while (!atEnd() &&
               current().type != TokenType::SECTION &&
               current().type != TokenType::RBRACE)
        {
            // key = value — именованный параметр
            if (current().type == TokenType::IDENT && peek().type == TokenType::EQUAL) {
                std::string_view key = current().value;
                pos += 2; // skip key and '='
                std::string_view val = current().value;
                el.namedParams[key] = val;
                params.push_back(val);
                pos++;
            }
            else if (current().type == TokenType::IDENT  ||
                     current().type == TokenType::NUMBER ||
                     current().type == TokenType::STRING)
            {
                params.push_back(current().value);
                pos++;
            }
            else {
                errors.push_back(
                    "Parser Error: Unexpected '" + std::string(current().value) + "' in section params"
                );
                pos++;
            }
        }
        return params;
    }

    void parseBody(ObjectNode& obj) {
        ElementNode curEl;
        bool        hasEl = false;

        while (!atEnd() && current().type != TokenType::RBRACE) {
            if (current().type != TokenType::SECTION) {
                errors.push_back(
                    "Parser Error: Expected '@section', got '" +
                    std::string(current().value) + "'"
                );
                pos++;
                continue;
            }

            std::string_view secName = current().value;
            pos++;

            auto params = readParams(curEl);

            // Сохраняем секцию для общего доступа
            obj.sections.push_back({secName, params});

            // Применяем к текущему элементу
            if (secName == "shape") {
                if (hasEl) { obj.elements.push_back(std::move(curEl)); curEl = ElementNode(); }
                if (!params.empty()) curEl.shape = params[0];
                curEl.shapeParams = params;
                hasEl = true;
            }
            else if (secName == "tranc")   { curEl.localPos    = params; }
            else if (secName == "rot")     { curEl.localRot    = params; }
            else if (secName == "color")   { if (!params.empty()) curEl.colorName   = params[0]; }
            else if (secName == "texture") { if (!params.empty()) curEl.texturePath = params[0]; }
            else if (secName == "physic")  { /* будет: curEl.physType = params[0] */ }
            else {
                errors.push_back("Parser Warning: Unknown section '@" + std::string(secName) + "'");
            }
        }

        if (hasEl) obj.elements.push_back(std::move(curEl));
    }
};