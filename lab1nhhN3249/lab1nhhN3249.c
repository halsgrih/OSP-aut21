// Call plugin_process_file() from given .so
//
// Compile with:
// gcc -o lab1call lab1call.c -ldl
//
// (c) Alexei Guirik, 2021
// This source is licensed under CC BY-NC 4.0 
// (https://creativecommons.org/licenses/by-nc/4.0/)
//  

#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include "plugin_api.h"

static int count = 0;
static int opts_to_pass_len = 0;
static struct option *opts_to_pass = NULL;
static struct option *longopts = NULL;

static int opts_to_pass2_len = 0;
static struct option *opts_to_pass2 = NULL;
static struct option *longopts2 = NULL;
static char *folder_working;
static char *folder_lib;
static int need_lib2;
static char **lib_name;

typedef int (*pgi_func_t)(struct plugin_info*);
static pgi_func_t pgi_func = NULL;
typedef int (*ppf_func_t)(const char*, struct option*, size_t);
static void *func = NULL;
static void *func1 = NULL;

static int search_dir(char *folder, int aflag, int oflag, int nflag);
static void get_abs_path(char *path, char *folder, char*file_name);
static void get_lib(char* folder, char *libname, int i);
static int is_lib(const char *filename);
static char *get_filename_ext(const char *filename);

