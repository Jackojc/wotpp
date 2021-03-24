#include <string>
#include <vector>
#include <limits>
#include <algorithm>

#include <misc/fwddecl.hpp>
#include <misc/util/util.hpp>
#include <frontend/char.hpp>
#include <frontend/parser/parser.hpp>


namespace {
	inline bool peek_is_intrinsic(const wpp::Token& tok) {
		return
			tok == wpp::TOKEN_RUN or
			tok == wpp::TOKEN_FILE or
			tok == wpp::TOKEN_ASSERT or
			tok == wpp::TOKEN_PIPE or
			tok == wpp::TOKEN_ERROR or
			tok == wpp::TOKEN_SLICE or
			tok == wpp::TOKEN_FIND or
			tok == wpp::TOKEN_LENGTH or
			tok == wpp::TOKEN_ESCAPE or
			tok == wpp::TOKEN_LOG
		;
	}

	inline bool peek_is_keyword(const wpp::Token& tok) {
		return
			tok == wpp::TOKEN_LET or
			tok == wpp::TOKEN_DROP or
			tok == wpp::TOKEN_MAP or
			tok == wpp::TOKEN_USE or
			tok == wpp::TOKEN_PUSH or
			tok == wpp::TOKEN_POP
		;
	}

	inline bool peek_is_smart_string(const wpp::Token& tok) {
		return
			tok == wpp::TOKEN_CODESTR or
			tok == wpp::TOKEN_RAWSTR or
			tok == wpp::TOKEN_PARASTR
		;
	}

	// Check if the token is a string.
	inline bool peek_is_string(const wpp::Token& tok) {
		return
			tok == wpp::TOKEN_DOUBLEQUOTE or
			tok == wpp::TOKEN_QUOTE or

			tok == wpp::TOKEN_STRINGIFY or

			tok == wpp::TOKEN_HEX or
			tok == wpp::TOKEN_BIN or

			peek_is_smart_string(tok)
		;
	}

	inline bool peek_is_reserved_name(const wpp::Token& tok) {
		return
			peek_is_intrinsic(tok) or
			peek_is_keyword(tok)
		;
	}

	// Check if the token is an expression.
	inline bool peek_is_call(const wpp::Token& tok) {
		return
			tok == wpp::TOKEN_IDENTIFIER or
			peek_is_intrinsic(tok)
		;
	}

	// Check if the token is an expression.
	inline bool peek_is_expr(const wpp::Token& tok) {
		return
			tok == wpp::TOKEN_POP or
			tok == wpp::TOKEN_MAP or
			tok == wpp::TOKEN_EVAL or
			tok == wpp::TOKEN_LBRACE or
			peek_is_string(tok) or
			peek_is_call(tok)
		;
	}

	// Check if the token is a statement.
	inline bool peek_is_stmt(const wpp::Token& tok) {
		return
			peek_is_keyword(tok) or
			peek_is_expr(tok)
		;
	}

	inline bool peek_is_escape(const wpp::Token& tok) {
		return
			tok == wpp::TOKEN_ESCAPE_BACKSLASH or
			tok == wpp::TOKEN_ESCAPE_CARRIAGERETURN or
			tok == wpp::TOKEN_ESCAPE_NEWLINE or
			tok == wpp::TOKEN_ESCAPE_TAB or
			tok == wpp::TOKEN_ESCAPE_BIN or
			tok == wpp::TOKEN_ESCAPE_HEX or
			tok == wpp::TOKEN_ESCAPE_DOUBLEQUOTE or
			tok == wpp::TOKEN_ESCAPE_QUOTE
		;
	}
}


namespace wpp {
	namespace {
		// Forward declarations.
		wpp::node_t normal_string(wpp::Lexer&, wpp::AST&, wpp::Positions&, wpp::Env&);
		wpp::node_t stringify_string(wpp::Lexer&, wpp::AST&, wpp::Positions&, wpp::Env&);
		wpp::node_t hex_string(wpp::Lexer&, wpp::AST&, wpp::Positions&, wpp::Env&);
		wpp::node_t bin_string(wpp::Lexer&, wpp::AST&, wpp::Positions&, wpp::Env&);

		wpp::node_t raw_string(wpp::Lexer&, wpp::AST&, wpp::Positions&, wpp::Env&);
		wpp::node_t para_string(wpp::Lexer&, wpp::AST&, wpp::Positions&, wpp::Env&);
		wpp::node_t code_string(wpp::Lexer&, wpp::AST&, wpp::Positions&, wpp::Env&);

		wpp::node_t expression(wpp::Lexer&, wpp::AST&, wpp::Positions&, wpp::Env&);
		wpp::node_t fninvoke(wpp::Lexer&, wpp::AST&, wpp::Positions&, wpp::Env&);
		wpp::node_t map(wpp::Lexer&, wpp::AST&, wpp::Positions&, wpp::Env&);
		wpp::node_t block(wpp::Lexer&, wpp::AST&, wpp::Positions&, wpp::Env&);
		wpp::node_t codeify(wpp::Lexer&, wpp::AST&, wpp::Positions&, wpp::Env&);
		wpp::node_t string(wpp::Lexer&, wpp::AST&, wpp::Positions&, wpp::Env&);

