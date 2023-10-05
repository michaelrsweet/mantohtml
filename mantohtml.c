//
// Man page to HTML conversion program.
//
// Copyright © 2022-2023 by Michael R Sweet.
//
// Licensed under Apache License v2.0.
// <https://opensource.org/licenses/Apache-2.0>
//
// Usage:
//
//    mantohtml [OPTIONS] MAN-FILE [... MAN-FILE] >HTML-FILE
//
// Options:
//
//    --author 'AUTHOR'        Set author metadata
//    --chapter 'CHAPTER'      Set chapter (H1 heading)
//    --copyright 'COPYRIGHT'  Set copyright metadata
//    --css CSS-FILE-OR-URL    Use named stylesheet
//    --help                   Show help
//    --subject 'SUBJECT'      Set subject metadata
//    --title 'TITLE'          Set output title
//    --version                Show version
//

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#if _WIN32
#  include <io.h>
#  define access _access
#else
#  include <unistd.h>
#endif // _WIN32
#if !defined(__has_extension) && !defined(__GNUC__)
#  define __attribute__(...)
#endif // !__has_extension && !__GNUC__


//
// Local types...
//

typedef enum man_font_e			// Man page fonts
{
  MAN_FONT_REGULAR,			// Plain/regular font
  MAN_FONT_BOLD,			// Boldface (<strong>) font
  MAN_FONT_ITALIC,			// Italic (<em>) font
  MAN_FONT_SMALL,			// Small (<small>) font
  MAN_FONT_SMALL_BOLD,			// Small bold (<small><strong>) font
  MAN_FONT_MONOSPACE			// Monospaced (<pre>) font
} man_font_t;

typedef enum man_heading_e		// Man page heading levels
{
  MAN_HEADING_TOPIC,			// Topic heading (.TH)
  MAN_HEADING_SECTION,			// Section heading (.SH)
  MAN_HEADING_SUBSECTION		// Sub-section heading (.SS)
} man_heading_t;

typedef struct man_state_s		// Current man page state
{
  bool		wrote_header;		// Did we write the HTML header?
  char		basepath[1024];		// Source base path
  const char	*in_block;		// Current block element?
  bool		in_link;		// Are we in a link?
  size_t	indent;			// Indentation level
  const char	*author;		// Author metadata
  const char	*chapter;		// Chapter title
  const char	*copyright;		// Copyright metadata
  const char	*css;			// Stylesheet
  const char	*subject;		// Subject metadata
  const char	*title;			// Title
  char		atopic[256],		// Current topic (anchor)
		asection[256];		// Current section (anchor)
  man_font_t	font;			// Current font
} man_state_t;


//
// Local functions...
//

static void	convert_man(man_state_t *state, const char *filename);
static char	*html_anchor(char *anchor, const char *s, size_t anchorsize);
static void	html_font(man_state_t *state, man_font_t font);
static void	html_footer(man_state_t *state);
static void	html_header(man_state_t *state, const char *title);
static void	html_heading(man_state_t *state, man_heading_t heading, const char *s);
static void	html_printf(const char *format, ...) __attribute__((__format__(__printf__, 1, 2)));
static void	html_putc(int ch);
static void	html_puts(const char *s);
static char	*man_gets(FILE *fp, char *buffer, size_t bufsize, int *linenum);
static void	man_puts(man_state_t *state, const char *s);
static void	man_xx(man_state_t *state, man_font_t a, man_font_t b, const char *line);
static char	*parse_measurement(char *buffer, const char **lineptr, size_t bufsize, char defunit);
static char	*parse_value(char *buffer, const char **lineptr, size_t bufsize);
static void	safe_strcpy(char *dst, const char *src, size_t dstsize);
static int	usage(const char *opt);


//
// 'main()' - Convert a man page to HTML.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line args
     char *argv[])			// I - Command-line arguments
{
  int		i;			// Looping var
  man_state_t	state;			// Current man state
  bool		end_of_options = false;	// End of options seen?


  // Initialize the current state...
  memset(&state, 0, sizeof(state));

  // Parse command-line...
  for (i = 1; i < argc; i ++)
  {
    if (!strcmp(argv[i], "--author"))
    {
      // --author "AUTHOR"
      i ++;
      if (i >= argc)
      {
        fputs("mantohtml: Missing author after --author.\n", stderr);
        return (1);
      }

      state.author = argv[i];
    }
    else if (!strcmp(argv[i], "--chapter"))
    {
      // --chapter "CHAPTER"
      i ++;
      if (i >= argc)
      {
        fputs("mantohtml: Missing chapter after --chapter.\n", stderr);
        return (1);
      }

      state.chapter = argv[i];
    }
    else if (!strcmp(argv[i], "--copyright"))
    {
      // --copyright "COPYRIGHT"
      i ++;
      if (i >= argc)
      {
        fputs("mantohtml: Missing copyright after --copyright.\n", stderr);
        return (1);
      }

      state.copyright = argv[i];
    }
    else if (!strcmp(argv[i], "--css"))
    {
      // --css "CSS-FILE-OR-URL"
      i ++;
      if (i >= argc)
      {
        fputs("mantohtml: Missing CSS filename or URL after --css.\n", stderr);
        return (1);
      }

      state.css = argv[i];
    }
    else if (!strcmp(argv[i], "--help"))
    {
      // --help
      return (usage(NULL));
    }
    else if (!strcmp(argv[i], "--subject"))
    {
      // --subject "SUBJECT"
      i ++;
      if (i >= argc)
      {
        fputs("mantohtml: Missing subject after --subject.\n", stderr);
        return (1);
      }

      state.subject = argv[i];
    }
    else if (!strcmp(argv[i], "--title"))
    {
      // --title "TITLE"
      i ++;
      if (i >= argc)
      {
        fputs("mantohtml: Missing title after --title.\n", stderr);
        return (1);
      }

      state.title = argv[i];
    }
    else if (!strcmp(argv[i], "--version"))
    {
      // --version
      puts(VERSION);
      return (0);
    }
    else if (!strcmp(argv[i], "--"))
    {
      // -- (end of options)
      end_of_options = true;
    }
    else if (argv[i][0] == '-' && !end_of_options)
    {
      // Unknown option...
      return (usage(argv[i]));
    }
    else
    {
      // Convert the named file...
      convert_man(&state, argv[i]);
    }
  }

  // Finish up...
  if (state.wrote_header)
  {
    // HTML footer and return...
    html_footer(&state);
    return (0);
  }

  // If we get here we didn't have any man pages to convert...
  usage(NULL);

  return (1);
}