int main(int argc, char *argv[]) {
    
    char *DEBUG = getenv("LAB1DEBUG");
    // Minimum number of arguments is 3: 
    // $ program_name lib_name --opt1
    if (argc < 3) {        
        fprintf(stdout, "Usage: ./lab1nhhN3249 libnhhN3249.so --options [value] \n");
        return 0;
    }

    // short option
    int aflag = 0;
    int oflag = 0;
    int nflag = 0;
    int vflag = 0;
    int hflag = 0;
    char *pvalue = NULL;
    int short_opt;
    
    need_lib2 = 0;
    opterr = 0;
    while ((short_opt = getopt(argc, argv, "-AONvhP:")) != -1){
        //fprintf(stdout, "short_opt = %c\t optind = %d\n", (char) short_opt, optind);
        switch (short_opt){
            case 'A':
                aflag = 1;
                need_lib2 = 1;
                break;
            case 'O':
                oflag = 1;
                need_lib2 = 1;
                break;
            case 'N':
                nflag = 1;
                need_lib2 = 1;
                break;
            case 'v':
                vflag = 1;
                need_lib2 = 1;
                break;
            case 'h':
                hflag = 1;
                need_lib2 = 1;
                break;    
            case 'P':
                pvalue = optarg;
                break;
            case '?':
                if (optopt == 'P'){
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                    return -1;
                } else {
                    break;
                }              
        }

        if (optind < argc){
             if (argv[optind][1] == '-'){    // long option is detected 
                goto START;
            }
        }
    }

    START: ;
    

    if (DEBUG){
        fprintf(stdout,"\nargv[]: ");
        for (int i = 0; i < argc; i++){
            fprintf(stdout, "%s ", argv[i]);
        }
        fprintf(stdout,"\n");
    }
    int num_iter = need_lib2 + 1;

    if (DEBUG){
        fprintf(stdout, "\nDEBUG: Short options: aflag = %d, oflag = %d, nflag = %d, vflag = %d, hflag = %d, pvalue = %s, num_iter = %d\n", aflag, oflag, nflag, vflag, hflag, pvalue, num_iter);
    }
    lib_name = (char**)malloc(num_iter * sizeof(char*)); //NULL;
    for (int i = 0; i < num_iter; ++i){
        lib_name[i] = (char*)malloc(sizeof(char)*PATH_MAX);
    }
    
    folder_working = malloc(sizeof(char)*PATH_MAX);
    folder_lib = malloc(sizeof(char)*PATH_MAX);

    if (getcwd(folder_working, PATH_MAX) != NULL) {
           if (DEBUG) fprintf(stdout, "Current working dir: %s\n", folder_working);
        } else {
            if (DEBUG) perror("getcwd() error: ");
            goto END;
    }

    if (pvalue != NULL){
        strcpy(folder_lib, pvalue);
    } else {
        strcpy(folder_lib, folder_working);
    }


    struct plugin_info pi[2];
    int ret;
    void *dl[2] = {NULL, NULL};


    if (((argv[1][0] == '-') || ((argv[2][0] == '-'))) && (need_lib2 == 1)){
        fprintf(stderr, "ERROR: The program need two dynamic libraries (*.so)\n");
        goto END;
    }

    // Name of the main lib. Should be passed as the first argument.

    for (int i = 0; i < num_iter; ++i){
        get_lib(folder_lib, argv[i+1], i);
    }
            
    if (DEBUG) fprintf(stdout, "\nUsing libraries:\n %s\n %s\n", lib_name[0], lib_name[1]);

    for (int i = 0; i < num_iter; ++i){
        dl[i] = dlopen(lib_name[i], RTLD_LAZY);
        if (!dl[i]){
            fprintf(stderr, "ERROR: dlopen() failed: %s\n", dlerror());
            goto END;
        } 

        // Check for plugin_get_info() func
        if (!dlsym(dl[i], "plugin_get_info")) {
            fprintf(stderr, "ERROR: dlsym() failed: %s\n", dlerror());
            goto END;
        } else {
            if (i == 0){
                func = dlsym(dl[i], "plugin_get_info");
                pgi_func = (pgi_func_t)func;
            }   
            else if (i == 1){
                func1 = dlsym(dl[i], "plugin_get_info");
                pgi_func = (pgi_func_t)func1;
            }
        }

        ret = pgi_func(&pi[i]);
        if (ret < 0) {        
            fprintf(stderr, "ERROR: plugin_get_info() failed\n");
            goto END;
        }
       
        // If library supports no options then we have to stop
        if (pi[i].sup_opts_len == 0) {
            fprintf(stderr, "ERROR: library supports no options! How so?\n");
            goto END;
        }


        // Get pointer to plugin_process_file()
        if (i == 0)///!!!!!!!!!!!!!!!!!!!!
        {
            func = dlsym(dl[i], "plugin_process_file");
            pgi_func = (pgi_func_t)func;
        }   
        else if (i == 1)
        {
            func1 = dlsym(dl[i], "plugin_process_file");
          pgi_func = (pgi_func_t)func1;
        }       
            
    
        // Prepare array of options for getopt_long
        
        // Copy option information
        if (i == 0){
            longopts = calloc(pi[i].sup_opts_len + 1, sizeof(struct option));
            if (!longopts) {
                fprintf(stderr, "ERROR: calloc() failed: %s\n", strerror(errno));
                goto END;
            }
            for (size_t j = 0; j < pi[i].sup_opts_len; j++) {
                // Mind this!
                // getopt_long() requires array of struct option in its longopts arg,
                // but pi.sup_opts is array of plugin_option structs, not option structs.
                memcpy(longopts + j, &pi[i].sup_opts[j].opt, sizeof(struct option));
            }
        } else if (i == 1){
            longopts2 = calloc(pi[i].sup_opts_len + 1, sizeof(struct option));
            if (!longopts2) {
                fprintf(stderr, "ERROR: calloc() failed: %s\n", strerror(errno));
                goto END;
            }
            for (size_t j = 0; j < pi[i].sup_opts_len; j++) {
                    // Mind this!
                    // getopt_long() requires array of struct option in its longopts arg,
                    // but pi.sup_opts is array of plugin_option structs, not option structs.
                    memcpy(longopts2 + j, &pi[i].sup_opts[j].opt, sizeof(struct option));
            }
        }
        // Prepare array of actually used options that will be passed to 
        // plugin_process_file() (Maximum pi.sup_opts_len options)
        if (i == 0){
            opts_to_pass = calloc(pi[i].sup_opts_len, sizeof(struct option));
            if (!opts_to_pass) {
                fprintf(stderr, "ERROR: calloc() failed: %s\n", strerror(errno));
                goto END;
            }
        }else if (i == 1){
            opts_to_pass2 = calloc(pi[i].sup_opts_len, sizeof(struct option));
            if (!opts_to_pass2) {
                fprintf(stderr, "ERROR: calloc() failed: %s\n", strerror(errno));
                goto END;
            }
        }
        
    }

    if (vflag){   
        if (ret < 0) {        
            fprintf(stderr, "ERROR: plugin_get_info() failed\n");
            goto END;
        }
        // Plugin info       
        fprintf(stdout, "Plugin purpose:\t\t%s\n", pi[0].plugin_purpose);
        fprintf(stdout, "Plugin author:\t\t%s\n", pi[0].plugin_author);
        fprintf(stdout, "Supported options: ");
        if (pi[0].sup_opts_len > 0) {
            fprintf(stdout, "\n");
            for (size_t i = 0; i < pi[0].sup_opts_len; i++) {
                fprintf(stdout, "\t--%s\t\t%s\n", pi[0].sup_opts[i].opt.name, pi[0].sup_opts[i].opt_descr);
            }
        } else {
            fprintf(stdout, "none (!?)\n");
        }
        fprintf(stdout, "\n");
        return 1;
    }    

    if (hflag){
        fprintf(stdout, "\n\t-P dir\t\t Plugin directory.\
                    \n\t-A\t\t Combine plugin options using AND operation (default).\
                    \n\t-O\t\t Combine plugin options using OR operation.\
                    \n\t-N\t\t Inverting the search condition (after combining options plugins with -A or -O).\
                    \n\t-v\t\t Display the version of the program and information about the program (full name performer, group number, laboratory version number).\
                    \n\t-h\t\t Display help for options.\n");
        for (int j = 0; j < 2; j++) {
            if (pi[j].sup_opts_len > 0) {
                fprintf(stdout, "\n");
                for (size_t i = 0; i < pi[j].sup_opts_len; i++) {
                    fprintf(stdout, "\t--%s\t\t%s\n", pi[j].sup_opts[i].opt.name, pi[j].sup_opts[i].opt_descr);
                }
            } else {
                fprintf(stdout, "none (!?)\n");
                return -1;
            }
        }
        return 1;
    }

    //printf("Here\n");
    // Now process options for the lib
    opterr = 0;
    int ret2 = -1;
    int lib_number = -1;
    int opt_count = 0, opt_count2 = 0;
    char* opt_arg1 = NULL;
    int val_len_opts2 = 0;
    if (need_lib2 == 1){
        val_len_opts2 = pi[1].sup_opts_len;
    }
    //printf("\noptind after getopt() calling %d\n", optind);
    
    while (1) {
        int opt_ind = 0;
        //printf("argv[%d] = %s\n", optind, argv[optind]);
        ret = getopt_long(argc, argv, "", longopts, &opt_ind);
        //printf("argv[%d] = %s\n", optind, argv[optind]);

        if (ret == 0){
            ret2 = 0;
            opt_arg1 = optarg;
            opt_count++;
            lib_number = 1;

        }
    
        if ((ret == '?') && (ret2 != -1)){
            //printf("ret = %d ret2 = %d\n", ret, ret2);
            memcpy(longopts + opt_ind, (int*)opt_arg1, sizeof(&opt_arg1));
            optind--;
            
            ret2 = getopt_long(argc, argv, "", longopts2, &opt_ind);
            //printf("argv[%d] = %s\n", optind, argv[optind]); 
            //printf("longopts2 %s\n",longopts2[1]->name);
            if (ret2 == 0){
                ret = 0;
                opt_count2++;
                lib_number = 2;
            }
        }

        if (ret == -1) break;
            
        if (ret != 0) {
            //continue;
            fprintf(stderr, "ERROR: failed to parse options\n");
            goto END;
        }
   
        // Check how many options we got up to this moment
        if ((size_t)opts_to_pass_len == pi[0].sup_opts_len + val_len_opts2) {
            fprintf(stderr, "ERROR: too many options!\n");
            goto END;
        }
        
        if (lib_number == 1){
            // Add this option to array of options actually passed to plugin_process_file()
            memcpy(opts_to_pass + opts_to_pass_len, longopts + opt_ind, sizeof(struct option));
       
            // Argument (if any) is passed in flag
            if ((longopts + opt_ind)->has_arg) {
            // Mind this!
            // flag is of type int*, but we are passing char* here (it's ok to do so).
                (opts_to_pass + opts_to_pass_len)->flag = (int*)strdup(optarg);
                
            }
            
            opts_to_pass_len++;  
              
        } else if (need_lib2 == 1) {
            memcpy(opts_to_pass2 + opts_to_pass2_len, longopts2 + opt_count2 - 1, sizeof(struct option));
            
            // Argument (if any) is passed in flag
            if ((longopts2 + (opt_count2 - 1))->has_arg) {
                // Mind this!
                // flag is of type int*, but we are passing char* here (it's ok to do so). 
                (opts_to_pass2 + opts_to_pass2_len)->flag = (int*)strdup(optarg);
            }

            opts_to_pass2_len++;
            
        }
    }
     
    if (getenv("LAB1DEBUG")) {
        fprintf(stderr, "DEBUG: opts_to_pass_len = %d %d\n", opts_to_pass_len, opts_to_pass2_len);
        for (int i = 0; i < opts_to_pass_len; i++) {
            fprintf(stderr, "DEBUG: passing option '%s' with arg '%s'\n",
                (opts_to_pass + i)->name,
                (char*)(opts_to_pass + i)->flag);
        }

        if (need_lib2 == 1){
            for (int i = 0; i < opts_to_pass2_len; i++) {
                fprintf(stderr, "DEBUG: passing option '%s' with arg '%s'\n",
                    (opts_to_pass2 + i)->name,
                    (char*)(opts_to_pass2 + i)->flag);
            }
        }
    }

    ret = search_dir(folder_working, aflag, oflag, nflag);
    fprintf(stdout, "\n%d file(s) have been found\n", count);
    
    END:
    if (opts_to_pass) {
        for (int i = 0; i < opts_to_pass_len; i++)
            free( (opts_to_pass + i)->flag );
        free(opts_to_pass);
    }
    if (opts_to_pass2) {
        for (int i = 0; i < opts_to_pass2_len; i++)
            free( (opts_to_pass2 + i)->flag );
        free(opts_to_pass2);
    }
   
    if (longopts) free(longopts);

    if (longopts2) free(longopts2);

    for (int i = 0; i < num_iter; i++){
        if (lib_name[i]) free(lib_name[i]);
        dlclose(dl[i]);
    }
    
    if (lib_name) free(lib_name);
    if (folder_working) free(folder_working);
    if (folder_lib) free (folder_lib);
    
    return 0;
}

