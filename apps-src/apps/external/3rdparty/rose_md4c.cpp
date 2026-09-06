#include "rose_md4c.hpp"
#include <SDL.h>
#include "rose_filesystem_dll.hpp"
#include "rose_exception.hpp"
#include "rose_string_utils.hpp"

using namespace std::placeholders;

#ifdef __cplusplus
extern "C" {
#endif

extern int md2html_main(int argc, char** argv);

#ifdef __cplusplus
}
#endif

namespace md4c {

char MD_BLOCKTYPE_str[][32] {
    /* <body>...</body> */
    "MD_BLOCK_DOC",

    /* <blockquote>...</blockquote> */
    "MD_BLOCK_QUOTE",

    /* <ul>...</ul>
     * Detail: Structure MD_BLOCK_UL_DETAIL. */
    "MD_BLOCK_UL",

    /* <ol>...</ol>
     * Detail: Structure MD_BLOCK_OL_DETAIL. */
    "MD_BLOCK_OL",

    /* <li>...</li>
     * Detail: Structure MD_BLOCK_LI_DETAIL. */
    "MD_BLOCK_LI",

    /* <hr> */
    "MD_BLOCK_HR",

    /* <h1>...</h1> (for levels up to 6)
     * Detail: Structure MD_BLOCK_H_DETAIL. */
    "MD_BLOCK_H",

    /* <pre><code>...</code></pre>
     * Note the text lines within code blocks are terminated with '\n'
     * instead of explicit MD_TEXT_BR. */
    "MD_BLOCK_CODE",

    /* Raw HTML block. This itself does not correspond to any particular HTML
     * tag. The contents of it _is_ raw HTML source intended to be put
     * in verbatim form to the HTML output. */
    "MD_BLOCK_HTML",

    /* <p>...</p> */
    "MD_BLOCK_P",

    /* <table>...</table> and its contents.
     * Detail: Structure MD_BLOCK_TABLE_DETAIL (for MD_BLOCK_TABLE),
     *         structure MD_BLOCK_TD_DETAIL (for MD_BLOCK_TH and MD_BLOCK_TD)
     * Note all of these are used only if extension MD_FLAG_TABLES is enabled. */
    "MD_BLOCK_TABLE",
    "MD_BLOCK_THEAD",
    "MD_BLOCK_TBODY",
    "MD_BLOCK_TR",
    "MD_BLOCK_TH",
    "MD_BLOCK_TD"
};

const int MD_BLOCKTYPE_size = sizeof(MD_BLOCKTYPE_str) / sizeof(MD_BLOCKTYPE_str[0]);

char MD_SPANTYPE_str[][32] {
    /* <em>...</em> */
    "MD_SPAN_EM",

    /* <strong>...</strong> */
    "MD_SPAN_STRONG",

    /* <a href="xxx">...</a>
     * Detail: Structure MD_SPAN_A_DETAIL. */
    "MD_SPAN_A",

    /* <img src="xxx">...</a>
     * Detail: Structure MD_SPAN_IMG_DETAIL.
     * Note: Image text can contain nested spans and even nested images.
     * If rendered into ALT attribute of HTML <IMG> tag, it's responsibility
     * of the parser to deal with it.
     */
    "MD_SPAN_IMG",

    /* <code>...</code> */
    "MD_SPAN_CODE",

    /* <del>...</del>
     * Note: Recognized only when MD_FLAG_STRIKETHROUGH is enabled.
     */
    "MD_SPAN_DEL",

    /* For recognizing inline ($) and display ($$) equations
     * Note: Recognized only when MD_FLAG_LATEXMATHSPANS is enabled.
     */
    "MD_SPAN_LATEXMATH",
    "MD_SPAN_LATEXMATH_DISPLAY",

    /* Wiki links
     * Note: Recognized only when MD_FLAG_WIKILINKS is enabled.
     */
    "MD_SPAN_WIKILINK",

    /* <u>...</u>
     * Note: Recognized only when MD_FLAG_UNDERLINE is enabled. */
    "MD_SPAN_U"
};

const int MD_SPANTYPE_size = sizeof(MD_SPANTYPE_str) / sizeof(MD_SPANTYPE_str[0]);

char MD_TEXTTYPE_str[][32] {
    /* Normal text. */
    "MD_TEXT_NORMAL",

    /* NULL character. CommonMark requires replacing NULL character with
     * the replacement char U+FFFD, so this allows caller to do that easily. */
    "MD_TEXT_NULLCHAR",

    /* Line breaks.
     * Note these are not sent from blocks with verbatim output (MD_BLOCK_CODE
     * or MD_BLOCK_HTML). In such cases, '\n' is part of the text itself. */
    "MD_TEXT_BR",         /* <br> (hard break) */
    "MD_TEXT_SOFTBR",     /* '\n' in source text where it is not semantically meaningful (soft break) */

    /* Entity.
     * (a) Named entity, e.g. &nbsp; 
     *     (Note MD4C does not have a list of known entities.
     *     Anything matching the regexp /&[A-Za-z][A-Za-z0-9]{1,47};/ is
     *     treated as a named entity.)
     * (b) Numerical entity, e.g. &#1234;
     * (c) Hexadecimal entity, e.g. &#x12AB;
     *
     * As MD4C is mostly encoding agnostic, application gets the verbatim
     * entity text into the MD_PARSER::text_callback(). */
    "MD_TEXT_ENTITY",

    /* Text in a code block (inside MD_BLOCK_CODE) or inlined code (`code`).
     * If it is inside MD_BLOCK_CODE, it includes spaces for indentation and
     * '\n' for new lines. MD_TEXT_BR and MD_TEXT_SOFTBR are not sent for this
     * kind of text. */
    "MD_TEXT_CODE",

    /* Text is a raw HTML. If it is contents of a raw HTML block (i.e. not
     * an inline raw HTML), then MD_TEXT_BR and MD_TEXT_SOFTBR are not used.
     * The text contains verbatim '\n' for the new lines. */
    "MD_TEXT_HTML",

    /* Text is inside an equation. This is processed the same way as inlined code
     * spans (`code`). */
    "MD_TEXT_LATEXMATH"
};

const int MD_TEXTTYPE_size = sizeof(MD_TEXTTYPE_str) / sizeof(MD_TEXTTYPE_str[0]);

static int
enter_block_callback(MD_BLOCKTYPE type, void* detail, void* userdata)
{
	VALIDATE((int)type >= 0 && (int)type < MD_BLOCKTYPE_size, null_str);
    trender* r = reinterpret_cast<trender*>(userdata);
    if (r->verbose_call()) {
	    SDL_Log("{md4c}enter_block_callback, type: (%i)%s", type, MD_BLOCKTYPE_str[type]);
    }
    r->did_enter_block(type, detail);

    return 0;
}

static int
leave_block_callback(MD_BLOCKTYPE type, void* detail, void* userdata)
{
	VALIDATE((int)type >= 0 && (int)type < MD_BLOCKTYPE_size, null_str);
    trender* r = reinterpret_cast<trender*>(userdata);
    if (r->verbose_call()) {
	    SDL_Log("{md4c}leave_block_callback, type: (%i)%s", type, MD_BLOCKTYPE_str[type]);
    }
    r->did_leave_block(type, detail);

    return 0;
}

static int
enter_span_callback(MD_SPANTYPE type, void* detail, void* userdata)
{
	VALIDATE((int)type >= 0 && (int)type < MD_SPANTYPE_size, null_str);
    trender* r = reinterpret_cast<trender*>(userdata);
    if (r->verbose_call()) {
	    SDL_Log("{md4c}enter_span_callback, type: (%i)%s", type, MD_SPANTYPE_str[type]);
    }

    r->did_enter_span(type, detail);

    return 0;
}

static int
leave_span_callback(MD_SPANTYPE type, void* detail, void* userdata)
{
	VALIDATE((int)type >= 0 && (int)type < MD_SPANTYPE_size, null_str);
    trender* r = reinterpret_cast<trender*>(userdata);
    if (r->verbose_call()) {
	    SDL_Log("{md4c}leave_span_callback, type: (%i)%s", type, MD_SPANTYPE_str[type]);
    }
    r->did_leave_span(type, detail);

    return 0;
}

static int
text_callback(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* userdata)
{
    VALIDATE((int)type >= 0 && (int)type < MD_TEXTTYPE_size, null_str);
    VALIDATE(size > 0, null_str);

    // attation: text[size] maybe isn't '\0'.
    const std::string text2(text, size);

    trender* r = reinterpret_cast<trender*>(userdata);
    if (r->verbose_call()) {
		SDL_Log("{md4c}text_callback, type: (%i)%s, text: %s, size: %i", type, MD_TEXTTYPE_str[type], text2.c_str(), size);
    }
    r->did_text(type, text2);
    return 0;
}

static void
debug_log_callback(const char* msg, void* userdata)
{
    trender* r = reinterpret_cast<trender*>(userdata);
    r->did_debug_log(msg);
}

trender::trender(bool verbose_call, bool verbose_debug_log)
    : p_flags_(MD_FLAG_TABLES)
    , verbose_call_(verbose_call)
    , verbose_debug_log_(verbose_debug_log)
    , input_data_(nullptr)
    , input_data_size_(0)
{
    VALIDATE(sizeof(MD_CHAR) == 1, null_str);
}

void trender::did_debug_log(const char* msg)
{
    if (verbose_debug_log_) {
        SDL_Log("{md4c}: %s", msg);
	}
}

int md_data(const char* input_data, int input_size, trender& render)
{
    MD_PARSER parser = {
        0,
        render.parser_flags(),
        enter_block_callback,
        leave_block_callback,
        enter_span_callback,
        leave_span_callback,
        text_callback,
        debug_log_callback,
        NULL
    };
/*
    // Build map of characters which need escaping.
    for (int i = 0; i < 256; i++) {
        unsigned char ch = (unsigned char) i;

        if(strchr("\"&<>", ch) != NULL)
            render.escape_map[i] |= NEED_HTML_ESC_FLAG;

        if(!ISALNUM(ch)  &&  strchr("~-_.+!*(),%#@?=;:/,+$", ch) == NULL)
            render.escape_map[i] |= NEED_URL_ESC_FLAG;
    }
*/
    // Consider skipping UTF-8 byte order mark (BOM).
    const bool skip_utf8_bom = true;
    // if (renderer_flags & MD_HTML_FLAG_SKIP_UTF8_BOM  &&  sizeof(MD_CHAR) == 1) {
	if (skip_utf8_bom && sizeof(MD_CHAR) == 1) {
        const MD_CHAR bom[3] = { (char)0xef, (char)0xbb, (char)0xbf };
        if (input_size >= sizeof(bom) && memcmp(input_data, bom, sizeof(bom)) == 0) {
            input_data += sizeof(bom);
            input_size -= sizeof(bom);
        }
    }

    return md_parse(input_data, input_size, &parser, (void*)&render);
}

int md_file(const std::string& input_file, trender& render)
{
    tfile file(input_file, GENERIC_READ, OPEN_EXISTING);
    int fsize = file.read_2_data();
    if (fsize == 0) {
        return -1;
    }
    return md_data(file.data, fsize, render);
}

void md2html(const std::string& input_file, const std::string& output_file)
{
    VALIDATE(!input_file.empty() && !output_file.empty(), null_str);

	// md2html.exe --full-html --ftables c:\ddksample\2.md -o c:\ddksample\2.html
	char argv0[] = "md2html.exe";
	char argv1[] = "--full-html";
	char argv2[] = "--ftables";
	char argv3[256];
    SDL_strlcpy(argv3, input_file.c_str(), sizeof(argv3));
	char argv4[] = "-o";
	char argv5[256];
    SDL_strlcpy(argv5, output_file.c_str(), sizeof(argv5));
	char* argvp[] = {argv0, argv1, argv2, argv3, argv4, argv5};

	char** argv = argvp;
	int argc = sizeof(argvp) / sizeof(argvp[0]);
	md2html_main(argc, argv);
}

}