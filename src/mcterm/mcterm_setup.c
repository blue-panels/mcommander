/*
   Terminal widget mcterm for the M-Commander
   The shell integration mcterm puts into every shell it starts

   Copyright (C) 2026
   Ilia Maslakov il.smind@gmail.com

   This file is part of M-Commander.

   M-Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   M-Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see https://www.gnu.org/licenses/.
 */

/** \file mcterm_setup.c
 *  \brief Source: shell integration setup strings written into the pty
 *
 *  The strings here make the shells report their working directory (OSC 7) and
 *  mark their prompts and commands (OSC 133). Every mark carries the session
 *  token, so the host can tell its own shell from any other one. The POSIX
 *  shells get them typed in at their first prompt.
 *
 *  bash, zsh and fish are not typed into: what is typed at the prompt goes
 *  through the key bindings of the user, and it is also saved in the shell
 *  history. bash and zsh are started with startup files of their own; they live
 *  in mc's data directory and are the same for every session, so the token of
 *  the session reaches them in the environment, as $MC_TERM_TOKEN. fish takes
 *  the whole integration, token and all, in its -C option, which it runs after
 *  the configuration file of the user and before the first prompt.
 */

#include <config.h>

#include "lib/global.h"
#include "lib/fileloc.h"   // MC_ZDOTDIR_SUBDIR
#include "lib/mcconfig.h"  // mc_config_get_data_path()
#include "lib/shell.h"     // shell_type_t, SHELL_*
#include "lib/util.h"      // exist_file()

#include "mcterm.h"        // MCTERM_OSC7_TOKEN_PREFIX
#include "mcterm_proto.h"  // MCTERM_MARK_TOKEN_KEY
#include "mcterm_setup.h"

/*** file scope macros ***************************************************************************/

/* The session token tails every OSC the strings below make the shell send:
   mc=<token> on a semantic mark, ?mc=<token> on an OSC 7. */
#define MC_MARK_TOK MCTERM_MARK_TOKEN_KEY MCTERM_TOKEN_PLACEHOLDER
#define MC_OSC7_TOK MCTERM_OSC7_TOKEN_PREFIX MCTERM_TOKEN_PLACEHOLDER

/*** file scope type declarations ****************************************************************/

struct mcterm_shell_rc
{
    GPtrArray *env;   // name, value, name, value ... the child starts with
    GPtrArray *args;  // what the shell's own argv gets after argv[0]
};

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */

/* The integration zsh reads from the startup files below. */
static const char *
mcterm_zsh_integration (void)
{
    static const char setup[] =
        // Started by hand, and not by the terminal: there is no session to report to.
        "[[ -n $MC_TERM_TOKEN ]] || return 0\n"
        "__mc_tok=$MC_TERM_TOKEN\n"
        /* A command of the terminal's own goes in behind a leading space, and this is
           what keeps such a line out of the user's history. */
        "setopt hist_ignore_space\n"
        "__mc_pe(){\n"
        " local s=$1 o='' c i\n"
        " for (( i=1; i<=${#s}; i++ )); do\n"
        "  c=${s[i]}\n"
        "  case $c in\n"
        "  [a-zA-Z0-9/_~.-]) o+=$c;;\n"
        "  *) printf -v o '%s%%%02X' \"$o\" \"'$c\";;\n"
        "  esac\n"
        " done\n"
        " printf %s \"$o\"\n"
        "}\n"
        "__mc_precmd(){\n"
        " local e=$?\n"
        " printf '\\033]133;D;%s;" MCTERM_MARK_TOKEN_KEY "%s\\007' \"$e\" \"$__mc_tok\"\n"
        " printf '\\033]7;file://%s" MCTERM_OSC7_TOKEN_PREFIX "%s\\007'"
        " \"$(__mc_pe \"$PWD\")\" \"$__mc_tok\"\n"
        /* An assignment to PROMPT throws the marks away: put them back. */
        " [[ $PROMPT == *\"133;B;" MCTERM_MARK_TOKEN_KEY "$__mc_tok\"* ]]"
        " || PROMPT=$'%{\\e]133;A;" MCTERM_MARK_TOKEN_KEY "'$__mc_tok$'\\a%}'$PROMPT"
        "$'%{\\e]133;B;" MCTERM_MARK_TOKEN_KEY "'$__mc_tok$'\\a%}'\n"
        "}\n"
        "precmd_functions+=(__mc_precmd)\n"
        "__mc_preexec(){ printf '\\033]133;C;" MCTERM_MARK_TOKEN_KEY "%s\\007' \"$__mc_tok\" }\n"
        "preexec_functions+=(__mc_preexec)\n"
        "printf '\\033]7;file://__mc_sync__/" MCTERM_OSC7_TOKEN_PREFIX "%s\\007' \"$__mc_tok\"\n";

    return setup;
}

