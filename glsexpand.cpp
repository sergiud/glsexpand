//
// Copyright (c) 2019 Sergiu Deitsch <sergiu.deitsch@gmail.com>
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation and/or
// other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