		wpp::node_t statement(wpp::Lexer&, wpp::AST&, wpp::Positions&, wpp::Env&);
		wpp::node_t drop(wpp::Lexer&, wpp::AST&, wpp::Positions&, wpp::Env&);
		wpp::node_t let(wpp::Lexer&, wpp::AST&, wpp::Positions&, wpp::Env&);
		wpp::node_t use(wpp::Lexer&, wpp::AST&, wpp::Positions&, wpp::Env&);

		wpp::node_t pop(wpp::Lexer&, wpp::AST&, wpp::Positions&, wpp::Env&);
		wpp::node_t push(wpp::Lexer&, wpp::AST&, wpp::Positions&, wpp::Env&);
	}
}


namespace wpp {
	namespace {
		// Consume tokens comprising a string. Handles escape chars.
		std::string handle_escapes(const wpp::Token& part) {
			DBG();

			std::string str;

			// handle escape sequences.
			if (part == TOKEN_ESCAPE_DOUBLEQUOTE)
				str = "\"";

			else if (part == TOKEN_ESCAPE_QUOTE)
				str = "'";

			else if (part == TOKEN_ESCAPE_BACKSLASH)
				str = "\\";

			else if (part == TOKEN_ESCAPE_NEWLINE)
				str = "\n";

			else if (part == TOKEN_ESCAPE_TAB)
				str = "\t";

			else if (part == TOKEN_ESCAPE_CARRIAGERETURN)
				str = "\r";

			else if (part == TOKEN_ESCAPE_HEX) {
				// Get first and second nibble.
				uint8_t first_nibble = *part.view.ptr;
				uint8_t second_nibble = *(part.view.ptr + 1);

				// Shift the first nibble to the left by 4 and then OR it with
				// the second nibble so the first nibble is the first 4 bits
				// and the second nibble is the last 4 bits.
				str = static_cast<uint8_t>(
					wpp::hex_to_digit(first_nibble) << 4 |
					wpp::hex_to_digit(second_nibble)
				);
			}

			else if (part == TOKEN_ESCAPE_BIN) {
				const auto& [ptr, len] = part.view;
				uint8_t value = 0;

				// Shift value by 1 each loop and OR 0/1 depending on
				// the current char.
				for (size_t i = 0; i < len; ++i) {
					value <<= 1;
					value |= ptr[i] - '0';
				}

				str = value;
			}

			return str;
		}


		wpp::node_t normal_string(wpp::Lexer& lex, wpp::AST& tree, wpp::Positions& pos, wpp::Env& env) {
			const wpp::node_t node = tree.add<String>();
			pos.emplace_back(lex.position());

			auto& str = tree.get<String>(node).value;

			const auto delim = lex.advance(wpp::lexer_modes::string); // Store delimeter.

			// Consume tokens until we reach `delim` or EOF.
			while (lex.peek(wpp::lexer_modes::string) != delim) {
				if (lex.peek(wpp::lexer_modes::string) == TOKEN_EOF)
					wpp::error(node, env, "unterminated string", "reached EOF while parsing string literal that begins here");

				// Parse escape characters and append "parts" of the string to `str`.
				if (peek_is_escape(lex.peek(wpp::lexer_modes::string)))
					str += wpp::handle_escapes(lex.advance(wpp::lexer_modes::string));

				else
					str += lex.advance(wpp::lexer_modes::string).str();
			}

			lex.advance(); // Skip terminating quote.

			DBG("'", str, "'");

			return node;
		}


		wpp::node_t stringify(wpp::Lexer& lex, wpp::AST& tree, wpp::Positions& pos, wpp::Env& env) {
			const wpp::node_t node = tree.add<String>();
			pos.emplace_back(lex.position());

			lex.advance(); // skip '`'.

			if (lex.peek() != TOKEN_IDENTIFIER)
				wpp::error(
					lex.position(), env,
					"identifier expected",
					"expecting an identifier to follow `\\`",
					"insert an identifier after `\\` to stringify it"
				);

			tree.get<String>(node).value = lex.advance().str();

			DBG("'", tree.get<String>(node).value, "'");

			return node;
		}


		wpp::node_t raw_string(wpp::Lexer& lex, wpp::AST& tree, wpp::Positions& pos, wpp::Env& env) {
			const wpp::node_t node = tree.add<String>();
			pos.emplace_back(lex.position());
			auto& str = tree.get<String>(node).value;

			const auto delim = lex.advance().view.at(1);  // User defined delimiter.
			const auto quote = lex.advance(wpp::lexer_modes::string_raw); // ' or "

			while (true) {
				if (lex.peek(wpp::lexer_modes::string_raw) == TOKEN_EOF)
					wpp::error(node, env, "unterminated string", "reached EOF while parsing raw string literal that begins here");

				// If we encounter ' or ", we check one character ahead to see
				// if it matches the user defined delimiter, it if does,
				// we erase the last quote character and break.
				else if (lex.peek(wpp::lexer_modes::string_raw) == quote) {
					// Store this quote, it may not actually be a part
					// of the string terminator. We append it to the string
					// if the next `if` block tests false.
					const auto tmp = lex.advance(wpp::lexer_modes::string_raw);

					if (lex.peek(wpp::lexer_modes::chr).view == delim) {
						lex.advance(wpp::lexer_modes::chr); // Skip user delimiter.
						break;  // Exit the loop, string is fully consumed.
					}

					str += tmp.str();
				}

				// If not EOF or '/", consume.
				else {
					str += lex.advance(wpp::lexer_modes::string_raw).str();
				}
			}

			DBG("'", str, "'");

			return node;
		}


