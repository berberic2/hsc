/*
 * This source code is part of hsc, a html-preprocessor,
 * Copyright (C) 2005 Matthias Bethke
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
/*
 * hsc/args_gnu.c
 *
 * user argument handling, glibc style
 *
 * created: 12-Apr-2005
 */

#include "hsc/global.h"
#include "hsc/status.h"
#include "hsc/callback.h"
#include "hsc/args.h"
#include "hsc/args_shared.h"
#include "ugly/fname.h"
#include "ugly/prginfo.h"
#include "ugly/returncd.h"
#include "ugly/uargs.h"
#include "hscprj/license.h"
#include <unistd.h>
#include <getopt.h>

static STRPTR arg_inpfname = NULL;  /* temp vars for set_args() */
static STRPTR arg_outfname = NULL;
static STRPTR arg_extension = NULL;
static STRPTR arg_server_dir = NULL;
static BOOL arg_mode = FALSE;
static BOOL arg_compact = FALSE;
static BOOL arg_getsize = FALSE;
static BOOL arg_rplc_ent = FALSE;
static BOOL arg_rplc_quote = FALSE;
static BOOL arg_smart_ent = FALSE;
static BOOL arg_strip_cmt = FALSE;
static BOOL arg_strip_badws = FALSE;
static BOOL arg_strip_ext = FALSE;
static BOOL arg_license = FALSE;
static BOOL arg_help = FALSE;
static BOOL arg_debug = FALSE;
static BOOL arg_nonesterr = FALSE;
static BOOL arg_lctags = FALSE;
static BOOL arg_xhtml = FALSE;
static BOOL arg_nvcss = FALSE;
static BOOL arg_checkext = FALSE;
static STRPTR arg_iconbase = NULL;
static STRPTR arg_striptags = NULL;
static LONG arg_entitymode = EMODE_INVALID;
static LONG arg_quotemode = QMODE_DOUBLE;

static ARGFILE *argf = NULL;

/*
 * cleanup_hsc_args: free local resources
 */
VOID cleanup_hsc_args(VOID)
{
    del_argfile(argf);
    del_estr(fileattr_str);
    if (msg_browser != NULL)
        del_msg_browser(arg_hp);
}

/*
 * user_defines_ok
 *
 * process all defines passed via user args
 *
 * result: always TRUE
 */
BOOL user_defines_ok(HSCPRC * hp)
{
    /* define destination attributes (HSC.DOCUMENT.URI etc.) */
    define_file_attribs(hp);

    if (define_list && dll_first(define_list)) {
        DLNODE *nd = dll_first(define_list);
        EXPSTR *defbuf = init_estr(64);

        while (nd) {
            STRPTR defarg = (STRPTR) dln_data(nd);

            D(fprintf(stderr, DHSC "define using `%s'\n", defarg));

            set_estr(defbuf, "<$define ");

            /* append attribute name */
            do {
                app_estrch(defbuf, defarg[0]);
                defarg++;
            } while (defarg[0] && (defarg[0] != '=')
                  && (defarg[0] != '/') && (defarg[0] != ':'));

            /* if no type set, use "string" as default */
            if (defarg[0] != ':')
                app_estr(defbuf, ":string");

            /* append type (if set) and attribute-flags */
            while (defarg[0] && (defarg[0] != '=')) {
                app_estrch(defbuf, defarg[0]);
                defarg++;
            }

            /* append value (if any) and quotes */
            if (defarg[0] == '=') {
                char quote_needed = 0;  /* flag: user did not use quotes */

                /* append "=" */
                app_estrch(defbuf, defarg[0]);
                defarg++;

                /* check which kind of quote should be appended */
                if ((defarg[0] != '\"') && (defarg[0] != '\'')) {
                    BOOL single_quote = FALSE;
                    BOOL double_quote = FALSE;
                    STRPTR scanarg = defarg;

                    /* scan value for quotes */
                    while (scanarg[0]) {
                        if (scanarg[0] == '\"')
                            double_quote = TRUE;
                        else if (scanarg[0] == '\'')
                            single_quote = TRUE;
                        scanarg++;
                    }

                    /* choose quote to enclose value */
                    if (!double_quote)
                        quote_needed = '\"';
                    else if (!single_quote)
                        quote_needed = '\'';
                    else
                        panic("both quotes in value");
                }

                /* append quote (if not already done by user) */
                if (quote_needed)
                    app_estrch(defbuf, quote_needed);

                /* append value */
                while (defarg[0]) {
                    app_estrch(defbuf, defarg[0]);
                    defarg++;
                }

                /* append quote (if not already done by user) */
                if (quote_needed)
                    app_estrch(defbuf, quote_needed);

            }

            /* append end ">" */
            app_estrch(defbuf, '>');

            D(fprintf(stderr, DHSC "define: `%s'\n", estr2str(defbuf)));

            hsc_include_string(hp, "DEFINE",
                             estr2str(defbuf), IH_PARSE_HSC | IH_NO_STATUS);
            nd = dln_next(nd);
        }

        del_estr(defbuf);
    } else {
        D(fprintf(stderr, DHSC "(no defines)\n"));
    }
    return ((BOOL) (return_code < RC_ERROR));
}

