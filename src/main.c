/*
  Copyright Simon Mitternacht 2013-2016.

  This file is part of FreeSASA.

  FreeSASA is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  FreeSASA is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with FreeSASA.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <stdarg.h>
#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include "freesasa.h"
#include "util.h"

#if STDC_HEADERS
extern int getopt(int, char * const *, const char *);
extern int optind;
extern char *optarg;
#endif

#ifdef PACKAGE_VERSION
const char *version = PACKAGE_VERSION;
#else
const char* version = "unknown";
#endif

char *program_name;
const char* options_string;

freesasa_parameters parameters;
freesasa_classifier *classifier = NULL;
FILE *output_pdb = NULL;
FILE *per_residue_type_file = NULL;
FILE *per_residue_file = NULL;
FILE *output = NULL;
int per_residue_type = 0;
int per_residue = 0;
int printlog = 1;
int structure_options = 0;
int printpdb = 0;
int n_chain_groups = 0;
const char** chain_groups;
int n_select = 0;
const char** select_cmd;

void
help(void)
{
    fprintf(stderr,"\nUsage: %s [options] pdb-file(s)\n\n",
            program_name);
    fprintf(stderr,"GENERAL OPTIONS\n"
            "  -h (--help)           Print this message\n"
            "  -v (--version)        Print version of the program\n");
    fprintf(stderr,"\nPARAMETERS\n"
            "  -S (--shrake-rupley)  Use Shrake & Rupley algorithm\n"
            "  -L (--lee-richards)   Use Lee & Richards algorithm [default]\n");
    fprintf(stderr,
            "\n"
            "  -p <value>  (--probe-radius=<value>)\n"
            "                        Probe radius [default: %4.2f Å]\n"
            "\n"
            "  -n <value>  (--resolution=<value>)\n"
            "                        Either: \n"
            "                        - Number of test points in Shrake & Rupley algorithm,\n"
            "                          [default: %d] or\n"
            "                        - number of slices per atom in Lee & Richards algorithm.\n"
            "                          [default: %d]\n"
            "                        depending on which is selected.\n",
            FREESASA_DEF_PROBE_RADIUS,FREESASA_DEF_SR_N,FREESASA_DEF_LR_N);
#ifdef USE_THREADS
    fprintf(stderr,
            "\n  -t <value>  (--n-threads=<value>)\n"
            "                        Number of threads to use in calculation. [default %d]\n",
            FREESASA_DEF_NUMBER_THREADS);
#endif
    fprintf(stderr,
            "\n  -c <file> (--config-file=<file>)\n"
            "                        Use atomic radii and classes provided in file, example\n"
            "                        configuration files can be found in the directory\n"
            "                        share/.\n");
    fprintf(stderr,
            "\nINPUT\n"
            "  -H (--hetatm)         Include HETATM entries from input.\n"
            "  -Y (--hydrogen)       Include hydrogen atoms (skipped by default). Default\n"
            "                        classifier emits warnings. Use with care. To get\n"
            "                        sensible results, one probably needs to redefine atomic\n"
            "                        radii with the -c option. Default H radius is 1.10 Å.\n"
            "  -m (--join-models)    Join all MODELs in input into one big structure.\n"
            "  -C (--separate-chains) Calculate SASA for each chain separately.\n"
            "  -M (--separate-models) Calculate SASA for each MODEL separately.\n"
            "\n"
            "  -g <chains> (--chain-groups=<chains>)\n"
            "                        Select chain or group of chains to treat separately.\n"
            "                        Several groups can be concatenated by '+', or by\n"
            "                        repetition.\n\n"
            "                        Examples:\n"
            "                            '-g A', '-g AB', -g 'A+B', '-g A -g B', '-g AB+CD'\n"
            "\n"
            "  --unknown=<guess|skip|halt>\n"
            "                        When an unknown atom is encountered FreeSASA can either\n"
            "                        'guess' its VdW radius, 'skip' the atom, or 'halt'.\n"
            "                        Default is 'guess'.\n");
    fprintf(stderr,"\nOUTPUT\n"
            "  -l (--no-log)         Don't print log message (useful with -r -R and -B)\n"
            "  -w (--no-warnings)    Don't print warnings (will still print warnings due to\n"
            "                        invalid command line options)\n"
            "\n"
            "  -o <file> (--output=<file>)\n"
            "  -e <file> (--error-file=<file>)\n"
            "                        Redirect output and/or errors and warnings to file.\n"
            "\n"
            "  -r  (--foreach-residue-type)  --residue-type-file=<output-file>\n"
            "  -R  (--foreach-residue)       --residue-file=<output-file>\n"
            "                        Print SASA for each residue, either grouped by type or\n"
            "                        sequentially. Use the -file variant to specify an output\n"
            "                        file.\n"
            "\n"
            "  -B  (--print-as-B-values)     --B-value-file=<output-file>\n"
            "                        Print PDB file where the temperature factor of each atom\n"
            "                        has been replaced by its SASA, and the occupancy number\n"
            "                        by the atomic radius. Use the -file variant to specify\n"
            "                        an output file.\n"
            "                        This option might give confusing output when used in\n"
            "                        conjuction with the options -C and -g.\n"
            "\n"
            "  --select <command>    Select atoms using Pymol select syntax.\n"
            "                        The option can be repeated to define several selections.\n\n"
            "                        Examples:\n"
            "                            'AR, resn ala+arg', 'chain_A, chain A'\n"
            "                        AR and chain_A are just the names of the selections,\n"
            "                        which will be reused in output. See documentation for\n"
            "                        full syntax specification.\n");
    fprintf(stderr,
            "\nIf no pdb-file is specified STDIN is used for input.\n\n"
            "To calculate SASA of one or several PDB file using default parameters simply\ntype:\n\n"
            "   '%s pdb-file(s)'     or    '%s < pdb-file'\n\n",
            program_name,program_name);
}

void
short_help(void)
{
    fprintf(stderr,"Run '%s -h' for usage instructions.\n",
            program_name);
}

void
abort_msg(const char *format,
          ...)
{
    va_list arg;
    va_start(arg, format);
    fprintf(stderr, "%s: error: ", program_name);
    vfprintf(stderr, format, arg);
    va_end(arg);
    fputc('\n', stderr);
    short_help();
    fputc('\n', stderr);
    fflush(stderr);
    exit(EXIT_FAILURE);
}


void
run_analysis(FILE *input,
             const char *name)
{
    int several_structures = 0, name_len = strlen(name);
    freesasa_result *result;
    freesasa_strvp *classes = NULL;
    freesasa_structure *single_structure[1];
    freesasa_structure **structures;
    int n = 0;
    
    if ((structure_options & FREESASA_SEPARATE_CHAINS) ||
        (structure_options & FREESASA_SEPARATE_MODELS)) {
        structures = freesasa_structure_array(input,&n,classifier,structure_options);
        several_structures = 1;
        for (int i = 0; i < n; ++i) {
            if (structures[i] == NULL) abort_msg("Invalid input.\n");
        }
    } else {
        single_structure[0] = freesasa_structure_from_pdb(input,classifier,structure_options);
        structures = single_structure;
        n = 1;
    }
    if (structures == NULL) abort_msg("Invalid input. Aborting.\n");

    if (n_chain_groups > 0) {
        int n2 = n;
        if (several_structures == 0) {
            structures = malloc(sizeof(freesasa_structure*));
            structures[0] = single_structure[0];
            several_structures = 1;
        }
        for (int i = 0; i < n_chain_groups; ++i) {
            for (int j = 0; j < n; ++j) {
                freesasa_structure* tmp = freesasa_structure_get_chains(structures[j],chain_groups[i]);
                if (tmp != NULL) {
                    ++n2;
                    structures = realloc(structures,sizeof(freesasa_structure*)*n2);
                    structures[n2-1] = tmp;
                } else {
                    abort_msg("Chain(s) '%s' not found.\n",chain_groups[i]);
                }
            }
        }
        n = n2;
    }

    for (int i = 0; i < n; ++i) {
        char name_i[name_len+10];
        result = freesasa_calc_structure(structures[i],&parameters);
        if (result == NULL)        abort_msg("Can't calculate SASA.\n");
        classes = freesasa_result_classify(result,structures[i],classifier);
        if (classes == NULL)       abort_msg("Can't determine atom classes. Aborting.\n");
        strcpy(name_i,name);
        if (several_structures) {
            if (structure_options & FREESASA_SEPARATE_MODELS) 
                sprintf(name_i+strlen(name_i),":%d",freesasa_structure_model(structures[i]));
            sprintf(name_i+strlen(name_i),":%s",freesasa_structure_chain_labels(structures[i]));
        }
        if (printlog) {
            freesasa_log(output,result,name_i,&parameters,classes);
            if (strlen(freesasa_structure_chain_labels(structures[i])) > 1)
                freesasa_per_chain(output,result,structures[i]);
            if (n > 1) fprintf(output,"\n#######################\n");
        }
        if (per_residue_type) {
            if (several_structures) fprintf(per_residue_type_file,"\n## %s\n",name_i);
            freesasa_per_residue_type(per_residue_type_file,result,structures[i]);
        }
        if (per_residue) {
            if (several_structures) fprintf(per_residue_file,"\n## %s\n",name_i);
            freesasa_per_residue(per_residue_file,result,structures[i]);
        }
        if (printpdb) {
            freesasa_write_pdb(output_pdb,result,structures[i]);
        }
        if (n_select > 0) {
            fprintf(output,"\nSELECTIONS\n");
            for (int c = 0; c < n_select; ++c) {
                double a;
                char name[FREESASA_MAX_SELECTION_NAME+1];
                if (freesasa_select_area(select_cmd[c],name,&a,structures[i],result)
                    == FREESASA_SUCCESS) {
                    fprintf(output,"%s : %10.2f\n",name,a);
                } else {
                }
            }
        }
        freesasa_result_free(result);
        freesasa_strvp_free(classes);
        freesasa_structure_free(structures[i]);
    }
    if (structures != single_structure) free(structures);
}

FILE*
fopen_werr(const char* filename,
           const char* mode) 
{
    errno = 0;
    FILE *f = fopen(filename,mode);
    if (f == NULL) {
        fprintf(stderr,"%s: error: could not open file '%s'; %s\n",
                program_name,optarg,strerror(errno));
        short_help();
        exit(EXIT_FAILURE);
    }
    return f;
}

void
add_chain_groups(const char* cmd) 
{
    char *str = strdup(cmd);
    const char *token = strtok(str,"+");
    while (token) {
        ++n_chain_groups;
        chain_groups = realloc(chain_groups,sizeof(char*)*n_chain_groups);
        if (chain_groups == NULL) {mem_fail(); abort();}
        chain_groups[n_chain_groups-1] = strdup(token);
        token = strtok(0,"+");
    }
    free(str);
}

void
add_select(const char* cmd) 
{
    ++n_select;
    select_cmd = realloc(select_cmd,sizeof(char*)*n_select);
    if (select_cmd == NULL) { mem_fail(); abort(); }
    select_cmd[n_select-1] = cmd;
}

void
add_unknown_option(const char *optarg)
{
    if (strcmp(optarg,"skip") == 0) {
        structure_options |= FREESASA_SKIP_UNKNOWN;
        return;
    } 
    if(strcmp(optarg,"halt") == 0) {
        structure_options |= FREESASA_HALT_AT_UNKNOWN;
        return;
    } 
    if(strcmp(optarg,"guess") == 0) {
        return; //default
    }
    abort_msg("Unknown alternative to option --unknown: '%s'", optarg);
}

int
main(int argc,
     char **argv) 
{
    int alg_set = 0;
    FILE *input = NULL, *errlog = NULL;
    char opt;
    int n_opt = 'z'+1;
    char opt_set[n_opt];
    int option_index = 0;
    int option_flag;
    enum {B_FILE,RES_FILE,SEQ_FILE,SELECT,UNKNOWN};
    parameters = freesasa_default_parameters;
    memset(opt_set,0,n_opt);
    // errors from this file will be prepended with freesasa, library errors with FreeSASA
    program_name = "freesasa";
    struct option long_options[] = {
        {"lee-richards", no_argument, 0, 'L'},
        {"shrake-rupley", no_argument, 0, 'S'},
        {"probe-radius", required_argument, 0, 'p'},
        {"resolution", required_argument, 0, 'n'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {"no-log", no_argument, 0, 'l'},
        {"no-warnings", no_argument, 0, 'w'},
        {"n-threads",required_argument, 0, 't'},
        {"hetatm",no_argument, 0, 'H'},
        {"hydrogen",no_argument, 0, 'Y'},
        {"separate-chains",no_argument, 0, 'C'},
        {"separate-models",no_argument, 0, 'M'},
        {"join-models",no_argument, 0, 'm'},
        {"config-file",required_argument,0,'c'},
        {"foreach-residue-type",no_argument,0,'r'},
        {"foreach-residue",no_argument,0,'R'},
        {"print-as-B-values",no_argument,0,'B'},
        {"chain-groups",required_argument,0,'g'},
        {"error-file",required_argument,0,'e'},
        {"output",required_argument,0,'o'},
        {"residue-type-file",required_argument,&option_flag,RES_FILE},
        {"residue-file",required_argument,&option_flag,SEQ_FILE},
        {"B-value-file",required_argument,&option_flag,B_FILE},
        {"select",required_argument,&option_flag,SELECT},
        {"unknown",required_argument,&option_flag,UNKNOWN}
    };
    options_string = ":hvlwLSHYCMmBrRc:n:t:p:g:e:o:";
    while ((opt = getopt_long(argc, argv, options_string,
                              long_options, &option_index)) != -1) {
        opt_set[(int)opt] = 1;
        errno = 0;
        // Assume arguments starting with dash are actually missing arguments
        if (optarg != NULL && optarg[0] == '-') {
            if (option_index > 0) abort_msg("Missing argument? Value '%s' cannot be argument to '--%s'.\n",
                                            program_name,optarg,long_options[option_index].name);
            else abort_msg("Missing argument? Value '%s' cannot be argument to '-%c'.\n",
                           optarg,opt);
        }
        switch(opt) {
        case 0:
            switch(long_options[option_index].val) {
            case RES_FILE:
                per_residue_type = 1;
                per_residue_type_file = fopen_werr(optarg,"w");
                break;
            case SEQ_FILE:
                per_residue = 1;
                per_residue_file = fopen_werr(optarg,"w");
                break;
            case B_FILE:
                printpdb = 1;
                output_pdb = fopen_werr(optarg,"w");
                break;
            case SELECT:
                add_select(optarg);
                break;
            case UNKNOWN:
                add_unknown_option(optarg);
                break;
            default:
                abort(); // what does this even mean?
            }
            break;
        case 'h':
            help();
            exit(EXIT_SUCCESS);
        case 'v':
            printf("%s\n",version);
            exit(EXIT_SUCCESS);
        case 'e': 
            errlog = fopen_werr(optarg,"w");
            freesasa_set_err_out(errlog);
            break;
        case 'o':
            output = fopen_werr(optarg,"w");
            break;
        case 'l':
            printlog = 0;
            break;
        case 'w':
            freesasa_set_verbosity(FREESASA_V_NOWARNINGS);
            break;
        case 'c': {
            FILE *f = fopen_werr(optarg,"r");
            classifier = freesasa_classifier_from_file(f);
            fclose(f);
            if (classifier == NULL) abort_msg("Can't read file '%s'.\n",optarg);
            break;
        }
        case 'n':
            parameters.shrake_rupley_n_points = atoi(optarg);
            parameters.lee_richards_n_slices = atoi(optarg);
            if (parameters.shrake_rupley_n_points <= 0)
                abort_msg("error: Resolution needs to be at least 1 (20 recommended minum for S&R, 5 for L&R).\n");
            break;
        case 'S':
            parameters.alg = FREESASA_SHRAKE_RUPLEY;
            ++alg_set;
            break;
        case 'L':
            parameters.alg = FREESASA_LEE_RICHARDS;
            ++alg_set;
            break;
        case 'p':
            parameters.probe_radius = atof(optarg);
            if (parameters.probe_radius <= 0)
                abort_msg("error: probe radius must be 0 or larger.\n");
            break;
        case 'H':
            structure_options |= FREESASA_INCLUDE_HETATM;
            break;
        case 'Y':
            structure_options |= FREESASA_INCLUDE_HYDROGEN;
            break;
        case 'M':
            structure_options |= FREESASA_SEPARATE_MODELS;
            break;
        case 'm':
            structure_options |= FREESASA_JOIN_MODELS;
            break;
        case 'C':
            structure_options |= FREESASA_SEPARATE_CHAINS;
            break;
        case 'r':
            per_residue_type = 1;
            break;
        case 'R':
            per_residue = 1;
            break;
        case 'b':
        case 'B':
            printpdb = 1;
            break;
        case 'g':
            add_chain_groups(optarg);
            break;
        case 't':
#if USE_THREADS
            parameters.n_threads = atoi(optarg);
            if (parameters.n_threads < 1) abort_msg("Number of threads must be 1 or larger.\n");
#else
            abort_msg("Option '-t' only defined if program compiled with thread support.\n");
#endif
            break;
        case ':':
            abort_msg("Option '-%c' missing argument.\n",optopt);
        case '?':
        default:
            fprintf(stderr, "%s: warning: Unknown option '-%c' (will be ignored)\n",
                    program_name,opt);
            break;
        }
    }
    if (output == NULL) output = stdout;
    if (per_residue_type_file == NULL) per_residue_type_file = output;
    if (per_residue_file == NULL) per_residue_file = output;
    if (output_pdb == NULL) output_pdb = output;
    if (alg_set > 1) abort_msg("Multiple algorithms specified.\n");
    if (opt_set['m'] && opt_set['M']) abort_msg("The options -m and -M can't be combined.\n");
    if (opt_set['g'] && opt_set['C']) abort_msg("The options -g and -C can't be combined.\n");
    if (printlog) fprintf(output,"## %s %s ##\n",program_name,version);
    if (argc > optind) {
        for (int i = optind; i < argc; ++i) {
            errno = 0;
            input = fopen(argv[i],"r");
            if (input != NULL) {
                run_analysis(input,argv[i]);
                fclose(input);
            } else {
                abort_msg("Opening file '%s'; %s\n",argv[i],strerror(errno));
            }
        }
    } else {
        if (!isatty(STDIN_FILENO)) run_analysis(stdin,"stdin");
        else abort_msg("No input.\n", program_name);
    }

    if (classifier) freesasa_classifier_free(classifier);
    if (output_pdb) fclose(output_pdb);
    if (per_residue_type_file) fclose(per_residue_type_file);
    if (per_residue_file) fclose(per_residue_file);
    if (errlog) fclose(errlog);
    return EXIT_SUCCESS;
}