/* --------------------------------------------------------------------------------------------- */

/* The integration bash reads from the startup file below. */
static const char *
mcterm_bash_integration (void)
{
    static const char setup[] =
        // Read by hand, and not by the terminal: there is no session to report to.
        "[[ -n ${MC_TERM_TOKEN-} ]] || return 0\n"
        "__mc_tok=$MC_TERM_TOKEN\n"
        "__mc_pe(){\n"
        " local s=$1 o= i c\n"
        " for ((i = 0; i < ${#s}; i++)); do\n"
        "  c=${s:i:1}\n"
        "  case $c in\n"
        "  [a-zA-Z0-9/_~.-]) o+=$c;;\n"
        "  *) printf -v o '%s%%%02X' \"$o\" \"'$c\";;\n"
        "  esac\n"
        " done\n"
        " printf '%s' \"$o\"\n"
        "}\n"
        "__mc_pc(){\n"
        " local e=$?\n"
        " printf '\\033]133;D;%s;" MCTERM_MARK_TOKEN_KEY "%s\\007' \"$e\" \"$__mc_tok\"\n"
        " printf '\\033]7;file://%s" MCTERM_OSC7_TOKEN_PREFIX "%s\\007'"
        " \"$(__mc_pe \"$PWD\")\" \"$__mc_tok\"\n"
        /* Setting PS1 is an everyday thing to do at a prompt, and it would throw the marks
           away. Put them back whenever they are gone. */
        " case ${PS1-} in\n"
        " *\"133;B;" MCTERM_MARK_TOKEN_KEY "$__mc_tok\"*) ;;\n"
        " *) PS1=\"\\[\\e]133;A;" MCTERM_MARK_TOKEN_KEY "$__mc_tok\\a\\]${PS1-}"
        "\\[\\e]133;B;" MCTERM_MARK_TOKEN_KEY "$__mc_tok\\a\\]\";;\n"
        " esac\n"
        " return $e\n"
        "}\n"
        /* PROMPT_COMMAND can be an array from bash 5.1 on; before that only its first
           element is run. */
        "if (( BASH_VERSINFO[0] * 100 + BASH_VERSINFO[1] >= 501 ))"
        " && [[ $(declare -p PROMPT_COMMAND 2>/dev/null) == 'declare -a'* ]]; then\n"
        " PROMPT_COMMAND+=(__mc_pc)\n"
        /* A newline and not a ';' before the hook: PROMPT_COMMAND of the user may end in a ';'
           of its own, and '; ;' is a syntax error. */
        "elif [[ -n ${PROMPT_COMMAND-} ]]; then\n"
        " PROMPT_COMMAND=\"$PROMPT_COMMAND\n"
        "__mc_pc\"\n"
        "else\n"
        " PROMPT_COMMAND=__mc_pc\n"
        "fi\n"
        "PS0=\"\\[\\e]133;C;" MCTERM_MARK_TOKEN_KEY "$__mc_tok\\a\\]${PS0-}\"\n"
        "printf '\\033]7;file://__mc_sync__/" MCTERM_OSC7_TOKEN_PREFIX "%s\\007' \"$__mc_tok\"\n";

    return setup;
}

/* --------------------------------------------------------------------------------------------- */

/* The integration fish runs from its -C option, with MCTERM_TOKEN_PLACEHOLDER where the token
   goes. fish has an event for everything, so nothing here replaces what the user set: the
   prompt function is copied and called, the rest hangs off events. */