/*
 * args_ok
 *
 * prepare args, check & parse user args, display error and
 * help message if neccessary
 *
 * result: TRUE, if all args ok
 */
#ifdef AMIGA_STYLE_ARGS
BOOL args_ok(HSCPRC * hp, int argc, char *argv[])
{
    BOOL ok;                    /* return value */
    DLLIST *ignore_list = NULL; /* dummy */
    EXPSTR *destdir = init_estr(32);    /* destination dir */
    EXPSTR *rel_destdir = init_estr(32);    /* relative destination dir */
    EXPSTR *kack_name = init_estr(0);   /* temp. str for outfilename */
    struct arglist *hsc_args;   /* argument structure */
    LONG maximum_number_of_errors = strtol(DEFAULT_MAXERR, (char **) NULL, 10);
    LONG maximum_number_of_messages = strtol(DEFAULT_MAXMSG, (char **) NULL, 10);

    arg_hp = hp;
    arg_mode_CB(DEFAULT_MODE_STR);

    /* create arg-table */
    hsc_args =
        prepare_args("HSC_ARGS",

    /* file args */
                     "FROM/M", &incfile,
                     "include- and input file(s)",

                     "TO/K", &arg_outfname,
                     "output file (default: stdout)",

                     "PRJFILE/T/K", &prjfilename,
                     "project file (default: none)",

                     "PREFSFILE/T/K", &prefsfilename,
                     "syntax definition file",

                     "MSGFILE=MF/T/K", &msgfilename,
                     "message file (default: stderr)",

                     "MSGFORMAT/T/K", &msg_format,
                     "how to display messages",

                     "MSGBROWSER/T/K", &msg_browser,
                     "message browser to use (default:none)",
    /* numeric */
                     "MAXERR/N/K", &maximum_number_of_errors,
                     "max. number of errors (default: " DEFAULT_MAXERR ")",

                     "MAXMSG/N/K", &maximum_number_of_messages,
                     "max. number of messages (default: " DEFAULT_MAXMSG ")",

                     "EXTENSION/T/K", &arg_extension,
                     "output file extension (default: " DEFAULT_EXTENSION ")",

                     "DEFINE=DEF/T/K/M", &define_list,
                     "define global attribute",

                     "IGNORE=IGN/K/M/$", arg_ignore_CB, &ignore_list,
                     "ignore message number or class",

                     "ENABLE=ENA/K/M/$", arg_enable_CB, &ignore_list,
                     "enable message number or class",

                     "MSGMODE/E/K/$", arg_mode_CB, MODE_ENUMSTR, &arg_mode,
                     "mode for syntax check (" MODE_ENUMSTR ")",

                     "QUOTESTYLE=QS/E/K", QMODE_ENUMSTR, &arg_quotemode,
                     "defines how quotes appear (" QMODE_ENUMSTR ")",

                     "ENTITYSTYLE=ES/E/K", EMODE_ENUMSTR, &arg_entitymode,
                     "set character entity rendering (" EMODE_ENUMSTR ")",

                     "INCLUDEDIR=IDIR/K/M/$", arg_incdir_CB, &ignore_list,
                     "add include directory",

    /* switches */
                     "COMPACT=CO/S", &arg_compact,
                     "strip useless white spaces",

                     "GETSIZE/S", &arg_getsize,
                     "get width and height of images",

                     "RPLCENT=RE/S", &arg_rplc_ent,
                     "replace special characters",

                     "RPLCQUOTE=RQ/S", &arg_rplc_quote,
                     "replace quotes in text with `&quot;'",

                     "STRIPBADWS/S", &arg_strip_badws,
                     "strip bad whitespace",

                     "STRIPCOMMENT=SC/S", &arg_strip_cmt,
                     "strip SGML comments",

                     "STRIPEXTERNAL=SX/S", &arg_strip_ext,
                     "strip tags with external URIs",

                     "STRIPTAGS=ST/K", &arg_striptags,
                     "tags to be stripped",

                     "ICONBASE/T/K", &arg_iconbase,
                     "base URI for icon entities",

                     "SERVERDIR/T/K", &arg_server_dir,
                     "base directory for server relative URIs",

                     "STATUS/E/K/$", arg_status_CB,
                     STATUS_ENUM_STR, &disp_status,
                     "status message (" STATUS_ENUM_STR ")",

                     "NONESTERR=NNE/S", &arg_nonesterr,
                     "don't show \"previous call\" tracebacks on error",

                     "LOWERCASETAGS=LCT/S", &arg_lctags,
                     "force all tags and attributes to lowercase",
                     
                     "XHTML/S", &arg_xhtml,
                     "use XHTML mode (implies: LCT QS=double)",

                     "NOVALIDATECSS=NVCS/S", &arg_nvcss,
                     "don't validate CSS in STYLE attributes",

                     "CHECKEXTERNAL=CKX/S", &arg_checkext,
                     "check external HTTP links"
#ifdef AMIGA
                        "(requires bsdsocket.library)"
#endif
                        ,

                     "-DEBUG/S", &arg_debug,
                     "enable debugging output if enabled at compile-time",
    /* help */
                     "HELP=?=-h=--help/S", &arg_help, "display this text",
                     "LICENSE/S", &arg_license, "display license",

                     NULL);

    /* remove dummy list TODO: this sucks */
    del_dllist(ignore_list);

    ok = (hsc_args != NULL);

    /* set & test args */
    if (ok) {
        BOOL use_stdout = FALSE;    /* flag: use stdout as output-file */
        BOOL any_input_passed = FALSE;  /* flag: any input specified in args */
        STRPTR argfiles[] = {OPTION_FILE, NULL};

        argf = new_argfilev(argfiles);

        ok = set_args_file(argf, hsc_args) && set_args(argc, argv, hsc_args);

        /* display help, if requested vie HELP switch, or no
         * input to pipe or read is passed */
        any_input_passed = (incfile && dll_first(incfile));
        ok &= (!arg_help && any_input_passed);

        if (arg_license) {
            /* display license text */
            fprintf_prginfo(stderr);
            show_license();
            set_return_code(RC_WARN);
        } else if (!ok) {
            if (arg_help || !any_input_passed) {
                /* display help, if HELP-switch set */
                fprintf_prginfo(stderr);
                fprintf_arghelp(stderr, hsc_args);
            }
            set_return_code(RC_WARN);
        } else {
            BOOL fnsux = FALSE; /* flag: TRUE = can't evaluate out-filename */

            /* set debugging switch */
            hsc_set_debug(hp, arg_debug);

            /* autoset depending options */
            if (hsc_get_debug(hp))
                disp_status = STATUS_VERBOSE;

            /* set default options */
            if (!arg_extension)
                arg_extension = DEFAULT_EXTENSION;

            /* disable ID-warning if no project-file */
            if (!prjfilename)
                hsc_set_msg_ignore(hp, MSG_NO_DOCENTRY, TRUE);

            /* compute name of input file */
            arg_inpfname = NULL;
            if (dll_first(incfile)) {
                /* use last FROM as input file */
                arg_inpfname = dln_data(dll_last(incfile));

                set_estr(inpfilename, arg_inpfname);

                /* get path part of inputfilename as relative
                 * destination directory */
                get_fpath(rel_destdir, arg_inpfname);

                /* TODO: set reldir when including first file */
                /* TODO: find out why the above TODO is there */

                /* remove input filename from incfile */
                del_dlnode(incfile, dll_last(incfile));

                D(fprintf(stderr, DHSC "input : use `%s'\n"
                          DHSC "reldir: use `%s'\n",
                          estr2str(inpfilename), estr2str(rel_destdir)));
            }

            /* display include files */
            D(
                 {
                 DLNODE * nd = dll_first(incfile);

                 while (nd)
                 {
                 fprintf(stderr, DHSC "includ: use `%s'\n", (
                                                      STRPTR) dln_data(nd));
                 nd = dln_next(nd);
                 }
                 }
            );

            /*
             * if no output-filename given,
             * outfilename stays NULL. this let open_output
             * open stdout as output-file
             */
            if (arg_outfname) {
                /* check, if last char of outputfilename is a
                 * directory separator; if so, use the filename
                 * as destination directory
                 */
                if (arg_outfname) {
                    UBYTE lastch = 0;

#ifdef AMIGA
                    /* treat `TO ""' and `TO=""' the same for AmigaOS */
                    if (!strcmp(arg_outfname, "\"\"")) {
                        arg_outfname = "";
                        D(fprintf(stderr,
                         DHSC "AMIGA: use current dir, strange version\n"));
                    }
#endif

                    /* get last char of outfname to determine
                     * if it's a directory
                     */
                    if (strlen(arg_outfname))
                        lastch = arg_outfname[strlen(arg_outfname) - 1];

#ifdef AMIGA
                    /* for Amiga, accept empty string for current dir */
                    if (!lastch) {
                        lastch = (PATH_SEPARATOR[0]);
                        D(fprintf(stderr, DHSC "AMIGA: use current dir\n"));
                    }
#endif

                    if (strchr(PATH_SEPARATOR, lastch)) {
                        /* use outfilename as destdir */
                        set_estr(destdir, arg_outfname);
                        arg_outfname = NULL;
                        D(fprintf(stderr, DHSC "output: use `%s' as destdir\n",
                                  estr2str(destdir)));
                    } else if (arg_inpfname) {
                        /* output-filename already specified */
                        /* separate it to destdir + reldir + name */
                        EXPSTR *kack_destdir = init_estr(0);
                        EXPSTR *kack_reldir = init_estr(0);
                        STRPTR inp_reldir = estr2str(rel_destdir);
                        STRPTR out_reldir = NULL;
                        STRPTR ou2_reldir = NULL;

                        get_fname(kack_name, arg_outfname);
                        get_fpath(kack_destdir, arg_outfname);

                        /* check corresponding dirs for
                         * consistency: check if last strlen(rel_destdir)
                         * chars are equal */
                        out_reldir = estr2str(kack_destdir);
                        ou2_reldir = out_reldir;
                        out_reldir = out_reldir
                            + (strlen(out_reldir) - strlen(inp_reldir));

                        if (out_reldir[0]) {
                            /* search for next dir-sparator backwards */
                            /* (this ones only needed for a smart error message) */
                            while ((out_reldir != ou2_reldir)
                                 && (!strchr(PATH_SEPARATOR, out_reldir[0]))) {
                                out_reldir--;
                            }

                            if (out_reldir != ou2_reldir)
                                out_reldir++;
                        }
                        D(fprintf(stderr, DHSC "corr_inp: `%s'\n"
                                  DHSC "corr_out: `%s'\n",
                                  inp_reldir, out_reldir));

                        /* check if correspondig relative in/out-dirs
                         * are equal */
                        if (!fnamecmp(inp_reldir, out_reldir)) {
                            /* they match.. */
                            STRPTR tmp_name = NULL;     /* copy of kack_nam */

                            /* cut corresponding chars */
                            get_left_estr(kack_destdir, kack_destdir,
                                          estrlen(kack_destdir)
                                          - strlen(out_reldir));

                            set_estr(kack_reldir, inp_reldir);

                            D(fprintf(stderr, DHSC "kack_dst: `%s'\n"
                                      DHSC "kack_rel: `%s'\n"
                                      DHSC "kack_nam: `%s'\n",
                                      estr2str(kack_destdir),
                                      estr2str(kack_reldir),
                                      estr2str(kack_name))
                                );

                            /* just copy these values where they are
                             * expected to be */
                            estrcpy(destdir, kack_destdir);
                            estrcpy(rel_destdir, kack_reldir);

                            /* create output filename */
                            tmp_name = strclone(estr2str(kack_name));
                            estrcpy(kack_name, kack_destdir);
                            estrcat(kack_name, kack_reldir);
                            app_estr(kack_name, tmp_name);
                            ufreestr(tmp_name);

                            arg_outfname = estr2str(kack_name);
                        } else {
                            /* unmatched corresponding dirs */
                            fprintf(stderr, "unmatched corresponding relative "
                                    "directories:\n  input  `%s'\n  output `%s'\n",
                                    inp_reldir, out_reldir);
                            ok = FALSE;
                        }

                        /* free temp. vars */
                        del_estr(kack_reldir);
                        del_estr(kack_destdir);
                    }
                }
                if (arg_outfname) {
                    /* set outputfilename with value passed iwithin args */
                    outfilename = init_estr(32);
                    set_estr(outfilename, arg_outfname);
                    D(fprintf(stderr, DHSC "output: set to `%s'\n",
                              estr2str(outfilename)));
                } else {
                    /* no outfilename given */
                    /* ->outfilename = destdir + inpfilename + ".html" */

                    /* link destdir & input filename */
                    outfilename = init_estr(32);
                    link_fname(outfilename, estr2str(destdir),
                               arg_inpfname);
                    if (strcmp(arg_extension, "."))
                        set_fext(outfilename, arg_extension);
                    D(fprintf(stderr,
                              DHSC "output: concat destdir+inpfile+`.%s'\n"
                              DHSC "output: set to `%s'\n",
                              arg_extension, estr2str(outfilename)));
                }

                if (fnsux) {
                    /* no way to find out output filename */
                    status_error("unable to evaluate output filename\n");
                    arg_outfname = NULL;
                    ok = FALSE;
                }
            } else {
                D(fprintf(stderr, DHSC "output: use stdout\n"));
                use_stdout = TRUE;
            }

            if (!ok)
                set_return_code(RC_ERROR);
        }

        if (ok) {
            /* set server dir */
            if (arg_server_dir)
                hsc_set_server_dir(hp, arg_server_dir);

            /* set icon base */
            if (arg_iconbase)
                hsc_set_iconbase(hp, arg_iconbase);

            /* check, if stdout should be used as output */
            if (!use_stdout)
                hsc_set_filename_document(hp, estr2str(outfilename));
        }

        /* display argument error message */
        if (!ok) {
            /* NOTE: no strclone() is used on outfilename, if an
             * error already occured within set_args(). therefore,
             * you must not call ufreestr( outfilename ) */
            pargerr();
            arg_outfname = NULL;
            set_return_code(RC_ERROR);
        } else {
            EXPSTR *tmp_fname = init_estr(32);  /* filename only part */

            fileattr_str = init_estr(64);

            /* set HSC.DOCUMENT */
            if (outfilename)
                get_fname(tmp_fname, estr2str(outfilename));
            set_dest_attribs(hp, estr2str(destdir),
                                 estr2str(rel_destdir),
                                 estr2str(tmp_fname));

            /* set HSC.SOURCE */
            if (inpfilename)
                get_fname(tmp_fname, estr2str(inpfilename));
            else
                clr_estr(tmp_fname);

            set_source_attribs(hp, estr2str(rel_destdir),
                               estr2str(tmp_fname));
            D(
                 {
                 HSCMSG_ID i;

                 fprintf(stderr, "\n"
                         DHSC "input : `%s'\n", estr2str(inpfilename));
                 fprintf(stderr, DHSC "output: `%s'\n", get_outfilename());
                 fprintf(stderr, DHSC "destdr: `%s'\n", estr2str(destdir));
                 fprintf(stderr, DHSC "reldst: `%s'\n", estr2str(rel_destdir));
                 if (prjfilename)
                 fprintf(stderr, DHSC "projct: `%s'\n", prjfilename);
                 if (!use_stdout)
                 fprintf(stderr, DHSC "procss: `%s'\n", estr2str(outfilename));

                /* show classes to be ignored */
                 fprintf(stderr, DHSC "ignore class:");
                 if (hsc_get_msg_ignore_notes(hp))
                 {
                 fprintf(stderr, " notes");
                 }
                 fprintf(stderr, "\n");
                /* show messages to be ignored */
                 fprintf(stderr, DHSC "ignore:");
                 for (i = 0; i < MAX_MSGID; i++)
                 {
                 if (hsc_get_msg_ignore(hp, i) == ignore)
                 {
                 fprintf(stderr, " %lu", i);
                 }
                 }
                 fprintf(stderr, "\n");
                /* show messages to be enabled */
                 fprintf(stderr, DHSC "enable:");
                 for (i = 0; i < MAX_MSGID; i++)
                 {
                 if (hsc_get_msg_ignore(hp, i) == enable)
                 {
                 fprintf(stderr, " %lu", i);
                 }
                 }
                 fprintf(stderr, "\n");
                 }
            );

            del_estr(tmp_fname);
        }

        /*
         * set flags of hsc-process
         */
        if (ok) {
            hsc_set_chkid(hp, TRUE);
            hsc_set_chkuri(hp, TRUE);
            hsc_set_compact(hp, arg_compact);
            hsc_set_debug(hp, arg_debug);
            hsc_set_getsize(hp, arg_getsize);
            hsc_set_rplc_ent(hp, arg_rplc_ent);
            hsc_set_rplc_quote(hp, arg_rplc_quote);
            hsc_set_smart_ent(hp, arg_smart_ent);
            hsc_set_strip_badws(hp, arg_strip_badws);
            hsc_set_strip_cmt(hp, arg_strip_cmt);
            hsc_set_strip_ext(hp, arg_strip_ext);
            hsc_set_no_nested_errors(hp, arg_nonesterr);
            hsc_set_strip_tags(hp, arg_striptags);
            hsc_set_lctags(hp, arg_lctags);
            hsc_set_checkext(hp, arg_checkext);
            if(arg_xhtml && arg_nvcss)
               fprintf(stderr, "Warning: cannot disable CSS checking in XHTML mode!\n");
            else
               hsc_set_novcss(hp, arg_nvcss);
            if(arg_xhtml && (QMODE_DOUBLE != arg_quotemode))
               fprintf(stderr, "Warning: XHTML only allows double quotes, ignoring QUOTESTYLE option!\n");
            else
               hsc_set_quote_mode(hp, arg_quotemode);
            hsc_set_xhtml(hp, arg_xhtml);
            hsc_set_entity_mode(hp, arg_entitymode);

            /* set message limits; 0 means use the value set by
             * init_hscprc(), which means infinite */
            if (maximum_number_of_messages)
                hsc_set_maximum_messages(hp, maximum_number_of_messages);
            if (maximum_number_of_errors)
                hsc_set_maximum_errors(hp, maximum_number_of_errors);

            /* set directories */
            hsc_set_destdir(hp, estr2str(destdir));
            hsc_set_reldir(hp, estr2str(rel_destdir));

            if (msg_browser != NULL) {
                STRPTR compilation_unit = estr2str(inpfilename);

                if (compilation_unit == NULL)
                    compilation_unit = "<stdin>";

                init_msg_browser(hp, compilation_unit);
            }

            /* set global attributes according to opts (only XHTML so far) */
            set_global_attribs(hp);
        }
        /* release mem used by args */
        free_args(hsc_args);
    } else {
        D(
        /* only for developer */
             fprintf(stderr, "ArgDef error: %lu\n", prep_error_num);
            );
    }

    del_estr(destdir);
    del_estr(rel_destdir);
    del_estr(kack_name);

    return (ok);
}
#else

