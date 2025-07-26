//
// TeX command parser for automated expansion of glossaries commands and
// rebuttal markup
//
// Copyright 2025 Sergiu Deitsch <sergiu.deitsch@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// #define BOOST_SPIRIT_X3_DEBUG

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <locale>
#include <map>
#include <string>
#include <vector>

#include <boost/filesystem/fstream.hpp>
#include <boost/spirit/home/support/iterators/istream_iterator.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>

namespace ast {

enum Flags
{
    None      = 0,
    Plural    = 1 << 0,
    Uppercase = 1 << 1,
    First     = 1 << 2
};

constexpr int ModifiersMask = Plural | Uppercase;

// Abbreviation definition
struct Abbreviation
{
    std::string name;
    std::string shortName;
    std::string value;
};

// Reference to an abbreviation
struct Reference
{
    std::string name;
    unsigned flags;
};

} // namespace ast

namespace gls {
    using namespace boost::spirit::x3;

    const rule<struct options, std::string> options = "options";
    const rule<struct addition, std::string> addition = "addition";

    const rule<struct text, std::string> text = "text";
    const rule<struct nested, std::string> nested = "nested";
    const rule<struct group, std::string> group = "group";
    const rule<struct nested_group, std::string> nested_group = "nested_group";

    const auto text_def = lexeme[+(char_ - '{' - '}')];
    const auto nested_def =
        +(text | nested_group); // Allow to mix groups and text as in "{group1 {group2} text}"
    // Make sure to capture the braces of nested groups
    const auto nested_group_def = char_('{') >> nested >> char_('}');
    const auto group_def = '{' >> -nested >> '}';

    const auto options_def = '[' >> *(char_ - ']') >> ']';

    BOOST_SPIRIT_DEFINE(text, nested, nested_group, group);

    const auto addition_def =
           "\\addition"
        >> omit[options]
        >> group
        ;

    BOOST_SPIRIT_DEFINE(addition, options);

    const auto newacronym_def =
           "\\newacronym"
        >> group
        [
            (
                [] (auto& ctx)
                {
                    _val(ctx).name = _attr(ctx);
                }
            )
        ]
        >> group
        [
            (
                [] (auto& ctx)
                {
                    _val(ctx).shortName = _attr(ctx);
                }
            )
        ]
        >> group
        [
            (
                [] (auto& ctx)
                {
                    _val(ctx).value = _attr(ctx);
                }
            )
        ]
        ;

    // Parse \gls
    const auto gls_def =
           lit("\\gls")
        >> group
        [
            (
                [] (auto& ctx)
                {
                    _val(ctx).name = _attr(ctx);
                    _val(ctx).flags = ast::None;
                }
            )
        ]
        ;

    // Parse \Gls
    const auto Gls_def =
           lit("\\Gls")
        >> group
        [
            (
                [] (auto& ctx)
                {
                    _val(ctx).name = _attr(ctx);
                    _val(ctx).flags = ast::Uppercase;
                }
            )
        ]
        ;

    // Parse \glspl
    const auto glspl_def =
           lit("\\glspl")
        >> group
        [
            (
                [] (auto& ctx)
                {
                    _val(ctx).name = _attr(ctx);
                    _val(ctx).flags = ast::Plural;
                }
            )
        ]
        ;

    // Parse \Glspl
    const auto Glspl_def =
           lit("\\Glspl")
        >> group
        [
            (
                [] (auto& ctx)
                {
                    _val(ctx).name = _attr(ctx);
                    _val(ctx).flags = ast::Uppercase | ast::Plural;
                }
            )
        ]
        ;

    // Parse \Glsfirst
    const auto Glsfirst_def =
           lit("\\Glsfirst")
        >> group
        [
            (
                [] (auto& ctx)
                {
                    _val(ctx).name = _attr(ctx);
                    _val(ctx).flags = ast::Uppercase | ast::First;
                }
            )
        ]
        ;

    const auto gls_other =
           lit("\\gls") >> +alpha
           ;

    const rule<struct Gls, ast::Reference> Gls = "Gls";
    const rule<struct Glsfrst, ast::Reference> Glsfirst = "Glsfirst";
    const rule<struct Glspl, ast::Reference> Glspl = "Glspl";
    const rule<struct gls, ast::Reference> gls = "gls";
    const rule<struct glspl, ast::Reference> glspl = "glspl";
    const rule<struct newacronym, ast::Abbreviation> newacronym = "newacronym";

    BOOST_SPIRIT_DEFINE(newacronym);
    BOOST_SPIRIT_DEFINE(gls);
    BOOST_SPIRIT_DEFINE(glspl);
    BOOST_SPIRIT_DEFINE(Gls);
    BOOST_SPIRIT_DEFINE(Glsfirst);
    BOOST_SPIRIT_DEFINE(Glspl);