		wpp::node_t para_string(wpp::Lexer& lex, wpp::AST& tree, wpp::Positions& pos, wpp::Env& env) {
			const wpp::node_t node = tree.add<String>();
			pos.emplace_back(lex.position());
			auto& str = tree.get<String>(node).value;

			const auto delim = lex.advance(wpp::lexer_modes::string_para).view.at(1);  // User defined delimiter.
			const auto quote = lex.advance(wpp::lexer_modes::string_para); // ' or "


			struct Part {
				std::string str;
				bool is_whitespace;

				Part(const std::string& str_, bool is_whitespace_):
					str(str_), is_whitespace(is_whitespace_) {}
			};

			std::vector<Part> chunks;


			while (wpp::eq_any(lex.peek(wpp::lexer_modes::string_para), TOKEN_WHITESPACE, TOKEN_WHITESPACE_NEWLINE))
				lex.advance(wpp::lexer_modes::string_para);


			while (true) {
				if (lex.peek(wpp::lexer_modes::string_para) == TOKEN_EOF)
					wpp::error(node, env, "unterminated string", "reached EOF while parsing paragraph string literal that begins here");

				// If we encounter ' or ", we check one character ahead to see
				// if it matches the user defined delimiter, it if does,
				// we erase the last quote character and break.
				else if (lex.peek(wpp::lexer_modes::string_para) == quote) {
					// Store this quote, it may not actually be a part
					// of the string terminator. We append it to the string
					// if the next `if` block tests false.
					const auto tmp = lex.advance(wpp::lexer_modes::string_para);

					if (lex.peek(wpp::lexer_modes::chr).view == delim) {
						lex.advance(wpp::lexer_modes::chr); // Skip user delimiter.
						break;  // Exit the loop, string is fully consumed.
					}

					// Quote was not a part of the string terminator so we append it.
					chunks.emplace_back(tmp.str(), false);
				}

				// If not EOF or '/", consume.
				else {
					const auto token = lex.advance(wpp::lexer_modes::string_para);

					// Collapse pairs of newlines into a single newline and strip any loner newlines.
					if (token == TOKEN_WHITESPACE_NEWLINE) {
						chunks.emplace_back(std::string(token.str().size() / 2, '\n'), true);

						// If this newline has whitespace after it, we have to check
						// how much whitespace there is to track indentation level.
						if (chunks.back().str.size() > 0 and lex.peek(wpp::lexer_modes::string_para) == TOKEN_WHITESPACE)
							lex.advance(wpp::lexer_modes::string_para);
					}

					// Collapse repeated whitespace of the same type.
					else if (token == TOKEN_WHITESPACE)
						chunks.emplace_back(wpp::collapse_repeated(token.str()), true);

					// Handle escape sequences.
					else if (peek_is_escape(token))
						chunks.emplace_back(wpp::handle_escapes(token), false);

					// Otherwise just append the textual parts of the string.
					else
						chunks.emplace_back(token.str(), false);
				}
			}


			// Trim trailing whitespace.
			auto it = chunks.rbegin();
			while (it->is_whitespace)
				++it;

			chunks.erase(it.base(), chunks.end());


			// Join chunks.
			for (auto& [chunk, is_whitespace]: chunks)
				str += chunk;


			DBG("'", str, "'");

			return node;
		}