int search_dir(char *folder, int aflag, int oflag, int nflag){
    if (getenv("LAB1DEBUG")) fprintf(stdout, "\n\nWORKING FOLDER %s\n", folder);
    char file_name0[255] = "";

    //char *file_name0 = malloc(sizeof(char)*PATH_MAX*2);
    if (folder == NULL){
        return 1;
    }
    
    int ret = 0, ret2 = 0;
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir (folder)) != NULL) {
        //printf ("\nFolder opened\n");
        /* Parse all the files within directory */
        while ((ent = readdir (dir)) != NULL) {
            if (ent->d_type == DT_REG){
                get_abs_path(file_name0, folder, ent->d_name);
                if (getenv("LAB1DEBUG")) printf ("\nCHECKING FILE: %s\n", file_name0);
                if (file_name0 != NULL){
                    // Call plugin_process_file()
                    errno = 0;
                    ppf_func_t ppf_func = (ppf_func_t)func;
                    ret = ppf_func(file_name0, opts_to_pass, opts_to_pass_len);
                    //fprintf(stdout, "%ls\n",opts_to_pass->flag);
                    if (getenv("LAB1DEBUG")) fprintf(stdout, "plugin_process_file() returned %d\n", ret);
                    if (ret < 0) {
                        if (getenv("LAB1DEBUG")) fprintf(stdout, "Error information: %s\n", strerror(errno));
                    } 
                    
                    if (need_lib2 == 1){
                        ppf_func_t ppf_func1 = (ppf_func_t)func1;
                        ret2 = ppf_func1(file_name0, opts_to_pass2, opts_to_pass2_len);
                        if (getenv("LAB1DEBUG")) fprintf(stdout, "plugin_process_file() returned %d\n", ret2);
                        if (ret2 < 0) {
                            fprintf(stdout, "Error information: %s\n", strerror(errno));
                        }
                    }

                    if (aflag){
                        if (nflag){
                            if (!(ret == 1 && ret2 == 1)){
                                ++count;
                                fprintf(stdout, "%s\n",file_name0);
                            }
                            continue;
                        } else {
                            if (ret == 1 && ret2 == 1){
                                ++count;
                                fprintf(stdout, "%s\n",file_name0);
                            }
                            continue;
                        }
                    } else if (oflag){
                        if (nflag){
                            if (!(ret == 1 || ret2 == 1)){
                                ++count;
                                fprintf(stdout, "%s\n",file_name0);
                            }
                            continue;
                        } else {
                            if (ret == 1 || ret2 == 1){
                                ++count;
                                fprintf(stdout, "%s\n",file_name0);
                            }
                            continue;
                        }
                    } else if (ret == 1){
                        ++count;
                        fprintf(stdout, "%s\n",file_name0);
                    }                
                } else {
                    perror("Error when getting absolute path file: ");
                    //if (file_name0) free(file_name0);
                    return -1;
                }
                
            } else if (ent->d_type == DT_DIR){
               
                if ((strcmp(ent->d_name, ".") != 0) && (strcmp(ent->d_name, "..") != 0)){
                    get_abs_path(file_name0, folder, ent->d_name);
                    search_dir(file_name0, aflag, oflag, nflag);
                } 
            }
        }
        //free(file_name0);
        closedir (dir);
        return 1;
    } else {
        // could not open directory
        perror ("Error when open directory: ");
        return EXIT_FAILURE;
    }
       
}