#define ARG_NONE (0)
#define ARG_OBL (1)
#define ARG_OPT (2)
#define OPT_NONE (0)
#define OPT_TXT (1)
#define OPT_DEC (2)
#define OPT_FLT (3)
#define OPT_FNC (4)

struct hscoption {
   const char *lopt;
   const char *help;
   const char *arghelp;
   const BOOL (*argfunc)(char*, const char**);
   const char has_arg;
   const char arg_type; 
   const char sopt;
};


void print_help(const struct hscoption *o, int nopts)
{
   int i, j, helpx=0;
   for(i=0; i<nopts; ++i) {
      int thishelpx = 3;
      if(NULL != o[i].lopt)
         thishelpx += 2 + strlen(o[i].lopt);
      if(NULL != o[i].arghelp)
         thishelpx += 1 + strlen(o[i].arghelp) + 1;
      if(thishelpx > helpx)
         helpx = thishelpx;
   }
   for(i=0; i<nopts; ++i) {
      int xpos = 3;
      if('\0' != o[i].sopt)
         fprintf(stderr,"-%c ",o[i].sopt);
      else
         fputs("   ",stderr);
      if(NULL != o[i].lopt) {
         fprintf(stderr,"--%s",o[i].lopt);
         xpos += 2 + strlen(o[i].lopt);
         if(NULL != o[i].arghelp) {
            fprintf(stderr,"=%s",o[i].arghelp);
            xpos += 1 + strlen(o[i].arghelp);
         }
      }
      for(j=0; j<helpx-xpos; ++j)
         fputc(' ',stderr);
      fprintf(stderr," %s\n",o[i].help);
   }
}