		wpp::node_t code_string(wpp::Lexer& lex, wpp::AST& tree, wpp::Positions& pos, wpp::Env& env) {
			const wpp::node_t node = tree.add<String>();
			pos.emplace_back(lex.position());
			auto& str = tree.get<String>(node).value;

			const auto delim = lex.advance(wpp::lexer_modes::string_code).view.at(1);  // User defined delimiter.
			const auto quote = lex.advance(wpp::lexer_modes::string_code); // ' or "

			enum {
				KIND_LEADING,
				KIND_NEWLINE,
				KIND_WHITESPACE,
				KIND_OTHER,
			};

			struct Part {
				std::string str;
				uint8_t kind;

				Part(const std::string& str_, uint8_t kind_):
					str(str_), kind(kind_) {}
			};

			std::vector<Part> chunks;


			// Check if the first token is whitespace and then check if its followed
			// by text.
			// If it is followed by text, this whitespace is leading.
			if (lex.peek(wpp::lexer_modes::string_code) == TOKEN_WHITESPACE) {
				const auto tmp = lex.advance(wpp::lexer_modes::string_code).str();

				if (not wpp::eq_any(lex.peek(wpp::lexer_modes::string_code), TOKEN_WHITESPACE, TOKEN_WHITESPACE_NEWLINE))
					chunks.emplace_back(tmp, KIND_LEADING);

				else
					chunks.emplace_back(tmp, KIND_WHITESPACE);
			}


			while (true) {
				if (lex.peek(wpp::lexer_modes::string_code) == TOKEN_EOF)
					wpp::error(node, env, "unterminated string", "reached EOF while parsing paragraph string literal that begins here");

				// If we encounter ' or ", we check one character ahead to see
				// if it matches the user defined delimiter, it if does,
				// we erase the last quote character and break.
				else if (lex.peek(wpp::lexer_modes::string_code) == quote) {
					// Store this quote, it may not actually be a part
					// of the string terminator. We append it to the string
					// if the next `if` block tests false.
					const auto tmp = lex.advance(wpp::lexer_modes::string_code);

					if (lex.peek(wpp::lexer_modes::chr).view == delim) {
						lex.advance(wpp::lexer_modes::chr); // Skip user delimiter.
						break;  // Exit the loop, string is fully consumed.
					}

					// Quote was not a part of the string terminator so we append it.
					chunks.emplace_back(tmp.str(), KIND_OTHER);
				}

				// If not EOF or '/", consume.
				else {
					const auto token = lex.advance(wpp::lexer_modes::string_code);

					// Check for newline followed by whitespace.
					if (token == TOKEN_WHITESPACE_NEWLINE) {
						chunks.emplace_back(token.str(), KIND_NEWLINE);

						// If this newline has whitespace after it, we have to check
						// how much whitespace there is to track indentation level.
						if (lex.peek(wpp::lexer_modes::string_code) == TOKEN_WHITESPACE)
							chunks.emplace_back(lex.advance(wpp::lexer_modes::string_code).str(), KIND_LEADING);

						else
							chunks.emplace_back("", KIND_LEADING);
					}

					else if (token == TOKEN_WHITESPACE)
						chunks.emplace_back(token.str(), KIND_WHITESPACE);

					// Handle escape sequences.
					else if (peek_is_escape(token))
						chunks.emplace_back(wpp::handle_escapes(token), KIND_OTHER);

					// Otherwise just append the textual parts of the string.
					else
						chunks.emplace_back(token.str(), KIND_OTHER);
				}
			}


			// Trim leading whitespace.
			for (auto it = chunks.begin(); it != chunks.end();) {
				// If we find text, break. The leading whitespace has been removed.
				if (it->kind == KIND_OTHER)
					break;

				// If we find a newline, erase from the beginning of
				// the vector to the current iterator.
				else if (it->kind == KIND_NEWLINE) {
					// We use `i + 1` here to erase this newline also.
					chunks.erase(chunks.begin(), it + 1);

					it = chunks.begin(); // Reset from beginning and search.
					continue;
				}

				++it; // We increment here so that we avoid skipping the first element on a "reset".
			}


			// Trim trailing whitespace.
			// Loop from back of vector to beginning.
			for (auto it = chunks.rbegin(); it != chunks.rend(); ++it) {
				// If we find text, erase from the back of the
				// vector to the current position of the iterator.
				if (it->kind == KIND_OTHER) {
					chunks.erase(it.base(), chunks.end());
					break;
				}
			}


			// Discover common leading whitespace amount.
			size_t common_leading_whitespace = std::numeric_limits<size_t>::max();

			for (const auto& [chunk, kind]: chunks) {
				if (kind == KIND_LEADING and chunk.size() < common_leading_whitespace)
					common_leading_whitespace = chunk.size();
			}


			// Join chunks.
			for (auto& [chunk, kind]: chunks) {
				// If this chunk is leading whitespace, strip up to `common_leading_whitespace` from the front.
				if (kind == KIND_LEADING)
					chunk.erase(chunk.begin(), chunk.begin() + common_leading_whitespace);

				str += chunk;
			}


			DBG("'", str, "'");

			return node;
		}


		wpp::node_t hex_string(wpp::Lexer& lex, wpp::AST& tree, wpp::Positions& pos, wpp::Env&) {
			const auto& [ptr, len] = lex.advance().view;

			const wpp::node_t node = tree.add<String>();
			pos.emplace_back(lex.position());
			auto& str = tree.get<String>(node).value;

			size_t counter = 0; // index into string, doesnt count `_`.

			for (size_t i = len; i > 0; i--) {
				const char c = ptr[i - 1];

				// Skip underscores.
				if (c == '_')
					continue;

				// Odd index, shift digit by 4 and or it with last character.
				if (counter & 1)
					str.back() |= wpp::hex_to_digit(c) << 4;

				// Even index, push back digit.
				else
					str.push_back(wpp::hex_to_digit(c));

				counter++;
			}

			std::reverse(str.begin(), str.end());

			DBG("'", str, "'");

			return node;
		}