void get_abs_path (char *path, char *dir, char *fname){
    //char *path = malloc(sizeof(char)*PATH_MAX*2);
    //printf("\nBefore calling func: path %s dir %s fname %s\n", path, dir, fname);
    strcpy(path, dir);
    strcat(path, "/");
    strcat(path, fname);    
    //printf("\npath %s", path);
}

void get_lib(char* folder, char *libname, int i){
    char file_name0[255] = "";
    DIR *dir;
    struct dirent *ent;
    //fprintf(stdout, "\n\nFOLDER LIB %s\n", folder);
    if ((dir = opendir (folder)) != NULL) {
        //printf ("\nFolder lib opened");
        /* Parse all the files within directory */
        while ((ent = readdir (dir)) != NULL) {
            if (ent->d_type == DT_REG){
                get_abs_path(file_name0, folder, ent->d_name);
                //printf("\nFILE LIB %s\n", file_name0);
                if (file_name0 != NULL){
                    if (is_lib(file_name0)){
                        //printf ("\n\n %s %s\n", ent->d_name, libname);
                        if (strcmp(ent->d_name, libname) == 0){
                            strcpy(lib_name[i], file_name0);
                            closedir (dir);
                            return;
                        }
                    }
                } else {
                    perror("Error when getting absolute path file: ");
                    return;
                }
                
            } else if (ent->d_type == DT_DIR){
                
                if ((strcmp(ent->d_name, ".") != 0) && (strcmp(ent->d_name, "..") != 0)){
                    fprintf(stdout, "\n\nSUBFOLDER LIB %s\n", folder);
                    get_abs_path(file_name0, folder, ent->d_name);
                    //printf("\nCall recursively\n");
                    get_lib(file_name0, libname, i);
                } else {
                    //fprintf(stdout, "\nCHECKED\n");
                }
            }
        }
        //perror ("\nReach end ");
        closedir (dir);
        return;
    } else {
        // could not open directory
        perror ("Error opendir(): ");
        return;
    }
    
}

int is_lib(const char *filename) { 
    //fprintf(stdout, "file name parsed to is_lib() %s\n", filename);
    char *ext = get_filename_ext(filename);
    if (strcmp(ext,".so") == 0){
        return 1;
    } else {
        return 0;
    }
}

char *get_filename_ext(const char *filename) {
    char *dot = strrchr(filename, '.');
    //fprintf(stdout, "dot %s\n", dot);
    if(!dot || dot == filename) 
        return "";
    return dot;
}