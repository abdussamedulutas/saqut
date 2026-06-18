#include "tokenizer/tokenizer.hpp"

std::vector<Token*> Tokenizer::scan(std::string input, std::string filePath) {
    std::vector<Token*> tokens;
    hmx.setSourceText(filePath, input);
    while (true) {
        Token* token = scope();
        if (token->token == "EOL") break;
        tokens.push_back(token);
        if (hmx.isEnd()) break;
    }
    return tokens;
}

Token* Tokenizer::scope() {
    hmx.skipWhiteSpace();

    if (hmx.include("//", true))  { skipOneLineComment(); return scope(); }
    if (hmx.include("/*", true))  { skipMultiLineComment(); return scope(); }

    if (hmx.isEnd()) {
        Token* t = new Token();
        t->token = "EOL";
        return t;
    }

    if (hmx.getchar() == '"')
        return readString();

    if (hmx.isNumeric()) {
        INumber lem = hmx.readNumeric();
        NumberToken* nt = new NumberToken();
        nt->loc        = lem.startLoc;
        nt->base       = lem.base;
        nt->start      = lem.start;
        nt->end        = lem.end;
        nt->hasEpsilon = lem.hasEpsilon;
        nt->isFloat    = lem.isFloat;
        nt->token      = lem.token;
        return nt;
    }

    for (const auto& kw : keywords) {
        if (hmx.include(kw, false)) {
            char next = hmx.getchar(static_cast<int>(kw.size()));
            if ((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z') ||
                (next >= '0' && next <= '9') || next == '_' || next == '$') {
                continue;
            }
            KeywordToken* kt = new KeywordToken();
            kt->start = hmx.getOffset();
            kt->loc   = hmx.getLocation();
            hmx.toChar(static_cast<int>(kw.size()));
            kt->end   = hmx.getOffset();
            kt->token = kw;
            return kt;
        }
    }

    for (const auto& del : delimiters) {
        if (hmx.include(del, false)) {
            DelimiterToken* dt = new DelimiterToken();
            dt->start = hmx.getOffset();
            dt->loc   = hmx.getLocation();
            hmx.toChar(static_cast<int>(del.size()));
            dt->end   = hmx.getOffset();
            dt->token = del;
            return dt;
        }
    }

    for (const auto& op : operators) {
        if (hmx.include(op, false)) {
            OperatorToken* ot = new OperatorToken();
            ot->start = hmx.getOffset();
            ot->loc   = hmx.getLocation();
            hmx.toChar(static_cast<int>(op.size()));
            ot->end   = hmx.getOffset();
            ot->token = op;
            return ot;
        }
    }

    return readIdentifier();
}

IdentifierToken* Tokenizer::readIdentifier() {
    hmx.beginPosition();
    IdentifierToken* it = new IdentifierToken();
    it->start = hmx.getOffset();

    while (!hmx.isEnd()) {
        char c = hmx.getchar();
        bool read = false;

        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            read = true;
            it->token.push_back(c);
        } else if (c == '_' || c == '$') {
            read = true;
            it->token.push_back(c);
        }

        if (read) {
            hmx.nextChar();
        } else {
            if (it->token.empty()) { hmx.nextChar(); } break;
        }
    }

    it->end  = hmx.getOffset();
    it->size = static_cast<int>(it->context.size());
    it->loc  = hmx.sourceFile.offsetToLocation(it->start);
    hmx.acceptPosition();
    return it;
}

StringToken* Tokenizer::readString() {
    hmx.beginPosition();
    StringToken* st = new StringToken();
    bool started = false;
    bool ended   = false;
    st->start = hmx.getOffset();

    while (!hmx.isEnd()) {
        char c = hmx.getchar();
        st->token.push_back(c);
        switch (c) {
            case '"':
                if (!started) {
                    started = true;
                } else {
                    ended = true;
                }
                break;
            case '\\':
                hmx.nextChar();
                c = hmx.getchar();
                st->token.push_back(c);
                st->context.push_back(c);
                break;
            default:
                st->context.push_back(c);
                break;
        }
        hmx.nextChar();
        if (ended) break;
    }

    st->end  = hmx.getOffset();
    st->size = static_cast<int>(st->context.size());
    st->loc  = hmx.sourceFile.offsetToLocation(st->start);
    hmx.acceptPosition();
    return st;
}

void Tokenizer::skipOneLineComment() {
    while (!hmx.isEnd()) {
        if (hmx.getchar() == '\n') {
            hmx.nextChar();
            hmx.skipWhiteSpace();
            return;
        }
        hmx.nextChar();
    }
}

void Tokenizer::skipMultiLineComment() {
    while (!hmx.isEnd()) {
        if (hmx.include("*/", true)) {
            hmx.skipWhiteSpace();
            return;
        }
        hmx.nextChar();
    }
}