		wpp::node_t bin_string(wpp::Lexer& lex, wpp::AST& tree, wpp::Positions& pos, wpp::Env&) {
			const wpp::node_t node = tree.add<String>();
			pos.emplace_back(lex.position());
			auto& str = tree.get<String>(node).value;

			const auto& [ptr, len] = lex.advance().view;

			// Reserve some space, this is kind of arbitrary.
			str.reserve(len);

			size_t counter = 0; // index into string without tracking `_`.

			for (size_t i = len; i > 0; i--) {
				const char c = ptr[i - 1];

				// Skip underscores.
				if (c == '_')
					continue;

				// We shift and or every character encounter.
				if (counter & 7)
					str.back() |= (c - '0') << (counter & 7);

				// When we get to a multiple of 8, we push back the character.
				else
					str.push_back(c - '0');

				counter++;
			}

			std::reverse(str.begin(), str.end());

			DBG("'", str, "'");

			return node;
		}


		// Parse a string.
		// `"hey" 'hello' "a\nb\nc\n"`
		wpp::node_t string(wpp::Lexer& lex, wpp::AST& tree, wpp::Positions& pos, wpp::Env& env) {
			wpp::node_t node = wpp::NODE_EMPTY;

			if (lex.peek() == TOKEN_QUOTE or lex.peek() == TOKEN_DOUBLEQUOTE)
				node = normal_string(lex, tree, pos, env);

			else if (lex.peek() == TOKEN_STRINGIFY)
				node = stringify(lex, tree, pos, env);

			else if (lex.peek() == TOKEN_RAWSTR)
				node = raw_string(lex, tree, pos, env);

			else if (lex.peek() == TOKEN_CODESTR)
				node = code_string(lex, tree, pos, env);

			else if (lex.peek() == TOKEN_PARASTR)
				node = para_string(lex, tree, pos, env);

			else if (lex.peek() == TOKEN_HEX)
				node = hex_string(lex, tree, pos, env);

			else if (lex.peek() == TOKEN_BIN)
				node = bin_string(lex, tree, pos, env);

			return node;
		}


		// Parses a function.
		wpp::node_t let(wpp::Lexer& lex, wpp::AST& tree, wpp::Positions& pos, wpp::Env& env) {
			// Create `Fn` node ahead of time so we can insert member data
			// directly instead of copying/moving it into a new node at the end.
			const wpp::node_t node = tree.add<Fn>();
			pos.emplace_back(lex.position());

			// Skip `let` keyword. The statement parser already checked
			// for it before calling us.
			lex.advance();


			// Make sure the next token is an identifier, if it is, set the name
			// of our `Fn` node to match.
			if (lex.peek() != TOKEN_IDENTIFIER)
				wpp::error(
					lex.position(), env,
					"expected identifier",
					"expecting an identifier to follow `let`"
				);

			tree.get<Fn>(node).identifier = lex.advance().view;


			// Variable definition
			if (peek_is_expr(lex.peek())) {
				const wpp::node_t expr = wpp::expression(lex, tree, pos, env);
				tree.replace<Var>(node, tree.get<Fn>(node).identifier, expr);
				return node;
			}


			// Otherwise, this is a function definition.
			if (lex.peek() != TOKEN_LPAREN)
				wpp::error(
					lex.position(), env,
					"expected `)`",
					"expecting `)` to follow parameter after `,`",
					"there might be a non-identifier token in the parameter list"
				);

			lex.advance();  // Skip `(`.

			if (lex.peek() != TOKEN_RPAREN) {
				// Collect parameters.
				// Advance until we run out of identifiers.
				// While there is an identifier there is another parameter.
				while (lex.peek() == TOKEN_IDENTIFIER) {
					// Add the parameter
					tree.get<Fn>(node).parameters.emplace_back(lex.advance().view);

					// If the next token is a comma, skip it.
					if (lex.peek() == TOKEN_COMMA)
						lex.advance(); // skip the comma

					// Otherwise it must be ')'?
					else if (lex.peek() != TOKEN_RPAREN)
						// If it's not, throw an exception.
						wpp::error(
							lex.position(), env,
							"expected `)`",
							"expecting `)` to follow parameter after `,`",
							"there might be a non-identifier token in the parameter list"
						);
				}

				// Check if there's a keyword conflict.
				// We check if the next token is a reserved name and throw an error
				// if it is. The reason we don't check this in the while loop body is
				// because the loop condition checks for an identifier and so breaks
				// out if the next token is an intrinsic.
				if (peek_is_reserved_name(lex.peek()))
					wpp::error(
						lex.position(), env,
						"invalid name",
						wpp::cat("parameter name '", lex.peek().str(), "' conflicts with keyword of the same name")
					);
			}

			// Make sure parameter list is terminated by `)`.
			if (lex.peek() != TOKEN_RPAREN)
				wpp::error(
					lex.position(), env,
					"expected `)`",
					"expecting `)` to follow parameter list",
					"there might be a non-identifier token in the parameter list"
				);

			lex.advance();

			// Parse the function body.
			const wpp::node_t body = expression(lex, tree, pos, env);
			tree.get<Fn>(node).body = body;

			DBG(tree.get<Fn>(node).identifier, " (", tree.get<Fn>(node).parameters.size(), " params)");

			return node;
		}