static const char *
mcterm_fish_integration (void)
{
    static const char setup[] =
        "functions -q __mc_orig_prompt; or functions -c fish_prompt __mc_orig_prompt; "
        "function fish_prompt; printf "
        "'\\033]133;A;" MC_MARK_TOK "\\a'; __mc_orig_prompt; "
        "printf '\\033]133;B;" MC_MARK_TOK "\\a'; end; "
        "function __mc_preexec --on-event fish_preexec; "
        "printf '\\033]133;C;" MC_MARK_TOK "\\a'; end; "
        "function __mc_postexec --on-event fish_postexec; "
        "printf '\\033]133;D;%s;" MC_MARK_TOK "\\a' $status; end; "
        "function __mc_cwd --on-event fish_prompt; "
        "printf '\\033]7;file://%s" MC_OSC7_TOK "\\a' (string escape --style=url -- $PWD); end; "
        "printf "
        "'\\033]7;file://__mc_sync__/" MC_OSC7_TOK "\\a'";

    return setup;
}

/* --------------------------------------------------------------------------------------------- */

/* Put one startup file where the shell will read it. It is written only when it is not
   already there word for word, so that a running shell does not have the file pulled from
   under it, and an mc that has not been updated does not rewrite it at every start. */
static gboolean
mcterm_rc_install (const char *dir, const char *name, const char *contents)
{
    char *path = g_build_filename (dir, name, (char *) NULL);
    char *found = NULL;
    gboolean ok = TRUE;

    if (!g_file_get_contents (path, &found, NULL, NULL) || strcmp (found, contents) != 0)
        ok = g_file_set_contents (path, contents, -1, NULL);

    g_free (found);
    g_free (path);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

static void
mcterm_rc_env (mcterm_shell_rc_t *rc, const char *name, const char *value)
{
    g_ptr_array_add (rc->env, g_strdup (name));
    g_ptr_array_add (rc->env, g_strdup (value));
}

/* --------------------------------------------------------------------------------------------- */

/* What every stub below starts with: the user had ZDOTDIR the way the environment says. */
#define MC_ZSH_RESTORE                                                                             \
    "if [ -n \"${MC_ZDOTDIR-}\" ]; then\n"                                                         \
    " ZDOTDIR=$MC_ZDOTDIR\n"                                                                       \
    "else\n"                                                                                       \
    " unset ZDOTDIR\n"                                                                             \
    "fi\n"

/* A file zsh reads before .zshrc. It hands ZDOTDIR back for the user's own file of that name
   and then takes it again: the files after it would otherwise be looked for in the user's
   directory, and ours never read. */
static char *
mcterm_rc_zsh_early (const char *name)
{
    return g_strconcat ("# M-Commander: the terminal starts the shell here.\n"
                        "__mc_zdotdir=$ZDOTDIR\n" MC_ZSH_RESTORE "if [ -r \"${ZDOTDIR:-$HOME}/",
                        name,
                        "\" ]; then\n"
                        " . \"${ZDOTDIR:-$HOME}/",
                        name,
                        "\"\n"
                        "fi\n"
                        "ZDOTDIR=$__mc_zdotdir\n"
                        "unset __mc_zdotdir\n",
                        (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

/* zsh reads a whole directory, not a file: every startup file it would have read before .zshrc
   has to be stood in for, and each of them hands the user's own back its say. What the user had
   on ZDOTDIR comes in the environment, so that the files are the same for everyone.

   .zlogin and .zlogout need no stub of ours: they are read after .zshrc, which has handed
   ZDOTDIR back for good, so zsh looks for them where the user keeps them. */
static gboolean
mcterm_rc_zsh (mcterm_shell_rc_t *rc, const char *dir)
{
    const char *orig = g_getenv ("ZDOTDIR");
    char *zshenv = mcterm_rc_zsh_early (".zshenv");
    char *zprofile = mcterm_rc_zsh_early (".zprofile");
    char *zshrc;
    gboolean ok;

    /* The integration goes in last, after everything the user's own .zshrc does, so that a
       prompt or a hook set there is the one it wraps. */
    zshrc = g_strconcat ("# M-Commander: the terminal starts the shell here.\n" MC_ZSH_RESTORE
                         "unset MC_ZDOTDIR\n"
                         "if [ -r \"${ZDOTDIR:-$HOME}/.zshrc\" ]; then\n"
                         " . \"${ZDOTDIR:-$HOME}/.zshrc\"\n"
                         "fi\n",
                         mcterm_zsh_integration (), (char *) NULL);

    ok = (mcterm_rc_install (dir, ".zshenv", zshenv)
          && mcterm_rc_install (dir, ".zprofile", zprofile)
          && mcterm_rc_install (dir, ".zshrc", zshrc));
    if (ok)
    {
        mcterm_rc_env (rc, "ZDOTDIR", dir);
        if (orig != NULL)
            mcterm_rc_env (rc, "MC_ZDOTDIR", orig);
    }

    g_free (zshenv);
    g_free (zprofile);
    g_free (zshrc);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/* bash reads one startup file, the one --rcfile names, and that is ours. It reads the file of
   the user first: the bashrc in mc's data directory when there is one, as mc has always done,
   ~/.bashrc when not. The integration goes in after it, so that a prompt or a PROMPT_COMMAND
   set there is the one it wraps. Nothing is typed at the prompt, so nothing of ours goes
   through readline or into the history. */
static gboolean
mcterm_rc_bash (mcterm_shell_rc_t *rc, const char *dir)
{
    char *custom = mc_config_get_full_path (MC_BASHRC_CUSTOM_PROFILE_FILE);
    char *inputrc = mc_config_get_full_path (MC_INPUTRC_FILE);
    char *user_rc;
    char *bashrc;
    gboolean ok;

    if (custom != NULL && exist_file (custom))
        user_rc = g_shell_quote (custom);
    else
        user_rc = g_strdup ("\"$HOME/" MC_BASHRC_DEFAULT_PROFILE_FILE "\"");

    bashrc = g_strconcat ("# M-Commander: the terminal starts the shell here.\n"
                          "if [ -r ",
                          user_rc,
                          " ]; then\n"
                          " . ",
                          user_rc,
                          "\n"
                          "fi\n",
                          mcterm_bash_integration (), (char *) NULL);

    ok = mcterm_rc_install (dir, MC_BASHRC_TERM_FILE, bashrc);
    if (ok)
    {
        g_ptr_array_add (rc->args, g_strdup ("--rcfile"));
        g_ptr_array_add (rc->args, g_build_filename (dir, MC_BASHRC_TERM_FILE, (char *) NULL));
        if (inputrc != NULL && exist_file (inputrc))
            mcterm_rc_env (rc, "INPUTRC", inputrc);
    }

    g_free (bashrc);
    g_free (user_rc);
    g_free (inputrc);
    g_free (custom);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/* fish reads the integration from its own command line: -C runs it after the user's config.fish
   and before the first prompt. Nothing is typed at the prompt, so nothing of ours is saved in
   the history of the user. The token goes in the argument itself, no environment needed. */
static mcterm_shell_rc_t *
mcterm_rc_fish (const char *token)
{
    mcterm_shell_rc_t *rc;

    rc = g_new0 (mcterm_shell_rc_t, 1);
    rc->env = g_ptr_array_new_with_free_func (g_free);
    rc->args = g_ptr_array_new_with_free_func (g_free);

    g_ptr_array_add (rc->args, g_strdup ("-C"));
    g_ptr_array_add (rc->args, mcterm_setup_with_token (mcterm_fish_integration (), token));

    return rc;
}

/* --------------------------------------------------------------------------------------------- */

/*** public functions ****************************************************************************/

/* --------------------------------------------------------------------------------------------- */

/**
 * The shell integration to type into a shell of type @shell_type, with MCTERM_TOKEN_PLACEHOLDER
 * standing where the per-session token goes; NULL for a shell that is started with the
 * integration already in it, see mcterm_shell_rc_new(), and for one the terminal cannot drive.
 */

char *
mcterm_setup_with_token (const char *setup, const char *token)
{
    static const size_t ph_len = sizeof (MCTERM_TOKEN_PLACEHOLDER) - 1;
    GString *out;
    const char *p = setup;

    out = g_string_sized_new (strlen (setup) + 64);

    while (TRUE)
    {
        const char *ph = strstr (p, MCTERM_TOKEN_PLACEHOLDER);

        if (ph == NULL)
        {
            g_string_append (out, p);
            break;
        }

        g_string_append_len (out, p, ph - p);
        g_string_append (out, token);
        p = ph + ph_len;
    }

    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */

const char *
mcterm_shell_setup (shell_type_t shell_type)
{
    switch (shell_type)
    {
    case SHELL_BASH:
        // bash is started with a startup file of its own, see mcterm_shell_rc_new().
        return NULL;

    case SHELL_ZSH:
        // zsh is started with startup files of its own, see mcterm_shell_rc_new().
        return NULL;

    case SHELL_FISH:
        // fish takes the integration on the command line, see mcterm_shell_rc_new().
        return NULL;

    case SHELL_SH:
    case SHELL_DASH:
    case SHELL_ASH_BUSYBOX:
    {
        /* No precmd or preexec hook here, but PS1 is expanded again at every prompt: a
         * command substitution in it reports the directory and the exit code and lays the
         * prompt marks down itself. There is no command-start (C) mark, nothing runs
         * between the prompt and the command for it to hang on. */
        static const char setup[] =
            "__mc_pe(){ s=$1; o=;"
            " while [ -n \"$s\" ]; do c=${s%\"${s#?}\"};"
            " case $c in [a-zA-Z0-9/_~.-]) o=$o$c;;"
            " *) o=$(printf '%s%%%02X' \"$o\" \"'$c\");; esac;"
            " s=${s#?}; done; printf %s \"$o\"; };"
            " __mc_a(){"
            " printf '\\033]133;D;%s;" MC_MARK_TOK "\\007' \"$1\";"
            " printf '\\033]7;file://%s" MC_OSC7_TOK "\\007' \"$(__mc_pe \"$PWD\")\";"
            " printf '\\033]133;A;" MC_MARK_TOK "\\007'; };"
            " __mc_b(){ printf '\\033]133;B;" MC_MARK_TOK "\\007'; };"
            " PS1='$(__mc_a $?)'\"$PS1\"'$(__mc_b)';"
            " printf "
            "'\\033]7;file://__mc_sync__/" MC_OSC7_TOK "\\007'\r";

        return setup;
    }

    default:
        /* ksh, mksh, tcsh and the rest: no hook to chain onto and no PS1 the terminal can
         * drive, so they run without the protocol. */
        return NULL;
    }
}

mcterm_shell_rc_t *
mcterm_shell_rc_new (shell_type_t shell_type, const char *token)
{
    mcterm_shell_rc_t *rc;
    char *dir;
    gboolean ok;

    if (token == NULL)
        return NULL;

    if (shell_type == SHELL_FISH)
        return mcterm_rc_fish (token);

    if (shell_type == SHELL_BASH)
        dir = g_strdup (mc_config_get_data_path ());
    else if (shell_type == SHELL_ZSH)
        dir = g_build_filename (mc_config_get_data_path (), MC_ZDOTDIR_SUBDIR, (char *) NULL);
    else
        return NULL;

    if (g_mkdir_with_parents (dir, 0700) != 0)
    {
        g_free (dir);
        return NULL;
    }

    rc = g_new0 (mcterm_shell_rc_t, 1);
    rc->env = g_ptr_array_new_with_free_func (g_free);
    rc->args = g_ptr_array_new_with_free_func (g_free);

    ok = shell_type == SHELL_BASH ? mcterm_rc_bash (rc, dir) : mcterm_rc_zsh (rc, dir);
    if (ok)
        // What the startup files put in every mark they make the shell send.
        mcterm_rc_env (rc, "MC_TERM_TOKEN", token);

    g_free (dir);

    if (!ok)
    {
        mcterm_shell_rc_free (rc);
        return NULL;
    }

    return rc;
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_shell_rc_child_env (const mcterm_shell_rc_t *rc)
{
    guint i;

    if (rc == NULL)
        return;

    for (i = 0; i + 1 < rc->env->len; i += 2)
        g_setenv (g_ptr_array_index (rc->env, i), g_ptr_array_index (rc->env, i + 1), TRUE);
}

/* --------------------------------------------------------------------------------------------- */

guint
mcterm_shell_rc_args (const mcterm_shell_rc_t *rc, const char **argv, guint argv_size)
{
    guint i;

    if (rc == NULL)
        return 0;

    for (i = 0; i < rc->args->len && i < argv_size; i++)
        argv[i] = g_ptr_array_index (rc->args, i);

    return i;
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_shell_rc_free (mcterm_shell_rc_t *rc)
{
    if (rc == NULL)
        return;

    g_ptr_array_free (rc->env, TRUE);
    g_ptr_array_free (rc->args, TRUE);
    g_free (rc);
}

/* --------------------------------------------------------------------------------------------- */

#undef MC_MARK_TOK
#undef MC_OSC7_TOK

/* --------------------------------------------------------------------------------------------- */