//
// 'convert_man()' - Convert a man page.
//

static void
convert_man(man_state_t *state,		// I - Current man state
            const char  *filename)	// I - Man filename
{
  FILE		*fp;			// Man file
  char		line[65536],		// Line from file
		macro[4];		// Macro from line
  const char	*lineptr;		// Pointer into line
  int		linenum = 0;		// Current line number
  bool		th_seen = false,	// Have we seen the TH macro?
		warning = false;	// Have we displayed a warning?
  const char	*break_text = "";	// Text to break after next line


  if ((fp = fopen(filename, "r")) == NULL)
  {
    perror(filename);
    return;
  }

  if (strchr(filename, '/'))
  {
    // Calculate base path for man source...
    char	*baseptr;		// Pointer into base path

    safe_strcpy(state->basepath, filename, sizeof(state->basepath));

    if ((baseptr = strrchr(state->basepath, '/')) != NULL)
      *baseptr = '\0';
    else
      safe_strcpy(state->basepath, ".", sizeof(state->basepath));
  }
  else
  {
    // Assume the man source is in the current directory...
    safe_strcpy(state->basepath, ".", sizeof(state->basepath));
  }

  while (man_gets(fp, line, sizeof(line), &linenum))
  {
//    fprintf(stderr, "%5d: %s\n", linenum, line);

    if (line[0] == '.')
    {
      // Start of a macro
      lineptr = line;
      parse_value(macro, &lineptr, sizeof(macro));

      if (!strcmp(macro, "."))
      {
        // . (blank line)
        continue;
      }
      else if (!strcmp(macro, ".TH"))
      {
        // .TH title section [footer-middle [footer-inside [header-middle]]]
        char	title[256],		// Man title
		section[32],		// Man section
		topic[300];		// title(section)

        if (!parse_value(title, &lineptr, sizeof(title)) || !title[0])
        {
          fprintf(stderr, "mantohtml: Missing title in '.TH' on line %d of '%s'.\n", linenum, filename);
          exit(1);
        }

        if (!parse_value(section, &lineptr, sizeof(section)) || !isdigit(section[0] & 255))
        {
          fprintf(stderr, "mantohtml: Missing section in '.TH' on line %d of '%s'.\n", linenum, filename);
          exit(1);
        }

        snprintf(topic, sizeof(topic), "%s(%s)", title, section);

        if (!state->wrote_header)
        {
          html_header(state, topic);
        }
        else
        {
	  if (state->in_link)
	  {
	    puts("</a>");
	    state->in_link = false;
	  }

	  if (state->in_block)
	  {
	    printf("</%s>\n", state->in_block);
	    state->in_block = NULL;
	  }
	}

        html_heading(state, MAN_HEADING_TOPIC, topic);

        th_seen = true;
      }
      else if (!th_seen)
      {
        if (!warning)
        {
	  fprintf(stderr, "mantohtml: Need '.TH' before '%s' macro on line %d of '%s'.\n", macro, linenum, filename);
	  warning = true;
	}
        continue;
      }
      else if (!strcmp(macro, ".B"))
      {
        // .B bold text
        man_font_t	font = state->font;
					// Current font

        if (!*lineptr)
        {
          man_gets(fp, line, sizeof(line), &linenum);
          lineptr = line;
        }

        html_font(state, MAN_FONT_BOLD);
        man_puts(state, lineptr);
        html_font(state, font);

        puts(break_text);
        break_text = "";
      }
      else if (!strcmp(macro, ".BI"))
      {
        // .BI bold italic ...
        if (!*lineptr)
        {
          man_gets(fp, line, sizeof(line), &linenum);
          lineptr = line;
        }

        man_xx(state, MAN_FONT_BOLD, MAN_FONT_ITALIC, lineptr);
        puts(break_text);
        break_text = "";
      }
      else if (!strcmp(macro, ".BR"))
      {
        // .BR bold regular ...
        if (!*lineptr)
        {
          man_gets(fp, line, sizeof(line), &linenum);
          lineptr = line;
        }

        man_xx(state, MAN_FONT_BOLD, MAN_FONT_REGULAR, lineptr);
        puts(break_text);
        break_text = "";
      }
      else if (!strcmp(macro, ".EE") || !strcmp(macro, ".fi"))
      {
        // .EE (end example)
        // .fi (resume fill)
        if (!state->in_block || strcmp(state->in_block, "pre"))
        {
          fprintf(stderr, "mantohtml: '%s' with no '.EX' or '.nf' on line %d of '%s'.\n", macro, linenum, filename);
        }
        else
        {
          puts("</pre>");
          state->in_block = NULL;
        }
      }
      else if (!strcmp(macro, ".EX") || !strcmp(macro, ".nf"))
      {
        // .EX (start example)
        // .nf (no fill)
	if (state->in_link)
	{
	  puts("</a>");
	  state->in_link = false;
	}

        if (state->in_block)
          printf("</%s>\n", state->in_block);

        fputs("    <pre>", stdout);
        state->in_block = "pre";
      }
      else if (!strcmp(macro, ".HP"))
      {
        // .HP indent (hanging paragraph)
        char	indent[256];		// Indentation

        if (!parse_measurement(indent, &lineptr, sizeof(indent), 'n'))
          safe_strcpy(indent, "2.5em", sizeof(indent));

	if (state->in_link)
	{
	  puts("</a>");
	  state->in_link = false;
	}

        if (state->in_block)
          printf("</%s>\n", state->in_block);

        printf("    <p style=\"margin-left: %s; text-indent: -%s;\">", indent, indent);
        state->in_block = "p";
      }
      else if (!strcmp(macro, ".I"))
      {
        // .I italic
        man_font_t	font = state->font;
					// Current font

        if (!*lineptr)
        {
          man_gets(fp, line, sizeof(line), &linenum);
          lineptr = line;
        }

        html_font(state, MAN_FONT_ITALIC);
        man_puts(state, lineptr);
        html_font(state, font);

        puts(break_text);
        break_text = "";
      }
      else if (!strcmp(macro, ".IB"))
      {
        // .IB italic bold
        if (!*lineptr)
        {
          man_gets(fp, line, sizeof(line), &linenum);
          lineptr = line;
        }

        man_xx(state, MAN_FONT_ITALIC, MAN_FONT_BOLD, lineptr);
        puts(break_text);
        break_text = "";
      }
      else if (!strcmp(macro, ".IP"))
      {
        // .IP [tag] [indent] (indented paragraph)
        char 	tag[256],		// Tag text
		indent[256] = "";	// Indentation
        const char *list = "";		// List style

        if (parse_value(tag, &lineptr, sizeof(tag)))
          parse_measurement(indent, &lineptr, sizeof(indent), 'n');
        if (!indent[0])
	  safe_strcpy(indent, "2.5em", sizeof(indent));

	if (state->in_link)
	{
	  puts("</a>");
	  state->in_link = false;
	}

        if (state->in_block && strcmp(state->in_block, "ul"))
        {
          printf("</%s>\n", state->in_block);
          state->in_block = NULL;
        }

        if (!state->in_block)
          puts("    <ul>");

        if (strcmp(tag, "\\(bu") && strcmp(tag, "-") && strcmp(tag, "*"))
          list = "list-style-type: none; ";

        html_printf("    <li style=\"%smargin-left: %s;\">", list, indent);
        state->in_block = "ul";

        break_text = "";
      }
      else if (!strcmp(macro, ".IR"))
      {
        // .IR italic regular
        if (!*lineptr)
        {
          man_gets(fp, line, sizeof(line), &linenum);
          lineptr = line;
        }

        man_xx(state, MAN_FONT_ITALIC, MAN_FONT_REGULAR, lineptr);
        puts(break_text);
        break_text = "";
      }
      else if (!strcmp(macro, ".LP") || !strcmp(macro, ".P") || !strcmp(macro, ".PP"))
      {
        // .LP/.P/.PP (paragraph)
	if (state->in_link)
	{
	  puts("</a>");
	  state->in_link = false;
	}

        if (state->in_block)
          printf("</%s>\n", state->in_block);

        fputs("    <p>", stdout);
        state->in_block = "p";
      }
      else if (!strcmp(macro, ".ME") || !strcmp(macro, ".UE"))
      {
        // .ME (end mailto link)
        // .UE (end of URL)
        if (state->in_link)
          puts("</a>");
      }
      else if (!strcmp(macro, ".MT"))
      {
        // .MT email-address
        char	email[1024];		// Email address

        if (parse_value(email, &lineptr, sizeof(email)) && email[0])
        {
          html_printf("<a href=\"mailto:%s\">", email);
          state->in_link = true;
        }
      }
      else if (!strcmp(macro, ".RB"))
      {
        // .RB regular bold
        if (!*lineptr)
        {
          man_gets(fp, line, sizeof(line), &linenum);
          lineptr = line;
        }

        man_xx(state, MAN_FONT_REGULAR, MAN_FONT_BOLD, lineptr);
        puts(break_text);
        break_text = "";
      }
      else if (!strcmp(macro, ".RE"))
      {
        // .RE (relative inset end)
        if (state->indent)
        {
          puts("    </div>");
          state->indent --;
        }
        else
        {
          fprintf(stderr, "mantohtml: Unbalanced '.RE' on line %d of '%s'.\n", linenum, filename);
        }
      }
      else if (!strcmp(macro, ".RS"))
      {
        // .RS (relative inset start)
        char 	indent[256];		// Indentation

        if (!parse_measurement(indent, &lineptr, sizeof(indent), 'n'))
          safe_strcpy(indent, "0.5in", sizeof(indent));

        printf("    <div style=\"margin-left: %s;\">\n", indent);
        state->indent ++;
      }
      else if (!strcmp(macro, ".SB"))
      {
        // .SB small-bold
        man_font_t	font = state->font;
					// Current font

        if (!*lineptr)
        {
          man_gets(fp, line, sizeof(line), &linenum);
          lineptr = line;
        }

        html_font(state, MAN_FONT_SMALL_BOLD);
        man_puts(state, lineptr);
        html_font(state, font);

        puts(break_text);
        break_text = "";
      }
      else if (!strcmp(macro, ".SH"))
      {
        // .SH section-heading
	if (state->in_link)
	{
	  puts("</a>");
	  state->in_link = false;
	}

        if (state->in_block)
        {
          printf("</%s>\n", state->in_block);
          state->in_block = NULL;
        }

        html_heading(state, MAN_HEADING_SECTION, lineptr);
      }
      else if (!strcmp(macro, ".SM"))
      {
        // .SM small
        man_font_t	font = state->font;
					// Current font

        if (!*lineptr)
        {
          man_gets(fp, line, sizeof(line), &linenum);
          lineptr = line;
        }

        html_font(state, MAN_FONT_SMALL);
        man_puts(state, lineptr);
        html_font(state, font);

        puts(break_text);
        break_text = "";
      }
      else if (!strcmp(macro, ".SS"))
      {
        // .SS subsection-heading
	if (state->in_link)
	{
	  puts("</a>");
	  state->in_link = false;
	}

        if (state->in_block)
        {
          printf("</%s>\n", state->in_block);
          state->in_block = NULL;
        }

        html_heading(state, MAN_HEADING_SUBSECTION, lineptr);
      }
      else if (!strcmp(macro, ".SY"))
      {
        // .SY (start of synopsis)
        if (state->in_block)
          printf("</%s>\n", state->in_block);

        fputs("    <p style=\"font-family: monospace;\">", stdout);
        state->in_block = "p";
      }
      else if (!strcmp(macro, ".TP"))
      {
        // .TP [indent]
        char	indent[256];		// Indentation

        if (!parse_measurement(indent, &lineptr, sizeof(indent), 'n'))
          safe_strcpy(indent, "2.5em", sizeof(indent));

	if (state->in_link)
	{
	  puts("</a>");
	  state->in_link = false;
	}

        if (state->in_block)
          printf("</%s>\n", state->in_block);

        printf("    <p style=\"margin-left: %s; text-indent: -%s;\">", indent, indent);
        state->in_block = "p";
        break_text      = "<br>";
      }
      else if (!strcmp(macro, ".UR"))
      {
        // .UR url
        char	url[1024];		// URL value

        if (parse_value(url, &lineptr, sizeof(url)) && url[0])
        {
          html_printf("<a href=\"%s\">", url);
          state->in_link = true;
        }
      }
      else if (!strcmp(macro, ".YS"))
      {
        // .YS (end of synopsis)
        if (!state->in_block || strcmp(state->in_block, "p"))
        {
          fprintf(stderr, "mantohtml: '.YS' seen without prior '.SY' on line %d of '%s'.\n", linenum, filename);
        }
        else
        {
          puts("</p>");
          state->in_block = NULL;
        }
      }
      else if (!strcmp(macro, ".br"))
      {
        // .br
        puts("<br>");
      }
      else if (!strcmp(macro, ".in"))
      {
        // .in indent
        char	indent[256];		// Indentation value

        if (parse_measurement(indent, &lineptr, sizeof(indent), 'm'))
        {
          // Indent...
          printf("    <div style=\"margin-left: %s;\">\n", indent);
          state->indent ++;
        }
        else if (state->indent > 0)
        {
          // Unindent...
          puts("    </div>");
          state->indent --;
        }
        else
        {
          fprintf(stderr, "mantohtml: '.in' seen without prior '.in INDENT' on line %d of '%s'.\n", linenum, filename);
        }
      }
      else if (!strcmp(macro, ".sp"))
      {
        // .sp [N] (vertical space)
        puts("<br>&nbsp;<br>");
      }
      else
      {
        // Something else we don't recognize.
        fprintf(stderr, "mantohtml: Unsupported command/macro '%s' on line %d of '%s'.\n", macro, linenum, filename);
      }
    }
    else if (th_seen)
    {
      // Text that needs to be written...
      if (!state->in_block)
      {
        fputs("<p>", stdout);
        state->in_block = "p";
      }

      man_puts(state, line);

      puts(break_text);
      break_text = "";
    }
    else if (line[0] && !warning)
    {
      fprintf(stderr, "mantohtml: Ignoring text before '.TH' on line %d of '%s'.\n", linenum, filename);
      warning = true;
    }
  }

  fclose(fp);
}