		wpp::node_t codeify(wpp::Lexer& lex, wpp::AST& tree, wpp::Positions& pos, wpp::Env& env) {
			DBG();

			wpp::node_t node = tree.add<Codeify>();
			pos.emplace_back(lex.position());

			lex.advance(); // Skip !.

			if (not peek_is_expr(lex.peek()))
				wpp::error(
					lex.position(), env,
					"expected expression",
					"expecting an expression to follow `!`",
					"insert an expression after `!`"
				);

			const wpp::node_t expr = wpp::expression(lex, tree, pos, env);
			tree.get<Codeify>(node).expr = expr;

			return node;
		}


		wpp::node_t drop(wpp::Lexer& lex, wpp::AST& tree, wpp::Positions& pos, wpp::Env& env) {
			DBG();

			const wpp::node_t node = tree.add<Drop>();
			pos.emplace_back(lex.position());

			lex.advance(); // Skip `drop`.

			const wpp::node_t call_expr = wpp::fninvoke(lex, tree, pos, env);
			tree.get<Drop>(node).func = call_expr;

			return node;
		}


		wpp::node_t use(wpp::Lexer& lex, wpp::AST& tree, wpp::Positions& pos, wpp::Env& env) {
			DBG();

			const wpp::node_t node = tree.add<Use>();
			pos.emplace_back(lex.position());

			lex.advance();

			if (not peek_is_string(lex.peek()))
				wpp::error(
					lex.position(), env,
					"expected string",
					"expecting a string as path name for `use`"
				);

			const wpp::node_t path = wpp::string(lex, tree, pos, env);
			tree.get<Use>(node).path = path;

			return node;
		}


		wpp::node_t push(wpp::Lexer& lex, wpp::AST& tree, wpp::Positions& pos, wpp::Env& env) {
			DBG();

			const wpp::node_t node = tree.add<Push>();
			pos.emplace_back(lex.position());

			lex.advance(); // skip `push`.

			if (not peek_is_expr(lex.peek()))
				wpp::error(
					lex.position(), env,
					"expected expression",
					"expecting an expression to follow `push`"
				);

			const wpp::node_t expr = wpp::expression(lex, tree, pos, env);
			tree.get<Push>(node).expr = expr;

			return node;
		}


		wpp::node_t pop(wpp::Lexer& lex, wpp::AST& tree, wpp::Positions& pos, wpp::Env& env) {
			DBG();

			const wpp::node_t node = tree.add<Pop>();
			pos.emplace_back(lex.position());

			lex.advance(); // skip `pop`.

			if (not peek_is_expr(lex.peek()))
				wpp::error(
					lex.position(), env,
					"expected expression",
					"expecting an expression to follow `pop`"
				);

			const wpp::node_t expr = wpp::expression(lex, tree, pos, env);
			tree.get<Pop>(node).expr = expr;

			return node;
		}


		// Parse a function call.
		wpp::node_t fninvoke(wpp::Lexer& lex, wpp::AST& tree, wpp::Positions& pos, wpp::Env& env) {
			wpp::node_t node = tree.add<FnInvoke>();
			pos.emplace_back(lex.position());

			const auto fn_token = lex.advance();

			// Optional arguments.
			if (lex.peek() != TOKEN_LPAREN) {
				tree.replace<VarRef>(node, fn_token.view);
				return node;
			}


			lex.advance();  // Skip `(`.

			// While there is an expression there is another parameter.
			while (peek_is_expr(lex.peek())) {
				// Parse expr.
				wpp::node_t expr = expression(lex, tree, pos, env);
				tree.get<FnInvoke>(node).arguments.emplace_back(expr);

				// If the next token is a comma, skip it.
				if (lex.peek() == TOKEN_COMMA)
					lex.advance(); // skip the comma

				// Otherwise it must be ')'
				else if (lex.peek() != TOKEN_RPAREN)
					wpp::error(
						lex.position(), env,
						"expected `)`",
						"expecting `)` to follow argument list",
						"there might be a non-identifier token in the argument list"
					);
			}

			// Make sure parameter list is terminated by `)`.
			if (lex.advance() != TOKEN_RPAREN)
				wpp::error(
					lex.position(), env,
					"expected `)`",
					"expecting `)` to follow argument list",
					"there might be a non-identifier token in the argument list"
				);

			// Check if function call is an intrinsic.

			// If it is an intrinsic, we replace the FnInvoke node type with
			// the Intrinsic node type and forward the arguments.
			if (peek_is_intrinsic(fn_token)) {
				const auto args = tree.get<FnInvoke>(node).arguments;
				const auto identifier = tree.get<FnInvoke>(node).identifier;

				tree.replace<Intrinsic>(node, args, identifier, fn_token.type);
				DBG(tree.get<Intrinsic>(node).identifier, " (", tree.get<Intrinsic>(node).arguments.size(), " args)");
			}

			else {
				tree.get<FnInvoke>(node).identifier = fn_token.view;
				DBG(tree.get<FnInvoke>(node).identifier, " (", tree.get<FnInvoke>(node).arguments.size(), " args)");
			}

			return node;
		}