    const auto gls_commands =
          newacronym
        | gls
        | glspl
        | Gls
        | Glsfirst
        | Glspl
        ;

    const auto gls_tokens =
    *(
          gls_commands
        | omit[gls_other]
        |
        +(
            char_
            - "\\newacronym"
            - "\\gls"
            - "\\glspl"
            - "\\Gls"
            - "\\Glspl"
        )
    ) >> eoi;

    const auto addition_tokens =
    *(
          addition
        |
        +(
            char_
            - "\\addition"
        )
    ) >> eoi;

} // namespace gls


std::ostream& make_uppercase(std::ostream& out, const std::string& value)
{
    if (!value.empty())
        out << std::toupper(value[0], std::locale::classic()) << value.substr(1);

    return out;
}

struct Expand
{
    using result_type = void;

    Expand(std::ostream& out, std::map<std::string, std::pair<ast::Abbreviation, bool> >& definitions)
        : out(out)
        , definitions(definitions)
    {
    }

    void operator()(const std::string& value) const
    {
        out << value;
    }

    void operator()(const ast::Reference& value)
    {
        auto pos = definitions.find(value.name);

        if (pos != definitions.end()) {
            bool used = pos->second.second && (value.flags & ast::First) != ast::First;

            switch (value.flags & ast::ModifiersMask) {
                case ast::None:
                    if (!used)
                        out << pos->second.first.value << " (" << pos->second.first.shortName << ")";
                    else
                        out << pos->second.first.shortName;
                    break;
                case ast::Plural:
                    if (!used)
                        out << pos->second.first.value << "s" << " (" << pos->second.first.shortName << "s)";
                    else
                        out << pos->second.first.shortName << "s";
                    break;
                case ast::Uppercase:
                    if (!used)
                        make_uppercase(out, pos->second.first.value) << " (" << pos->second.first.shortName << ")";
                    else
                        make_uppercase(out, pos->second.first.shortName);
                    break;
                case ast::Uppercase | ast::Plural:
                    if (!used)
                        make_uppercase(out, pos->second.first.value) << "s" << " (" << pos->second.first.shortName << "s)";
                    else
                        make_uppercase(out, pos->second.first.shortName) << "s";
                    break;
            }

            pos->second.second = true;
        }
        else
            throw std::runtime_error("missing definition for " + value.name);
    }

    void operator()(const ast::Abbreviation& value) const
    {
    }

    std::ostream& out;
    std::map<std::string, std::pair<ast::Abbreviation, bool> >& definitions;
};

struct BuildDictionary
{
    using result_type = void;

    void operator()(const std::string& value) const
    {
    }

    void operator()(const ast::Reference& value) const
    {
        //std::cout << value.name << std::endl;
    }

    void operator()(const ast::Abbreviation& value)
    {
        //std::cout << value.name << std::endl;
        dict[value.name] = std::make_pair(value, false);

        if (value.value.empty())
            std::cerr << "warning: description of " << value.name << " is empty\n";
    }

    std::map<std::string, std::pair<ast::Abbreviation, bool> > dict;
};

int main(int argc, char** argv)
{
    boost::filesystem::ifstream in(argv[1]);

    if (!in) {
        std::cerr << "error: failed to open input " << std::quoted(argv[1]) << std::endl;
        return EXIT_FAILURE;
    }

    in.unsetf(std::ios_base::skipws);

    using namespace boost::spirit::x3;

    using Entry = variant
    <
          std::string
        , ast::Reference
        , ast::Abbreviation
    >;

    std::vector
    <
        Entry
    > values;

    bool parsed = parse
    (
          boost::spirit::istream_iterator(in)
        , boost::spirit::istream_iterator()
        , gls::gls_tokens
        , values
    );

    if (!parsed) {
        std::cerr << "error: failed to parse the input\n";
        return EXIT_FAILURE;
    }

    BuildDictionary dict;

    std::for_each(values.begin(), values.end(),
        [&dict] (const auto& entry)
        {
            entry.apply_visitor(dict);
        }
    );

    // Expansion of \addition requires a second pass
    std::stringstream out;

    Expand e{out, dict.dict};

    for (auto val : values) {
        val.apply_visitor(e);
    }

    std::string gls = out.str();
    std::string expanded;

    parsed = parse
    (
          gls.begin()
        , gls.end()
        , gls::addition_tokens
        , expanded
    );

    if (!parsed) {
        std::cerr << "error: failed to parse the input\n";
        return EXIT_FAILURE;
    }

    std::copy(expanded.begin(), expanded.end(), std::ostreambuf_iterator<char>(std::cout));
}