//
// 'html_anchor()' - Convert a string to a HTML anchor.
//

static char *				// O - Anchor
html_anchor(char       *anchor,		// I - Anchor buffer
            const char *s,		// I - String
            size_t     anchorsize)	// I - Size of anchor buffer
{
  char	*ptr,				// Pointer into anchor buffer
	*end;				// Pointer to end of anchor buffer

  for (ptr = anchor, end = anchor + anchorsize - 1; *s && ptr < end; s ++)
  {
    if (isalnum(*s & 255) || *s == '.' || *s == '-')
      *ptr++ = tolower(*s);
    else if (strchr("( \t", *s) != NULL && s[1] && ptr > anchor && ptr[-1] != '-')
      *ptr++ = '-';
  }

  *ptr = '\0';

  return (anchor);
}


//
// 'html_font()' - Change the current font.
//

static void
html_font(man_state_t *state,		// I - Current man state
          man_font_t  font)		// I - New font
{
  static const char * const fonts[] =	// Font tags/elements
  {
    NULL,
    "strong",
    "em",
    "small",
    "small",
    "pre"
  };


  // No-op if the fonts are the same...
  if (state->font == font && state->in_block)
    return;

  // Close prior font as needed, open new font as needed.
  if (state->font)
    printf("</%s>", fonts[state->font]);

  if (!state->in_block)
  {
    fputs("<p>", stdout);
    state->in_block = "p";
  }

  if (font == MAN_FONT_SMALL_BOLD)
    fputs("<small style=\"font-weight: bold;\">", stdout);
  else if (font)
    printf("<%s>", fonts[font]);

  // Save the new font...
  state->font = font;
}