		// wpp::node_t prefix(wpp::Lexer& lex, wpp::AST& tree, wpp::Positions& pos, wpp::Env& env) {
		// 	DBG();

			// Create `Pre` node.
			// const wpp::node_t node = tree.add<Pre>();
			// pos.emplace_back(lex.position());


			// // Skip `prefix` token, we already known it's there because
			// // of it being seen by our caller, the statement parser.
			// lex.advance();


			// // Expect identifier.
			// if (not peek_is_expr(lex.peek()))
			// 	wpp::error(node, env, "prefix does not have a name.");

			// // Set name of `Pre`.
			// const wpp::node_t expr = wpp::expression(lex, tree, pos, env);
			// tree.get<Pre>(node).exprs = {expr};


			// // Expect opening brace.
			// if (lex.advance() != TOKEN_LBRACE)
			// 	wpp::error(node, env, "expecting '{' to follow name.");


			// // Loop through body of prefix and collect statements.
			// if (lex.peek() != TOKEN_RBRACE) {
			// 	// Parse statement and then append it to statements vector in `Pre`.
			// 	// The reason we seperate parsing and emplacing the node ID is
			// 	// to prevent dereferencing an invalidated iterator.
			// 	// If `tree` resizes while parsing the statement, by the time it returns
			// 	// and is emplaced, the reference to the `Pre` node in `tree` might
			// 	// be invalidated.
			// 	do {
			// 		const wpp::node_t stmt = statement(lex, tree, pos, env);
			// 		tree.get<Pre>(node).statements.emplace_back(stmt);
			// 	} while (peek_is_stmt(lex.peek()));
			// }


			// // Expect closing brace.
			// if (lex.advance() != TOKEN_RBRACE)
			// 	wpp::error(node, env, "prefix is unterminated.");


			// Return index to `Pre` node we created at the top of this function.
			// return NODE_EMPTY;
		// }


		// Parse a block.
		wpp::node_t block(wpp::Lexer& lex, wpp::AST& tree, wpp::Positions& pos, wpp::Env& env) {
			const wpp::node_t node = tree.add<Block>();
			pos.emplace_back(lex.position());

			lex.advance(); // Skip '{'.

			if (lex.peek() == TOKEN_RBRACE)
				wpp::error(
					lex.position(), env,
					"expected expression",
					"expecting a trailing expression at the end of block"
				);

			// Check for statement, otherwise we parse a single expression.
			// last_is_expr is used to check if the last statement holds
			// an expression, if it does we need to back up after parsing
			// the last statement to consider it as the trailing expression
			// of the block.
			bool last_is_expr = false;

			if (peek_is_stmt(lex.peek())) {
				// Consume statements.
				do {
					last_is_expr = peek_is_expr(lex.peek());

					const wpp::node_t stmt = statement(lex, tree, pos, env);
					tree.get<Block>(node).statements.emplace_back(stmt);
				} while (peek_is_stmt(lex.peek()));
			}

			// If the next token is not an expression and the last statement
			// was an expression then we can pop the last statement and use
			// it as our trailing expression.
			if (not peek_is_expr(lex.peek()) and last_is_expr) {
				tree.get<Block>(node).expr = tree.get<Block>(node).statements.back();
				tree.get<Block>(node).statements.pop_back();
			}

			else {
				wpp::error(
					tree.get<Block>(node).statements.back(), env,
					"expected expression",
					"expecting a trailing expression at the end of block"
				);
			}

			if (lex.peek() == TOKEN_ARROW)
				wpp::error(
					lex.position(), env,
					"unexpected `->`",
					"found `->` inside a block expression",
					"did you forget the test expression for map?"
				);

			// Expect '}'.
			if (lex.peek() != TOKEN_RBRACE) {
				wpp::error(
					node, env,
					"expected `}`",
					"expecting `}` to terminate block expression that begins here"
				);
			}

			DBG(tree.get<Block>(node).statements.size(), " statements + 1 expression");

			lex.advance(); // Skip '}'.

			return node;
		}