static struct option *hscopts_to_getopt_long(const struct hscoption *o, int nopts)
{
   struct option *lop, *tlop;
   int i, n = 0;

   for(i=0; i<nopts; ++i)
      if(o[i].lopt)
         ++n;
   lop = tlop = umalloc((1+n) * sizeof(*lop));
   for(i=0; i<nopts; ++i) {
      if(o[i].lopt) {
         tlop->name = o[i].lopt;
         tlop->has_arg = o[i].has_arg;
         tlop->flag = NULL;
         tlop->val = o[i].sopt;
         ++tlop;
      }
   }
   tlop->name = NULL;
   tlop->flag = NULL;
   tlop->has_arg = tlop->val = 0;
   return lop;
}

static char *hscopts_to_getopt_short(const struct hscoption *o, int nopts)
{
   char *sop, *tsop;
   int i, n = 0;

   for(i=0; i<nopts; ++i)
      if('\0' != o[i].sopt)
         ++n;
   sop = tsop = umalloc((3*n+1) * sizeof(*sop));
   for(i=0; i<nopts; ++i) {
      if('\0' != o[i].sopt) {
         *tsop++ = o[i].sopt;
         switch(o[i].has_arg) {
            case ARG_OPT :
               *tsop++ = ':';
               /* fall through */
            case ARG_OBL :
               *tsop++ = ':';
            default : 
#if DEBUG
               fprintf(stderr,"WARNING: unknown value for has_arg in hscopts_to_getopt_short()\n");
#endif
               break;
         }
      }
   } 
   *tsop = '\0';
   return sop;
}

