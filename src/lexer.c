#include <ctype.h>
#include <stdio.h>

#include "lexer.h"

static void add_word(token_list_t *list, const char *word)
{
    if (word[0] != '\0')
    {
        token_add(list, TOKEN_WORD, word);
    }
}

int lexer(const char *input, token_list_t *list)
{
    int i = 0;

    if (input == NULL || list == NULL)
    {
        return -1;
    }

    token_list_init(list);

    while (input[i] != '\0')
    {
        char c = input[i];

        /* Skip whitespace */
        if (isspace((unsigned char)c))
        {
            i++;
            continue;
        }

        /* Pipe */
        if (c == '|')
        {
            token_add(list, TOKEN_PIPE, "|");
            i++;
            continue;
        }

        /* Input redirection */
        if (c == '<')
        {
            token_add(list, TOKEN_INPUT, "<");
            i++;
            continue;
        }

        /* Output / append redirection */
        if (c == '>')
        {
            if (input[i + 1] == '>')
            {
                token_add(list, TOKEN_APPEND, ">>");
                i += 2;
            }
            else
            {
                token_add(list, TOKEN_OUTPUT, ">");
                i++;
            }

            continue;
        }

        /* Background */
        if (c == '&')
        {
            token_add(list, TOKEN_BACKGROUND, "&");
            i++;
            continue;
        }

        /* Build WORD token */
        char word[MAX_TOKEN_LEN];
        int j = 0;

        while (input[i] != '\0')
        {
            c = input[i];

            /* Delimiters */
            if (isspace((unsigned char)c) ||
                c == '|' ||
                c == '<' ||
                c == '>' ||
                c == '&')
            {
                break;
            }

            /* Escape character */
            if (c == '\\')
            {
                i++;

                if (input[i] == '\0')
                {
                    break;
                }

                if (j < MAX_TOKEN_LEN - 1)
                {
                    word[j++] = input[i];
                }

                i++;
                continue;
            }

            /* Single quote */
            if (c == '\'')
            {
                i++;

                while (input[i] != '\0' && input[i] != '\'')
                {
                    if (j < MAX_TOKEN_LEN - 1)
                    {
                        word[j++] = input[i];
                    }

                    i++;
                }

                if (input[i] != '\'')
                {
                    fprintf(stderr,
                            "Lexer Error: Unterminated single quote\n");

                    return -1;
                }

                i++;
                continue;
            }

            /* Double quote */
            if (c == '"')
            {
                i++;

                while (input[i] != '\0' && input[i] != '"')
                {
                    if (input[i] == '\\' && input[i + 1] != '\0')
                    {
                        i++;
                    }

                    if (j < MAX_TOKEN_LEN - 1)
                    {
                        word[j++] = input[i];
                    }

                    i++;
                }

                if (input[i] != '"')
                {
                    fprintf(stderr,
                            "Lexer Error: Unterminated double quote\n");

                    return -1;
                }

                i++;
                continue;
            }

            /* Normal character */
            if (j < MAX_TOKEN_LEN - 1)
            {
                word[j++] = c;
            }

            i++;
        }

        word[j] = '\0';

        add_word(list, word);
    }

    /* Add END token */
    token_add(list, TOKEN_END, "END");

    return 0;
}