		wpp::node_t map(wpp::Lexer& lex, wpp::AST& tree, wpp::Positions& pos, wpp::Env& env) {
			const wpp::node_t node = tree.add<Map>();
			pos.emplace_back(lex.position());

			lex.advance(); // Skip `map`.


			// Check for test expression.
			if (not peek_is_expr(lex.peek()))
				wpp::error(
					lex.position(), env,
					"expected expression",
					"expecting an expression to follow `map`",
					"insert a test expression for `map` to match on"
				);


			const auto expr = wpp::expression(lex, tree, pos, env); // Consume test expression.
			tree.get<Map>(node).expr = expr;


			if (lex.peek() != TOKEN_LBRACE)
				wpp::error(
					lex.position(), env,
					"expected `{`",
					"expecting `{` to begin map expression body"
				);

			lex.advance();


			// Collect all arms of the map.
			while (peek_is_expr(lex.peek())) {
				const auto arm = wpp::expression(lex, tree, pos, env);

				if (lex.peek() != TOKEN_ARROW)
					wpp::error(
						lex.position(), env,
						"expected `->`",
						"expecting `->` to denote right hand side of map arm"
					);

				lex.advance();

				if (not peek_is_expr(lex.peek()))
					wpp::error(
						lex.position(), env,
						"expected expression",
						"expecting an expression after `->`"
					);

				const auto hand = wpp::expression(lex, tree, pos, env);

				tree.get<Map>(node).cases.emplace_back(std::pair{ arm, hand });
			}


			// Optional default case.
			if (lex.peek() == TOKEN_STAR) {
				lex.advance();

				if (lex.peek() != TOKEN_ARROW)
					wpp::error(
						lex.position(), env,
						"expected `->`",
						"expecting `->` to denote right hand side of map arm"
					);

				lex.advance();

				if (not peek_is_expr(lex.peek()))
					wpp::error(
						lex.position(), env,
						"expected expression",
						"expecting an expression after `->`"
					);

				const auto default_case = wpp::expression(lex, tree, pos, env);
				tree.get<Map>(node).default_case = default_case;
			}

			else {
				tree.get<Map>(node).default_case = wpp::NODE_EMPTY;
			}


			if (lex.peek() != TOKEN_RBRACE)
				wpp::error(
					node, env,
					"expected `}`",
					"expecting `}` to terminate map expression that begins here"
				);

			lex.advance();

			DBG("cases: ", tree.get<Map>(node).cases.size(), ", has default: ", tree.get<Map>(node).default_case != NODE_EMPTY);

			return node;
		}


		// Parse an expression.
		wpp::node_t expression(wpp::Lexer& lex, wpp::AST& tree, wpp::Positions& pos, wpp::Env& env) {
			// We use lhs to store the resulting expression
			// from the following cases and if the next token
			// is concatenation, we make a new Concat node using
			// lhs as the left hand side of the node and then continue on to
			// parse another expression for the right hand side.
			wpp::node_t lhs;

			const auto lookahead = lex.peek();

			if (peek_is_call(lookahead))
				lhs = wpp::fninvoke(lex, tree, pos, env);

			else if (peek_is_string(lookahead))
				lhs = wpp::string(lex, tree, pos, env);

			else if (lookahead == TOKEN_LBRACE)
				lhs = wpp::block(lex, tree, pos, env);

			else if (lookahead == TOKEN_MAP)
				lhs = wpp::map(lex, tree, pos, env);

			else if (lookahead == TOKEN_EVAL)
				lhs = wpp::codeify(lex, tree, pos, env);

			else if (lookahead == TOKEN_POP)
				lhs = wpp::pop(lex, tree, pos, env);

			else
				wpp::error(
					lex.position(), env,
					"expected expression",
					"expecting an expression to appear here"
				);

			if (lex.peek() == TOKEN_CAT) {
				const wpp::node_t node = tree.add<Concat>();
				pos.emplace_back(lex.position());

				lex.advance(); // Skip `..`.

				const wpp::node_t rhs = expression(lex, tree, pos, env);

				tree.get<Concat>(node).lhs = lhs;
				tree.get<Concat>(node).rhs = rhs;

				return node;
			}

			return lhs;
		}


		// Parse a statement.
		wpp::node_t statement(wpp::Lexer& lex, wpp::AST& tree, wpp::Positions& pos, wpp::Env& env) {
			const auto lookahead = lex.peek();

			if (lookahead == TOKEN_LET)
				return wpp::let(lex, tree, pos, env);

			else if (lookahead == TOKEN_DROP)
				return wpp::drop(lex, tree, pos, env);

			else if (lookahead == TOKEN_USE)
				return wpp::use(lex, tree, pos, env);

			else if (lookahead == TOKEN_PUSH)
				return wpp::push(lex, tree, pos, env);

			else if (peek_is_expr(lookahead))
				return wpp::expression(lex, tree, pos, env);

			wpp::error(
				lex.position(), env,
				"expected statement",
				"expecting a statement to appear here"
			);

			return wpp::NODE_EMPTY;
		}
	}


	// Parse a document.
	// A document is just a series of zero or more expressions.
	wpp::node_t document(wpp::Lexer& lex, wpp::AST& tree, wpp::Positions& pos, wpp::Env& env) {
		const wpp::node_t node = tree.add<Document>();
		pos.emplace_back(lex.position());

		// Consume expressions until we encounter eof or an error.
		while (lex.peek() != TOKEN_EOF) {
			try {
				const wpp::node_t stmt = statement(lex, tree, pos, env);
				tree.get<Document>(node).statements.emplace_back(stmt);
			}

			catch (const wpp::Error& e) {
				e.show();
				env.state |= wpp::INTERNAL_ERROR_STATE;

				while (
					not wpp::eq_any(lex.peek(),
						TOKEN_EOF,
						TOKEN_LET,
						TOKEN_LBRACE
					)
				)
					lex.advance();
			}
		}

		return node;
	}
}