static const struct hscoption *find_long_option(HSCPRC *hp, const struct hscoption *o, int n, const char *s)
{
   const struct hscoption *co = NULL;
   int i;

   for(i=0; i<n; ++i)
      if(0 == strcmp(o[i].lopt,s))
         co = o+i;
   return co;
}

static const struct hscoption *find_short_option(HSCPRC *hp, const struct hscoption *o, int n, char c)
{
   const struct hscoption *co = NULL;
   int i;
   for(i=0; i<n; ++i)
      if(o[i].sopt == c)
         co = o+i;
   return co;
}

static BOOL verify_option(HSCPRC *hp, const struct hscoption *o, char *val)
{
   int i;
   char *end;
   const char *desc;
   BOOL ok=TRUE;

   if(!o) return FALSE;

   switch(o->arg_type) {
      case OPT_NONE:
      case OPT_TXT :
         /* nothing to verify */
         break;
      case OPT_DEC :
         desc = "decimal";
         if('\0' == *val) {
            ok = FALSE;
            break;
         }
         i = strtol(val,&end,10);
         if('\0' != *end)
            ok = FALSE;
         break;
      case OPT_FNC :
         ok = o->argfunc(val,&desc);
      case OPT_FLT :
         /* unused so far, so fall through */
      default :
         panic("Unknown option type, check hscoptions!\n");
   }
   if(!ok) {
      const char *s = o->lopt;
      char t[2]={0};
      if(!s) {
         t[0] = o->sopt;
         s = t;
      }
      fprintf(stderr,"Error: option `%s' requires a %s argument!\n", s, desc);
   }
   return ok;
}