//
// 'html_footer()' - Write the HTML footer.
//

static void
html_footer(man_state_t *state)		// I - Current man state
{
  if (state->wrote_header)
  {
    puts("  </body>");
    puts("</html>");

    state->wrote_header = false;
  }
}


//
// 'html_header()' - Write the HTML header.
//

static void
html_header(man_state_t *state,		// I - Current man state
            const char  *title)		// I - Title
{
  if (state->wrote_header)
    return;

  state->wrote_header = true;

  puts("<!DOCTYPE html>");
  puts("<html>");
  puts("  <head>");
  if (state->css)
  {
    if (!strncmp(state->css, "http://", 7) || !strncmp(state->css, "https://", 8))
    {
      // Reference the stylesheet...
      html_printf("    <link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">\n", state->css);
    }
    else
    {
      // Embed the stylesheet...
      FILE	*fp;			// CSS file
      char	line[1024];		// Line from file

      puts("    <style><!--");

      if ((fp = fopen(state->css, "r")) == NULL)
      {
        perror(state->css);
        exit(1);
      }

      while (fgets(line, sizeof(line), fp))
        fputs(line, stdout);

      fclose(fp);

      puts("--></style>");
    }
  }

  if (state->author)
    html_printf("    <meta name=\"author\" content=\"%s\">\n", state->author);
  if (state->copyright)
    html_printf("    <meta name=\"copyright\" content=\"%s\">\n", state->copyright);
  puts("    <meta name=\"creator\" content=\"mantohtml v" VERSION "\">");
  if (state->subject)
    html_printf("    <meta name=\"subject\" content=\"%s\">\n", state->subject);
  html_printf("    <title>%s</title>\n", state->title ? state->title : title ? title : "Documentation");
  puts("  </head>");
  puts("  <body>");
  if (state->chapter)
  {
    char	anchor[256];		// Anchor for chapter

    html_printf("    <h1 id=\"%s\">%s</h1>\n", html_anchor(anchor, state->chapter, sizeof(anchor)), state->chapter);
  }
}


//
// 'html_heading()' - Write a heading.
//

static void
html_heading(man_state_t   *state,	// I - Current man state
             man_heading_t heading,	// I - Heading level
             const char    *s)		// I - Heading text
{
  int	hlevel;				// HTML heading level
  char	subsection[256],		// Sub-section anchor
	title[256],			// Heading title string
	*titleptr;			// Pointer into heading title


  // Convert heading level enum to HTML
  if (state->chapter)
    hlevel = heading + 2;
  else
    hlevel = heading + 1;

  safe_strcpy(title, s, sizeof(title));

  if (heading > MAN_HEADING_TOPIC)
  {
    // Rewrite the heading text to be capitalized...
    for (titleptr = title; *titleptr; titleptr ++)
    {
      if (isalpha(*titleptr & 255))
      {
	// Start of a word, see if we need to capitalize it
	if (titleptr == title || (strncmp(titleptr, "a ", 2) && strncmp(titleptr, "and ", 4) && strncmp(titleptr, "or ", 3) && strncmp(titleptr, "the ", 4)))
	  *titleptr = toupper(*titleptr);

	while (isalpha(titleptr[1] & 255))
	{
	  titleptr ++;
	  *titleptr = tolower(*titleptr);
	}
      }
    }
  }

  // Close current elements...
  if (state->in_link)
  {
    puts("</a>");
    state->in_link = false;
  }

  if (state->in_block)
  {
    // Close the current paragraph...
    printf("</%s>\n", state->in_block);
    state->in_block = NULL;
  }

  switch (heading)
  {
    case MAN_HEADING_TOPIC :
        html_printf("    <h%d id=\"%s\">", hlevel, html_anchor(state->atopic, s, sizeof(state->atopic)));
        break;

    case MAN_HEADING_SECTION :
        html_printf("    <h%d id=\"%s.%s\">", hlevel, state->atopic, html_anchor(state->asection, s, sizeof(state->asection)));
        break;

    case MAN_HEADING_SUBSECTION :
        html_printf("    <h%d id=\"%s.%s.%s\">", hlevel, state->atopic, state->asection, html_anchor(subsection, s, sizeof(subsection)));
        break;
  }

  man_puts(state, title);
  html_printf("</h%d>\n", hlevel);
}


//
// 'html_printf()' - Output a formatted string, quoting HTML entities as needed.
//
// Note: Currently only supports "%d", "%s", and "%%" format sequences.
//

static void
html_printf(const char *format,		// I - Format string
            ...)			// I - Additional arguments as needed
{
  const char	*start = format;	// Start of current fragment
  va_list	ap;			// Pointer into arguments
  int		ivalue;			// Integer value
  const char	*svalue;		// String value


  // Prep additional arguments...
  va_start(ap, format);

  // Loop through the format string, escaping as needed...
  while (*format)
  {
    if (*format == '%')
    {
      // Format character
      if (format > start)
        fwrite(start, 1, format - start, stdout);

      format ++;
      if (*format == 'd')
      {
        // Insert integer
        format ++;
	start = format;

        ivalue = va_arg(ap, int);

        printf("%d", ivalue);
      }
      else if (*format == 's')
      {
        // Insert string
        format ++;
	start = format;

        svalue = va_arg(ap, const char *);

        if (svalue)
          html_puts(svalue);
      }
      else if (*format == '%')
      {
        // Insert literal %
        start = format;
        format ++;
      }
      else
      {
        // Something else we don't understand...
        fprintf(stderr, "mantohtml: Fatal error - unsupported format sequence '%%%c' used.\n", *format);
        abort();
      }
    }
    else
    {
      // Literal character...
      format ++;
    }
  }

  // Done with additional arguments...
  va_end(ap);

  // Finish off the rest...
  if (format > start)
    fwrite(start, 1, format - start, stdout);
}


//
// 'html_putc()' - Put a single character, using entities as needed.
//

static void
html_putc(int ch)			// I - Character
{
  if (ch == '&')
    fputs("&amp;", stdout);
  else if (ch == '<')
    fputs("&lt;", stdout);
  else if (ch == '\"')
    fputs("&quot;", stdout);
  else
    putchar(ch);
}


//
// 'html_puts()' - Output a literal string, quoting HTML entities as needed.
//

static void
html_puts(const char *s)		// I - String
{
  const char	*start = s;		// Start of current fragment


  // Loop through the string, escaping as needed...
  while (*s)
  {
    if (strchr("&<\"", *s))
    {
      // Character that needs quoting...
      if (s > start)
        fwrite(start, 1, s - start, stdout);

      html_putc(*s ++);
      start = s;
    }
    else
    {
      // Literal character...
      s ++;
    }
  }

  // Finish off the rest...
  if (s > start)
    fwrite(start, 1, s - start, stdout);
}


//
// 'man_gets()' - Get a line from a man page file.
//

static char *				// O  - Line or `NULL` on EOF
man_gets(FILE   *fp,			// I  - File to read from
         char   *buffer,		// I  - Line buffer
         size_t bufsize,		// I  - Size of line buffer
         int    *linenum)		// IO - Line number
{
  int	ch;				// Current character
  char	*bufptr,			// Pointer into line
	*bufend;			// End of line buffer


  // Loop until we get the end of the line...
  bufptr = buffer;
  bufend = buffer + bufsize - 1;

  while ((ch = getc(fp)) != EOF)
  {
    if (ch == '\n')
    {
      // End of line
      *linenum += 1;
      break;
    }
    else if (ch == '\\')
    {
      // Check for \" (comment) or \LF (continuation)
      if ((ch = getc(fp)) == EOF)
        break;

      if (ch == '\n')
      {
        // Continuation
	*linenum += 1;
        continue;
      }
      else if (ch == '\"')
      {
      	// Comment
      	while ((ch = getc(fp)) != EOF)
      	{
      	  if (ch == '\n')
      	  {
	    *linenum += 1;
      	    break;
      	  }
      	}
      	break;
      }
      else
      {
        // Something else we'll interpret at a higher level...
        if (bufptr < bufend)
          *bufptr++ = '\\';
        if (bufptr < bufend)
          *bufptr++ = (char)ch;
      }
    }
    else if (bufptr < bufend)
    {
      // Save current character...
      *bufptr++ = (char)ch;
    }
  }

  *bufptr = '\0';

  // Return the line or NULL on EOF...
  if (ch == EOF)
    return (NULL);
  else
    return (buffer);
}



//
// 'man_puts()' - Output a man string, quoting with HTML entities as needed.
//

static void
man_puts(man_state_t *state,		// I - Current man state
         const char  *s)		// I - String
{
  const char	*start = s;		// Start of current string fragment


  // Scan the string for special characters and write things out...
  while (*s)
  {
    if (*s == '\\' && s[1])
    {
      // Escaped sequence
      if (s > start)
      {
        // Write current fragment...
        fwrite(start, 1, s - start, stdout);
        start = s;
      }

      s ++;

      if (*s == 'f' && s[1])
      {
        s ++;

        switch (*s)
        {
          case 'R' :
          case 'P' :
              html_font(state, MAN_FONT_REGULAR);
              break;

          case 'b' :
          case 'B' :
              html_font(state, MAN_FONT_BOLD);
              break;

          case 'i' :
          case 'I' :
              html_font(state, MAN_FONT_ITALIC);
              break;

          default :
              fprintf(stderr, "mantohtml: Unknown font '\\f%c' ignored.\n", *s);
              break;
        }

	s ++;
        start = s;
      }
      else if (*s == '*' && s[1])
      {
        // Substitute macro...
        s ++;

        switch (*s++)
        {
          case 'R' :
              fputs("&reg;", stdout);
              break;

          case '(' :
	      if (!strncmp(s, "aq", 2))
	      {
		putchar('\'');
		s += 2;
	      }
	      else if (!strncmp(s, "dq", 2))
	      {
		fputs("&quot;", stdout);
		s += 2;
	      }
	      else if (!strncmp(s, "lq", 2))
	      {
		fputs("&ldquo;", stdout);
		s += 2;
	      }
	      else if (!strncmp(s, "rq", 2))
	      {
		fputs("&rdquo;", stdout);
		s += 2;
	      }
              else if (!strncmp(s, "Tm", 2))
              {
                fputs("<sup>TM</sup>", stdout);
		s += 2;
	      }
              else
              {
                fprintf(stderr, "mantohtml: Unknown macro '\\*(%c%c' ignored.\n", s[0], s[1]);
                if (*s && s[1])
                  s += 2;
              }
              break;

          default :
              fprintf(stderr, "mantohtml: Unknown macro '\\*%c' ignored.\n", *s);
              if (*s)
	        s ++;
              break;
        }

	start = s;
      }
      else if (*s == '(')
      {
	if (!strncmp(s, "(bu", 3))
	{
	  // Bullet
	  fputs("&middot;", stdout);
	  s += 3;
	  start = s;
	}
        else if (!strncmp(s, "(em", 3))
        {
          fputs("&mdash;", stdout);
          s += 3;
          start = s;
        }
        else if (!strncmp(s, "(en", 3))
        {
          fputs("&ndash;", stdout);
          s += 3;
          start = s;
        }
        else if (!strncmp(s, "(ga", 3))
        {
          putchar('`');
          s += 3;
          start = s;
        }
        else if (!strncmp(s, "(ha", 3))
        {
          putchar('^');
          s += 3;
          start = s;
        }
        else if (!strncmp(s, "(ti", 3))
        {
          putchar('~');
          s += 3;
          start = s;
        }
      }
      else if (*s == '[')
      {
        // Substitute escaped character...
        s ++;

	if (!strncmp(s, "aq]", 3))
	{
	  putchar('\'');
	  s += 3;
	  start = s;
	}
	else if (!strncmp(s, "co]", 3))
	{
	  fputs("&copy;", stdout);
	  s += 3;
	  start = s;
	}
	else if (!strncmp(s, "cq]", 3))
	{
	  fputs("&rsquo;", stdout);
	  s += 3;
	  start = s;
	}
	else if (!strncmp(s, "de]", 3))
	{
	  fputs("&deg;", stdout);
	  s += 3;
	  start = s;
	}
	else if (!strncmp(s, "dq]", 3))
	{
	  fputs("&quot;", stdout);
	  s += 3;
	  start = s;
	}
	else if (!strncmp(s, "lq]", 3))
	{
	  fputs("&ldquo;", stdout);
	  s += 3;
	  start = s;
	}
	else if (!strncmp(s, "mc]", 3))
	{
	  fputs("&mu;", stdout);
	  s += 3;
	  start = s;
	}
	else if (!strncmp(s, "oq]", 3))
	{
	  fputs("&lsquo;", stdout);
	  s += 3;
	  start = s;
	}
        else if (!strncmp(s, "rg]", 3))
	{
	  fputs("&reg;", stdout);
	  s += 3;
	  start = s;
	}
	else if (!strncmp(s, "rq]", 3))
	{
	  fputs("&rdquo;", stdout);
	  s += 3;
	  start = s;
	}
	else if (!strncmp(s, "tm]", 3))
	{
	  fputs("<sup>TM</sup>", stdout);
	  s += 3;
	  start = s;
	}
      }
      else if (isdigit(s[0] & 255) && isdigit(s[1] & 255) && isdigit(s[2] & 255))
      {
	printf("&#%d;", ((s[0] - '0') * 8 + s[1] - '0') * 8 + s[2] - '0');
	s += 3;
	start = s;
      }
      else
      {
        if (*s != '\\' && *s != '\"' && *s != '\'' && *s != '-' && *s != 'e' && *s != ' ')
        {
          fprintf(stderr, "mantohtml: Unrecognized escape '\\%c' ignored.\n", *s);
          putchar('\\');
        }

        if (*s == 'e')
        {
          // Escape sequence for backslash...
          s ++;
          putchar('\\');
        }
        else
        {
          // Something else that is written as-is...
          html_putc(*s++);
        }

        start = s;
      }
    }
    else if (!strncmp(s, "http://", 7) || !strncmp(s, "https://", 8))
    {
      // Embed URL...
      char	url[1024],		// URL string
		*urlptr;		// Pointer into URL string

      if (s > start)
      {
        // Write current fragment...
        fwrite(start, 1, s - start, stdout);
      }

      for (urlptr = url; *s && !isspace(*s & 255) && urlptr < (url + sizeof(url) - 1); s ++)
      {
        if (strchr(",.)", *s) && strchr(",. \n\r\t", s[1]))
        {
          // End of URL
          break;
        }
        else if (*s == '\\' && s[1])
        {
          // Escaped character
          s ++;
          *urlptr++ = *s;
        }
        else
        {
          // Regular character...
          *urlptr++ = *s;
        }
      }

      *urlptr = '\0';
      html_printf("<a href=\"%s\">%s</a>", url, url);
      start = s;
    }
    else if (strchr("<\"&", *s))
    {
      // Quoted HTML character...
      if (s > start)
      {
	// Write current fragment...
	fwrite(start, 1, s - start, stdout);
      }

      html_putc(*s++);
      start = s;
    }
    else
    {
      // Literal character...
      s ++;
    }
  }

  if (s > start)
  {
    // Write current fragment...
    fwrite(start, 1, s - start, stdout);
  }
}


//
// 'man_xx()' - Parse font macro.
//

static void
man_xx(man_state_t *state,		// I - Current man state
       man_font_t  a,			// I - First font
       man_font_t  b,			// I - Second font
       const char  *line)		// I - Line
{
  char		word[256];		// Word from line
  man_font_t	font = state->font;	// Current font
  bool		use_a = true;		// Use the first font?


  // Loop until all words are written
  while (parse_value(word, &line, sizeof(word)))
  {
    bool	have_link = false;	// Have a link?

    if (a == MAN_FONT_BOLD && b == MAN_FONT_REGULAR && use_a)
    {
      char	section[256],		// Section (regular portion)
		*secptr;		// Pointer into section
      const char *saveline = line;	// Saved line pointer

      if (parse_value(section, &saveline, sizeof(section)) && section[0] == '(' && isdigit(section[1] & 255) && (secptr = strchr(section, ')')) != NULL)
      {
        // Possibly convert ".BR name (section)" to hyperlink...
        char	filename[1024];		// Man source file

        *secptr = '\0';
        snprintf(filename, sizeof(filename), "%s/%s.%s", state->basepath, word, section + 1);
        if (!access(filename, 0))
        {
          // Have a "name.section" source file...
          html_printf("<a href=\"%s.html\">", word);
          have_link = true;
        }
      }
    }

    html_font(state, use_a ? a : b);
    man_puts(state, word);

    if (have_link && parse_value(word, &line, sizeof(word)))
    {
      // Show man page section and close the link...
      html_font(state, b);
      man_puts(state, word);
      html_printf("</a>");
    }
    else
    {
      // Alternate fonts...
      use_a = !use_a;
    }
  }

  // Restore the original font...
  html_font(state, font);
  putchar('\n');
}


//
// 'parse_measurement()' - Parse a measurement value from the line.
//

static char *				// O  - String value
parse_measurement(
    char       *buffer,			// I  - String buffer
    const char **line,			// IO - Pointer into line
    size_t     bufsize,			// I  - Size of string buffer
    char       defunit)			// I  - Default units
{
  char	*bufptr,			// Pointer into buffer
	*bufend,			// End of buffer
	unit;				// Unit


  // First get a value...
  if (!parse_value(buffer, line, bufsize))
    return (NULL);

  // Then convert the value to a CSS measurement
  //
  //    ##c -> ##cm (centimeters)
  //    ##f -> ##% (1/65536 of font size - divide by 655.36)
  //    ##i -> ##in (inches)
  //    ##m -> ##em (em's)
  //    ##M -> ##em (1/100th em)
  //    ##n -> ##en (en's)
  //    ##P -> ##pi (picas)
  //    ##p -> ##pt (points)
  //    ##s -> ##% (multiple of font size - multiply by 100)
  //    ##u -> ##px (pixel)
  //    ##v -> ## (multiple of line height)
  if ((bufptr = buffer + strlen(buffer) - 1) < buffer)
    return (NULL);

  bufend = buffer + bufsize - 1;

  if (isalpha(*bufptr & 255))
    unit = *bufptr;
  else
    unit = defunit;

  switch (unit)
  {
    case 'c' : // Centimaters
        if ((bufptr + 1) >= bufend)
          return (NULL);

	// Convert to ##cm
	bufptr[1] = 'm';
	bufptr[2] = '\0';
        break;

    case 'f' : // 1/65536 of font size
	// Convert to ##%
	snprintf(buffer, bufsize, "%.1f%%", 100.0 * atof(buffer) / 65536.0);
        break;

    case 'i' : // Inches
        if ((bufptr + 1) >= bufend)
          return (NULL);

	// Convert to ##in
	bufptr[1] = 'n';
	bufptr[2] = '\0';
        break;

    case 'm' : // Ems
        if ((bufptr + 1) >= bufend)
          return (NULL);

	// Convert to ##em
	bufptr[0] = 'e';
	bufptr[1] = 'm';
	bufptr[2] = '\0';
        break;

    case 'M' : // 1/100th ems
	// Convert to ##em
	snprintf(buffer, bufsize, "%.2fem", 0.01 * atof(buffer));
        break;

    case 'n' : // Ens (1/2 em)
	// Convert to ##em
	snprintf(buffer, bufsize, "%gem", 0.5 * atof(buffer));
        break;

    case 'P' : // Picas
        if ((bufptr + 1) >= bufend)
          return (NULL);

	// Convert to ##pc
	bufptr[0] = 'p';
	bufptr[1] = 'c';
	bufptr[2] = '\0';
        break;

    case 'p' : // Points
        if ((bufptr + 1) >= bufend)
          return (NULL);

	// Convert to ##pt
	bufptr[1] = 't';
	bufptr[2] = '\0';
        break;

    case 's' : // Multiple of font size
	// Convert to ##%
	snprintf(buffer, bufsize, "%.1f%%", 100.0 * atof(buffer));
        break;

    case 'u' : // Device unit (pixel)
        if ((bufptr + 1) >= bufend)
          return (NULL);

	// Convert to ##px
	bufptr[0] = 'p';
	bufptr[1] = 'x';
	bufptr[2] = '\0';
        break;

    case 'v' : // Multiple of line height
	// Convert to ##
	*bufptr = '\0';
        break;

    default :
        return (NULL);
  }

  return (buffer);
}


//
// 'parse_value()' - Parse a value from the line.
//

static char *				// O  - String value
parse_value(char       *buffer,		// I  - String buffer
            const char **line,		// IO - Pointer into line
            size_t     bufsize)		// I  - Size of string buffer
{
  char		*bufptr,		// Pointer into buffer
		*bufend;		// End of buffer
  const char	*lineptr;		// Pointer into line


  // Save pointers...
  lineptr = *line;
  bufptr  = buffer;
  bufend  = buffer + bufsize - 1;

  // Skip leading whitespace...
  while (*lineptr && isspace(*lineptr & 255))
    lineptr ++;

  if (!*lineptr)
  {
    *bufptr = '\0';
    return (NULL);
  }

  // Parse the value...
  if (*lineptr == '\"')
  {
    // Quoted value
    lineptr ++;
    while (*lineptr && *lineptr != '\"')
    {
      if (bufptr < bufend)
        *bufptr++ = *lineptr;

      if (*lineptr == '\\' && lineptr[1])
      {
        lineptr ++;

	if (bufptr < bufend)
	  *bufptr++ = *lineptr;
      }

      lineptr ++;
    }

    if (*lineptr)
      lineptr ++;
  }
  else
  {
    // Unquoted value
    while (*lineptr && !isspace(*lineptr & 255))
    {
      if (bufptr < bufend)
        *bufptr++ = *lineptr;

      if (*lineptr == '\\' && lineptr[1])
      {
        // Make sure we don't lose an escaped value...
        lineptr ++;

	if (bufptr < bufend)
	  *bufptr++ = *lineptr;
      }

      lineptr ++;
    }
  }

  // Skip trailing whitespace...
  while (*lineptr && isspace(*lineptr & 255))
    lineptr ++;

  // Store where we ended up...
  *line   = lineptr;
  *bufptr = '\0';

  return (buffer);
}


//
// 'safe_strcpy()' - Safely copy a string.
//

static void
safe_strcpy(char       *dst,		// I - Destination string buffer
            const char *src,		// I - Source string
            size_t     dstsize)		// I - Destination buffer size
{
  size_t	srclen;			// Length of source string


  if ((srclen = strlen(src)) >= dstsize)
    srclen = dstsize - 1;

  memcpy(dst, src, srclen);
  dst[srclen] = '\0';
}


//
// 'usage()' - Show program usage.
//

static int				// O - Exit status
usage(const char *opt)			// I - Unknown option
{
  puts("Usage: mantohtml [OPTIONS] MAN-FILE [... MAN-FILE] >HTML-FILE");
  puts("Options:");
  puts("   --author 'AUTHOR'        Set author metadata");
  puts("   --chapter 'CHAPTER'      Set chapter (H1 heading)");
  puts("   --copyright 'COPYRIGHT'  Set copyright metadata");
  puts("   --css CSS-FILE-OR-URL    Use named stylesheet");
  puts("   --help                   Show help");
  puts("   --subject 'SUBJECT'      Set subject metadata");
  puts("   --title 'TITLE'          Set output title");
  puts("   --version                Show version");

  return (1);
}