void process_short_option(HSCPRC *hp, char c)
{
   switch(c) {
      case 'o' :
         break;
      case 'p' :
         break;
      case 's' :
         break;
      case 'D' :
         break;
      case 'i' :
         break;
      case 'e' :
         break;
      case 'm' :
         break;
      case 'I' :
         break;
      case 'c' :
         break;
      case 'g' :
         break;
      case 'q' :
         break;
      case 'l' :
         break;
      case 'x' :
         break;
      case 'd' :
         break;
      case 'h' :
         break;

      default :
         panic("Unknwon short option `%c'");
         break;
   }
}

void process_long_option(HSCPRC *hp, const char *s)
{
   static const struct BOOLOPT { const char *s; void (*v)(HSCPRC*,BOOL); } boolopts[] = {
      {"rplcent",&hsc_set_rplc_ent},
      {"rplcquote",&hsc_set_rplc_quote},
      {"stripbadws",&hsc_set_strip_badws},
      {"stripcomment",&hsc_set_strip_cmt},
      {"stripext",&hsc_set_strip_ext},
      {"nonesterr",&hsc_set_no_nested_errors},
      {"nocss",&hsc_set_novcss},
      {"ckxuri",&hsc_set_checkext}
   };
   /*
      "msgfile"
      "msgfmt"
      "browser"
      "maxerr"
      "maxmsg"
      "ext"
      "qstyle"
      "estyle"
      "striptags"
      "iconbase"
      "serverdir"
      "status"
      "license"
      */
}

BOOL args_ok(HSCPRC * hp, int argc, char *argv[])
{
   static const struct hscoption opts[] = {
      {"to",          "Output file",                                               "FILE",  NULL,ARG_OBL, OPT_TXT, 'o'},
      {"prj",         "Project file",                                              "FILE",  NULL,ARG_OBL, OPT_TXT, 'p'},
      {"syntax",      "Syntax definition file (default: hsc.prefs)",               "FILE",  NULL,ARG_OBL, OPT_TXT, 's'},
      {"msgfile",     "Message file for output (default: stderr)",                 "FILE",  NULL,ARG_OBL, OPT_TXT, 0},
      {"msgfmt",      "printf-style format for messages",                          "FORMAT",NULL,ARG_OBL, OPT_TXT, 0},
      {"browser",     "Message browser program to use",                            "PROG",  NULL,ARG_OBL, OPT_TXT, 0},
      {"maxerr",      "Maximum numer of errors (default: " DEFAULT_MAXERR ")",     "NUM",   NULL,ARG_OBL, OPT_DEC, 0},
      {"maxmsg",      "Maximum numer of messages (default: " DEFAULT_MAXMSG ")",   "NUM",   NULL,ARG_OBL, OPT_DEC, 0},
      {"ext",         "Output file extension (default: " DEFAULT_EXTENSION ")",    "WORD",  NULL,ARG_OBL, OPT_TXT, 0},
      {"define",      "Define global attribute",                                   "WORD",  NULL,ARG_OBL, OPT_TXT, 'D'},
      {"ignore",      "Ignore message numer or class",                             "WORD",  NULL,ARG_OBL, OPT_TXT, 'i'},
      {"enable",      "Enable message numer or class",                             "WORD",  NULL,ARG_OBL, OPT_TXT, 'e'},
      {"msgmode",     "Syntax checking mode (" MODE_ENUMSTR ")",                   "WORD",  NULL,ARG_OBL, OPT_TXT, 'm'},
      {"qstyle",      "Quote style  (" QMODE_ENUMSTR ")",                          "WORD",  NULL,ARG_OBL, OPT_TXT, 0},
      {"estyle",      "Entity style  (" EMODE_ENUMSTR ")",                         "WORD",  NULL,ARG_OBL, OPT_TXT, 0},
      {"incdir",      "Add include directory",                                     "DIR",   NULL,ARG_OBL, OPT_TXT, 'I'},
      {"compact",     "Strip superfluous whitespace",                              NULL,    NULL,ARG_NONE,OPT_NONE,'c'},
      {"getsize",     "Set width and height attributes for images",                NULL,    NULL,ARG_NONE,OPT_NONE,'g'},
      {"rplcent",     "Replace non-ASCII characters with entities (cf. --estyle)", NULL,    NULL,ARG_NONE,OPT_NONE,0},
      {"rplcquote",   "Replace double quotes with `&quot;'",                       NULL,    NULL,ARG_NONE,OPT_NONE,0},
      {"stripbadws",  "Strip bad whitespace",                                      NULL,    NULL,ARG_NONE,OPT_NONE,0},
      {"stripcomment","Strip SGML comments",                                       NULL,    NULL,ARG_NONE,OPT_NONE,0},
      {"stripext",    "Strip tags with external URIs",                             NULL,    NULL,ARG_NONE,OPT_NONE,0},
      {"striptags",   "Tags to be stripped",                                       "LIST",  NULL,ARG_OBL, ARG_TEXT, 0},
      {"iconbase",    "Base URI for icon entities",                                "URI",   NULL,ARG_OBL, ARG_TEXT, 0},
      {"serverdir",   "Base directory for server relative URIs",                   "DIR",   NULL,ARG_OBL, ARG_TEXT, 0},
      {"status",      "Status message verbosity (" STATUS_ENUM_STR ")",            "LIST",  NULL,ARG_OBL, ARG_TEXT, 0},
      {"quiet",       "Be quiet. Equivalent to --status=QUIET",                    NULL,    NULL,ARG_NONE,OPT_NONE,'q'}, 
      {"nonesterr",   "Don't show \"previous call\" tracebacks on error",          NULL,    NULL,ARG_NONE,OPT_NONE,0},
      {"lowercase",   "Force all tags and attributes to lowercase",                NULL,    NULL,ARG_NONE,OPT_NONE,'l'},
      {"xhtml",       "Use XHTML mode (implies -l --qstyle=DOUBLE)",               NULL,    NULL,ARG_NONE,OPT_NONE,'x'},
      {"nocss",       "Don't validate CSS in STYLE attributes",                    NULL,    NULL,ARG_NONE,OPT_NONE,0},
      {"ckxuri",      "Check external links",                                      NULL,    NULL,ARG_NONE,OPT_NONE,0},
      {"debug",       "Enable debugging output if enabled at compile time",        NULL,    NULL,ARG_NONE,OPT_NONE,'d'},
      {"license",     "Display the license",                                       NULL,    NULL,ARG_NONE,OPT_NONE,0},
      {"help",        "Show this help",                                            NULL,    NULL,ARG_NONE,OPT_NONE,'h'},
   };                                              
   int c;
   int nopts=sizeof(opts)/sizeof(opts[0]);
   struct option *lop = hscopts_to_getopt_long(opts,nopts);
   char *sop = hscopts_to_getopt_short(opts,nopts);

   while(1) {
      int opt_index;

      c = getopt_long(argc, argv, sop, lop, &opt_index);
      if(-1 == c)
         break;

      if(c) {
         if(!verify_option(hp, find_short_option(hp,opts,nopts,c), optarg))
            return FALSE;
         process_short_option(hp, c);
      } else {
         if(!verify_option(hp, find_long_option(hp,opts,nopts,lop[opt_index].name), optarg))
            return FALSE;
         process_long_option(hp, lop[opt_index].name);
         break;
      }
   }
   print_help(opts,nopts);

   ufree(lop);
   ufree(sop);
   return FALSE;
}
#endif

