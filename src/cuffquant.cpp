/*
 *  cuffdiff.cpp
 *  cufflinks
 *
 *  Created by Cole Trapnell on 10/21/09.
 *  Copyright 2009 Cole Trapnell. All rights reserved.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#else
#define PACKAGE_VERSION "INTERNAL"
#define SVN_REVISION "XXX"
#endif


#include <stdlib.h>
#include <getopt.h>
#include <string>
#include <numeric>
#include <cfloat>
#include <iostream>
#include <fstream>

#include "common.h"
#include "hits.h"
#include "bundles.h"
#include "abundances.h"
#include "tokenize.h"
#include "biascorrection.h"
#include "update_check.h"

#include <boost/thread.hpp>
#include <boost/version.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/matrix_proxy.hpp>
#include <boost/numeric/ublas/vector.hpp>
#include <boost/numeric/ublas/vector_proxy.hpp>
#include <boost/numeric/ublas/io.hpp>
#include <boost/algorithm/string.hpp>

#include "replicates.h"
#include "tracking.h"

// Need at least this many reads in a locus to do any testing on it

vector<string> sample_labels;

using namespace std;
using namespace boost;

// We leave out the short codes for options that don't take an argument
#if ENABLE_THREADS
const char *short_options = "m:p:s:c:I:j:L:M:o:b:TNqvuF:C:";
#else
const char *short_options = "m:s:c:I:j:L:M:o:b:TNqvuF:C:";
#endif



static struct option long_options[] = {
{"frag-len-mean",			required_argument,       0,          'm'},
{"frag-len-std-dev",        required_argument,       0,          's'},
{"transcript-score-thresh", required_argument,       0,          't'},
{"pre-mrna-fraction",		required_argument,		 0,			 'j'},
{"max-intron-length",		required_argument,		 0,			 'I'},
{"labels",					required_argument,		 0,			 'L'},
{"min-alignment-count",     required_argument,		 0,			 'c'},
{"FDR",					    required_argument,		 0,			 OPT_FDR},
{"seed",                    required_argument,		 0,			 OPT_RANDOM_SEED},
{"mask-file",               required_argument,		 0,			 'M'},
{"contrast-file",           required_argument,		 0,			 'C'},
{"norm-standards-file",     required_argument,		 0,			 OPT_NORM_STANDARDS_FILE},
{"use-sample-sheet",        no_argument,             0,			 OPT_USE_SAMPLE_SHEET},
{"output-dir",			    required_argument,		 0,			 'o'},
{"verbose",			    	no_argument,			 0,			 'v'},
{"quiet",			    	no_argument,			 0,			 'q'},
{"frag-bias-correct",       required_argument,		 0,			 'b'},
{"multi-read-correct",      no_argument,			 0,			 'u'},
{"time-series",             no_argument,             0,			 'T'},
{"upper-quartile-norm",     no_argument,	 		 0,	         'N'},
{"geometric-norm",          no_argument,	 		 0,	         OPT_GEOMETRIC_NORM},
{"raw-mapped-norm",         no_argument,	 		 0,	         OPT_RAW_MAPPED_NORM},
{"min-isoform-fraction",    required_argument,       0,          'F'},
#if ENABLE_THREADS
{"num-threads",				required_argument,       0,          'p'},
#endif
{"library-type",		    required_argument,		 0,			 OPT_LIBRARY_TYPE},
{"seed",                    required_argument,		 0,			 OPT_RANDOM_SEED},
{"no-collapse-cond-prob",   no_argument,             0,			 OPT_COLLAPSE_COND_PROB},
{"num-importance-samples",  required_argument,		 0,			 OPT_NUM_IMP_SAMPLES},
{"max-mle-iterations",		required_argument,		 0,			 OPT_MLE_MAX_ITER},
{"min-mle-accuracy",		required_argument,		 0,			 OPT_MLE_MIN_ACC},
{"poisson-dispersion",		no_argument,             0,		     OPT_POISSON_DISPERSION},
{"bias-mode",               required_argument,		 0,			 OPT_BIAS_MODE},
{"no-update-check",         no_argument,             0,          OPT_NO_UPDATE_CHECK},
{"emit-count-tables",       no_argument,             0,          OPT_EMIT_COUNT_TABLES},
{"compatible-hits-norm",    no_argument,	 		 0,	         OPT_USE_COMPAT_MASS},
{"total-hits-norm",         no_argument,	 		 0,	         OPT_USE_TOTAL_MASS},
//{"analytic-diff",           no_argument,	 		 0,	         OPT_ANALYTIC_DIFF},
{"no-diff",                 no_argument,	 		 0,	         OPT_NO_DIFF},
{"num-frag-count-draws",	required_argument,		 0,			 OPT_NUM_FRAG_COUNT_DRAWS},
{"num-frag-assign-draws",	required_argument,		 0,			 OPT_NUM_FRAG_ASSIGN_DRAWS},
    
// Some options for testing different stats policies
{"max-bundle-frags",        required_argument,       0,          OPT_MAX_FRAGS_PER_BUNDLE}, 
{"read-skip-fraction",      required_argument,	     0,          OPT_READ_SKIP_FRACTION},
{"no-read-pairs",           no_argument,	 		 0,          OPT_NO_READ_PAIRS},
{"trim-read-length",        required_argument,	     0,          OPT_TRIM_READ_LENGTH},
{"cov-delta",               required_argument,	     0,          OPT_MAX_DELTA_GAP},
{"locus-count-dispersion",  no_argument,             0,          OPT_LOCUS_COUNT_DISPERSION},
{"max-frag-multihits",      required_argument,       0,          OPT_FRAG_MAX_MULTIHITS},
{"min-outlier-p",           required_argument,       0,          OPT_MIN_OUTLIER_P},
{"min-reps-for-js-test",      required_argument,     0,          OPT_MIN_REPS_FOR_JS_TEST},
{"no-effective-length-correction",  no_argument,     0,          OPT_NO_EFFECTIVE_LENGTH_CORRECTION},
{"no-length-correction",    no_argument,             0,          OPT_NO_LENGTH_CORRECTION},
{"no-js-tests",             no_argument,             0,          OPT_NO_JS_TESTS},
{"dispersion-method",       required_argument,       0,          OPT_DISPERSION_METHOD},
{"library-norm-method",     required_argument,       0,          OPT_LIB_NORM_METHOD},
{"no-scv-correction",       no_argument,             0,          OPT_NO_SCV_CORRECTION},
{0, 0, 0, 0} // terminator
};

void print_usage()
{
	fprintf(stderr, "cuffquant v%s (%s)\n", PACKAGE_VERSION, SVN_REVISION); 
	fprintf(stderr, "-----------------------------\n"); 
	
	//NOTE: SPACES ONLY, bozo
    fprintf(stderr, "Usage:   cuffdiff [options] <transcripts.gtf> <sample1_hits.sam> <sample2_hits.sam> [... sampleN_hits.sam]\n");
	fprintf(stderr, "   Supply replicate SAMs as comma separated lists for each condition: sample1_rep1.sam,sample1_rep2.sam,...sample1_repM.sam\n");
    fprintf(stderr, "General Options:\n");
    fprintf(stderr, "  -o/--output-dir              write all output files to this directory              [ default:     ./ ]\n");
    fprintf(stderr, "  -L/--labels                  comma-separated list of condition labels\n");
	fprintf(stderr, "  --FDR                        False discovery rate used in testing                  [ default:   0.05 ]\n");
	fprintf(stderr, "  -M/--mask-file               ignore all alignment within transcripts in this file  [ default:   NULL ]\n");
    fprintf(stderr, "  --norm-standards-file        Housekeeping/spike genes to normalize libraries       [ default:   NULL ]\n"); // NOT YET DOCUMENTED, keep secret for now
    fprintf(stderr, "  -b/--frag-bias-correct       use bias correction - reference fasta required        [ default:   NULL ]\n");
    fprintf(stderr, "  -u/--multi-read-correct      use 'rescue method' for multi-reads                   [ default:  FALSE ]\n");
#if ENABLE_THREADS
	fprintf(stderr, "  -p/--num-threads             number of threads used during quantification          [ default:      1 ]\n");
#endif
    fprintf(stderr, "  --library-type               Library prep used for input reads                     [ default:  below ]\n");
    fprintf(stderr, "  --library-norm-method        Method used to normalize library sizes                [ default:  below ]\n");
    
    fprintf(stderr, "\nAdvanced Options:\n");
    fprintf(stderr, "  -m/--frag-len-mean           average fragment length (unpaired reads only)         [ default:    200 ]\n");
    fprintf(stderr, "  -s/--frag-len-std-dev        fragment length std deviation (unpaired reads only)   [ default:     80 ]\n");
    fprintf(stderr, "  -c/--min-alignment-count     minimum number of alignments in a locus for testing   [ default:   10 ]\n");
    fprintf(stderr, "  --max-mle-iterations         maximum iterations allowed for MLE calculation        [ default:   5000 ]\n");
    fprintf(stderr, "  --compatible-hits-norm       count hits compatible with reference RNAs only        [ default:   TRUE ]\n");
    fprintf(stderr, "  --total-hits-norm            count all hits for normalization                      [ default:  FALSE ]\n");
    fprintf(stderr, "  -v/--verbose                 log-friendly verbose processing (no progress bar)     [ default:  FALSE ]\n");
	fprintf(stderr, "  -q/--quiet                   log-friendly quiet processing (no progress bar)       [ default:  FALSE ]\n");
    fprintf(stderr, "  --seed                       value of random number generator seed                 [ default:      0 ]\n");
    fprintf(stderr, "  --no-update-check            do not contact server to check for update availability[ default:  FALSE ]\n");
    fprintf(stderr, "  --max-bundle-frags           maximum fragments allowed in a bundle before skipping [ default: 500000 ]\n");
    fprintf(stderr, "  --max-frag-multihits         Maximum number of alignments allowed per fragment     [ default: unlim  ]\n");
    fprintf(stderr, "  --no-effective-length-correction   No effective length correction                  [ default:  FALSE ]\n");
    fprintf(stderr, "  --no-length-correction       No length correction                                  [ default:  FALSE ]\n");
    
    fprintf(stderr, "\nDebugging use only:\n");
    fprintf(stderr, "  --read-skip-fraction         Skip a random subset of reads this size               [ default:    0.0 ]\n");
    fprintf(stderr, "  --no-read-pairs              Break all read pairs                                  [ default:  FALSE ]\n");
    fprintf(stderr, "  --trim-read-length           Trim reads to be this long (keep 5' end)              [ default:   none ]\n");
    fprintf(stderr, "  --no-scv-correction          Disable SCV correction                                [ default:  FALSE ]\n");
    print_library_table();
    print_dispersion_method_table();
    print_lib_norm_method_table();
}

int parse_options(int argc, char** argv)
{
    int option_index = 0;
    int next_option;
    string sample_label_list;
    string dispersion_method_str;
    string lib_norm_method_str;
    do {
        next_option = getopt_long_only(argc, argv, short_options, long_options, &option_index);
        if (next_option == -1)     /* Done with options. */
            break;
        switch (next_option) {
            case 0:
                /* If this option set a flag, do nothing else now. */
                if (long_options[option_index].flag != 0)
                    break;
                break;
                
			case 'm':
				user_provided_fld = true;
				def_frag_len_mean = (uint32_t)parseInt(0, "-m/--frag-len-mean arg must be at least 0", print_usage);
				break;
			case 's':
				user_provided_fld = true;
				def_frag_len_std_dev = (uint32_t)parseInt(0, "-s/--frag-len-std-dev arg must be at least 0", print_usage);
				break;
			case 'p':
				num_threads = (uint32_t)parseInt(1, "-p/--num-threads arg must be at least 1", print_usage);
				break;
            case 'L':
				sample_label_list = optarg;
				break;
			case OPT_NUM_IMP_SAMPLES:
				num_importance_samples = parseInt(1, "--num-importance-samples must be at least 1", print_usage);
				break;
			case OPT_MLE_MAX_ITER:
				max_mle_iterations = parseInt(1, "--max-mle-iterations must be at least 1", print_usage);
				break;
			case OPT_BIAS_MODE:
				if (!strcmp(optarg, "site"))
					bias_mode = SITE;
				else if (!strcmp(optarg, "pos"))
					bias_mode = POS;
				else if (!strcmp(optarg, "pos_vlmm"))
					bias_mode = POS_VLMM;
				else if (!strcmp(optarg, "vlmm"))
					bias_mode = VLMM;
                else if (!strcmp(optarg, "pos_site"))
					bias_mode = POS_SITE;
				else
				{
					fprintf(stderr, "Unknown bias mode.\n");
					exit(1);
				}
				break;
			case 'M':
			{
				mask_gtf_filename = optarg;
				break;
			}
			case OPT_NORM_STANDARDS_FILE:
			{
				norm_standards_filename = optarg;
				break;
			}
            case 'v':
			{
				if (cuff_quiet)
				{
					fprintf(stderr, "Warning: Can't be both verbose and quiet!  Setting verbose only.\n");
				}
				cuff_quiet = false;
				cuff_verbose = true;
				break;
			}
			case 'q':
			{
				if (cuff_verbose)
				{
					fprintf(stderr, "Warning: Can't be both verbose and quiet!  Setting quiet only.\n");
				}
				cuff_verbose = false;
				cuff_quiet = true;
				break;
			}
            case 'o':
			{
				output_dir = optarg;
				break;
			}
			case 'b':
			{
				fasta_dir = optarg;
				corr_bias = true;
				break;
            }    
            
            case 'u':
            {
                corr_multi = true;
                break;
            }
            case OPT_LIBRARY_TYPE:
			{
				library_type = optarg;
				break;
			}
            case OPT_NO_UPDATE_CHECK:
            {
                no_update_check = true;
                break;
            }
            case OPT_RANDOM_SEED:
            {
                random_seed = parseInt(0, "--seed must be at least 0", print_usage);
                break;
            }  
            case OPT_COLLAPSE_COND_PROB:
            {
                cond_prob_collapse = false;
                break;
            }
            case OPT_USE_COMPAT_MASS:
            {
                use_compat_mass = true;
                break;
            }
            case OPT_USE_TOTAL_MASS:
            {
                use_total_mass = true;
                break;
            }
            case OPT_MAX_FRAGS_PER_BUNDLE:
            {
                max_frags_per_bundle = parseInt(0, "--max-bundle-frags must be at least 0", print_usage);
                break;
            }
            case OPT_READ_SKIP_FRACTION:
            {
                read_skip_fraction = parseFloat(0, 1.0, "--read-skip-fraction must be between 0 and 1.0", print_usage);
                break;
            }
            case OPT_NO_READ_PAIRS:
            {
                no_read_pairs = true;
                break;
            }
            case OPT_TRIM_READ_LENGTH:
            {
                trim_read_length = parseInt(0, "--trim-read-length must be at least 1", print_usage);
                break;
            }
            case OPT_MLE_MIN_ACC:
            {
                bootstrap_delta_gap = parseFloat(0, 10000000.0, "--read-skip-fraction must be between 0 and 10000000.0", print_usage);
                break;
            }
            case OPT_FRAG_MAX_MULTIHITS:
            {
                max_frag_multihits = parseInt(1, "--max-frag-multihits must be at least 1", print_usage);
                break;
            }
            case OPT_NO_EFFECTIVE_LENGTH_CORRECTION:
            {
                no_effective_length_correction = true;
                break;
            }
            case OPT_NO_LENGTH_CORRECTION:
            {
                no_length_correction = true;
                break;
            }
            case OPT_LIB_NORM_METHOD:
			{
				lib_norm_method_str = optarg;
				break;
			}
            
			default:
				print_usage();
				return 1;
        }
    } while(next_option != -1);
	
	if (library_type != "")
    {
        map<string, ReadGroupProperties>::iterator lib_itr = 
		library_type_table.find(library_type);
        if (lib_itr == library_type_table.end())
        {
            fprintf(stderr, "Error: Library type %s not supported\n", library_type.c_str());
            exit(1);
        }
        else 
        {
            if (library_type == "transfrags")
            {
                allow_junk_filtering = false;
            }
            global_read_properties = &lib_itr->second;
        }
    }
    else
    {
        
    }
    
    // Set the count dispersion method to use
    if (dispersion_method_str == "")
    {
        dispersion_method_str = default_dispersion_method;
    }
    
    map<string, DispersionMethod>::iterator disp_itr = 
    dispersion_method_table.find(dispersion_method_str);
    if (disp_itr == dispersion_method_table.end())
    {
        fprintf(stderr, "Error: Dispersion method %s not supported\n", dispersion_method_str.c_str());
        exit(1);
    }
    else 
    {
        dispersion_method = disp_itr->second;
    }

    // Set the library size normalization method to use
    if (lib_norm_method_str == "")
    {
        lib_norm_method_str = default_lib_norm_method;
    }
    
    map<string, LibNormalizationMethod>::iterator lib_norm_itr =
    lib_norm_method_table.find(lib_norm_method_str);
    if (lib_norm_itr == lib_norm_method_table.end())
    {
        fprintf(stderr, "Error: Dispersion method %s not supported\n", lib_norm_method_str.c_str());
        exit(1);
    }
    else
    {
        lib_norm_method = lib_norm_itr->second;
    }


    
    if (use_total_mass && use_compat_mass)
    {
        fprintf (stderr, "Error: please supply only one of --compatibile-hits-norm and --total-hits-norm\n");
        exit(1);
    }
    
    tokenize(sample_label_list, ",", sample_labels);
    
	allow_junk_filtering = false;
	
	return 0;
}

struct Outfiles
{
	FILE* isoform_fpkm_tracking_out;
	FILE* tss_group_fpkm_tracking_out;
	FILE* gene_fpkm_tracking_out;
	FILE* cds_fpkm_tracking_out;
    
    FILE* isoform_count_tracking_out;
	FILE* tss_group_count_tracking_out;
	FILE* gene_count_tracking_out;
	FILE* cds_count_tracking_out;
    
    FILE* run_info_out;
    FILE* read_group_info_out;
    FILE* bias_out;
    FILE* var_model_out;
};



void print_FPKM_tracking(FILE* fout, 
						 const FPKMTrackingTable& tracking)
{
	fprintf(fout,"tracking_id\tclass_code\tnearest_ref_id\tgene_id\tgene_short_name\ttss_id\tlocus\tlength\tcoverage");
	FPKMTrackingTable::const_iterator first_itr = tracking.begin();
	if (first_itr != tracking.end())
	{
		const FPKMTracking& track = first_itr->second;
		const vector<FPKMContext>& fpkms = track.fpkm_series;
		for (size_t i = 0; i < fpkms.size(); ++i)
		{
			fprintf(fout, "\t%s_FPKM\t%s_conf_lo\t%s_conf_hi\t%s_status", sample_labels[i].c_str(), sample_labels[i].c_str(), sample_labels[i].c_str(), sample_labels[i].c_str());
		}
	}
	fprintf(fout, "\n");
	for (FPKMTrackingTable::const_iterator itr = tracking.begin(); itr != tracking.end(); ++itr)
	{
		const string& description = itr->first;
		const FPKMTracking& track = itr->second;
		const vector<FPKMContext>& fpkms = track.fpkm_series;
		
        AbundanceStatus status = NUMERIC_OK;
        BOOST_FOREACH (const FPKMContext& c, fpkms)
        {
            if (c.status == NUMERIC_FAIL)
                status = NUMERIC_FAIL;
        }
        
        string all_gene_ids = cat_strings(track.gene_ids);
		if (all_gene_ids == "")
			all_gene_ids = "-";
        
		string all_gene_names = cat_strings(track.gene_names);
		if (all_gene_names == "")
			all_gene_names = "-";
		
		string all_tss_ids = cat_strings(track.tss_ids);
		if (all_tss_ids == "")
			all_tss_ids = "-";
		
        char length_buff[33] = "-";
        if (track.length)
            sprintf(length_buff, "%d", track.length);
        
        fprintf(fout, "%s\t%c\t%s\t%s\t%s\t%s\t%s\t%s\t%s", 
                description.c_str(),
                track.classcode ? track.classcode : '-',
                track.ref_match.c_str(),
                all_gene_ids.c_str(),
                all_gene_names.c_str(), 
                all_tss_ids.c_str(),
                track.locus_tag.c_str(),
                length_buff,
                "-");
       		
		for (size_t i = 0; i < fpkms.size(); ++i)
		{
			double fpkm = fpkms[i].FPKM;
			//double std_dev = sqrt(fpkms[i].FPKM_variance);
			double fpkm_conf_hi = fpkms[i].FPKM_conf_hi;
			double fpkm_conf_lo = fpkms[i].FPKM_conf_lo;
            const char* status_str = "OK";
            
            if (fpkms[i].status == NUMERIC_OK)
            {
                status_str = "OK";
            }
            else if (fpkms[i].status == NUMERIC_FAIL)
            {
                status_str = "FAIL";
            }
            else if (fpkms[i].status == NUMERIC_LOW_DATA)
            {
                status_str = "LOWDATA";
            }
            else if (fpkms[i].status == NUMERIC_HI_DATA)
            {
                status_str = "HIDATA";
            }
            else
            {
                assert(false);
            }
            
			fprintf(fout, "\t%lg\t%lg\t%lg\t%s", fpkm, fpkm_conf_lo, fpkm_conf_hi, status_str);
		}
		
		fprintf(fout, "\n");
	}
}

void print_count_tracking(FILE* fout, 
						  const FPKMTrackingTable& tracking)
{
	fprintf(fout,"tracking_id");
	FPKMTrackingTable::const_iterator first_itr = tracking.begin();
	if (first_itr != tracking.end())
	{
		const FPKMTracking& track = first_itr->second;
		const vector<FPKMContext>& fpkms = track.fpkm_series;
		for (size_t i = 0; i < fpkms.size(); ++i)
		{
			fprintf(fout, "\t%s_count\t%s_count_variance\t%s_count_uncertainty_var\t%s_count_dispersion_var\t%s_status", sample_labels[i].c_str(), sample_labels[i].c_str(), sample_labels[i].c_str(), sample_labels[i].c_str(), sample_labels[i].c_str());
		}
	}
	fprintf(fout, "\n");
	for (FPKMTrackingTable::const_iterator itr = tracking.begin(); itr != tracking.end(); ++itr)
	{
		const string& description = itr->first;
		const FPKMTracking& track = itr->second;
		const vector<FPKMContext>& fpkms = track.fpkm_series;
		
        AbundanceStatus status = NUMERIC_OK;
        BOOST_FOREACH (const FPKMContext& c, fpkms)
        {
            if (c.status == NUMERIC_FAIL)
                status = NUMERIC_FAIL;
        }
        
        fprintf(fout, "%s", 
                description.c_str());
        
		for (size_t i = 0; i < fpkms.size(); ++i)
		{
            const char* status_str = "OK";
            
            if (fpkms[i].status == NUMERIC_OK)
            {
                status_str = "OK";
            }
            else if (fpkms[i].status == NUMERIC_FAIL)
            {
                status_str = "FAIL";
            }
            else if (fpkms[i].status == NUMERIC_LOW_DATA)
            {
                status_str = "LOWDATA";
            }
            else if (fpkms[i].status == NUMERIC_HI_DATA)
            {
                status_str = "HIDATA";
            }
            else
            {
                assert(false);
            }
            
            double external_counts = fpkms[i].count_mean;
            double external_count_var = fpkms[i].count_var;
            double uncertainty_var = fpkms[i].count_uncertainty_var;
            double dispersion_var = fpkms[i].count_dispersion_var;
			fprintf(fout, "\t%lg\t%lg\t%lg\t%lg\t%s", external_counts, external_count_var, uncertainty_var, dispersion_var, status_str);
		}
		
		fprintf(fout, "\n");
	}
}

void print_run_info(FILE* fout)
{
    fprintf(fout, "param\tvalue\n");
    fprintf(fout, "cmd_line\t%s\n", cmd_str.c_str());
    fprintf(fout, "version\t%s\n", PACKAGE_VERSION);
    fprintf(fout, "SVN_revision\t%s\n",SVN_REVISION); 
    fprintf(fout, "boost_version\t%d\n", BOOST_VERSION);
}


#if ENABLE_THREADS
boost::mutex inspect_lock;

mutex _recorder_lock;
mutex locus_thread_pool_lock;
int locus_curr_threads = 0;
int locus_num_threads = 0;

void decr_pool_count()
{
	locus_thread_pool_lock.lock();
	locus_curr_threads--;
	locus_thread_pool_lock.unlock();
}

#endif



void inspect_map_worker(ReplicatedBundleFactory& fac,
                        int& tmp_min_frag_len, 
                        int& tmp_max_frag_len)
{
#if ENABLE_THREADS
	boost::this_thread::at_thread_exit(decr_pool_count);
#endif
    
    int min_f = std::numeric_limits<int>::max();
    int max_f = 0;
    
    fac.inspect_replicate_maps(min_f, max_f);
    
#if ENABLE_THREADS
    inspect_lock.lock();
#endif
    tmp_min_frag_len = min(min_f, tmp_min_frag_len);
    tmp_max_frag_len = max(max_f, tmp_max_frag_len);
#if ENABLE_THREADS
    inspect_lock.unlock();
#endif
}

void learn_bias_worker(shared_ptr<BundleFactory> fac)
{
#if ENABLE_THREADS
	boost::this_thread::at_thread_exit(decr_pool_count);
#endif
	shared_ptr<ReadGroupProperties> rg_props = fac->read_group_properties();
	BiasLearner* bl = new BiasLearner(rg_props->frag_len_dist());
	learn_bias(*fac, *bl, false);
	rg_props->bias_learner(shared_ptr<BiasLearner>(bl));
}

typedef map<string, vector<AbundanceGroup> > light_ab_group_tracking_table;

// Similiar to TestLauncher, except this class records tracking data when abundance groups report in
struct AbundanceRecorder
{
private:
    AbundanceRecorder(AbundanceRecorder& rhs) {}
    
public:
    AbundanceRecorder(int num_samples,
                      Tracking* tracking,
                      ProgressBar* p_bar)
        :
        _orig_workers(num_samples),
        _tracking(tracking),
        _p_bar(p_bar)
        {
        }
    
    void operator()();
    
    void register_locus(const string& locus_id);
    void abundance_avail(const string& locus_id,
                         shared_ptr<SampleAbundances> ab,
                         size_t factory_id);
    void record_finished_loci();
    void record_tracking_data(const string& locus_id, vector<shared_ptr<SampleAbundances> >& abundances);
    bool all_samples_reported_in(vector<shared_ptr<SampleAbundances> >& abundances);
    bool all_samples_reported_in(const string& locus_id);
    
    void clear_tracking_data() { _tracking->clear(); }
    
    typedef list<pair<string, vector<shared_ptr<SampleAbundances> > > > recorder_sample_table;
    
    const light_ab_group_tracking_table& get_sample_table() const { return _ab_group_tracking_table; }
    
private:
    
    recorder_sample_table::iterator find_locus(const string& locus_id);
    
    int _orig_workers;
    
    recorder_sample_table _samples;
    
    Tracking* _tracking;
    ProgressBar* _p_bar;
    
    light_ab_group_tracking_table _ab_group_tracking_table;
};


AbundanceRecorder::recorder_sample_table::iterator AbundanceRecorder::find_locus(const string& locus_id)
{
    recorder_sample_table::iterator itr = _samples.begin();
    for(; itr != _samples.end(); ++itr)
    {
        if (itr->first == locus_id)
            return itr;
    }
    return _samples.end();
}

void AbundanceRecorder::register_locus(const string& locus_id)
{
#if ENABLE_THREADS
	boost::mutex::scoped_lock lock(_recorder_lock);
#endif
    
    recorder_sample_table::iterator itr = find_locus(locus_id);
    if (itr == _samples.end())
    {
        pair<recorder_sample_table::iterator, bool> p;
        vector<shared_ptr<SampleAbundances> >abs(_orig_workers);
        _samples.push_back(make_pair(locus_id, abs));
    }
}

void AbundanceRecorder::abundance_avail(const string& locus_id,
                                   shared_ptr<SampleAbundances> ab,
                                   size_t factory_id)
{
#if ENABLE_THREADS
	boost::mutex::scoped_lock lock(_recorder_lock);
#endif
    recorder_sample_table::iterator itr = find_locus(locus_id);
    if (itr == _samples.end())
    {
        assert(false);
    }
    itr->second[factory_id] = ab;
    //itr->second(factory_id] = ab;
}

// Note: this routine should be called under lock - it doesn't
// acquire the lock itself.
bool AbundanceRecorder::all_samples_reported_in(vector<shared_ptr<SampleAbundances> >& abundances)
{
    BOOST_FOREACH (shared_ptr<SampleAbundances> ab, abundances)
    {
        if (!ab)
        {
            return false;
        }
    }
    return true;
}

#if ENABLE_THREADS
mutex test_storage_lock; // don't modify the above struct without locking here
#endif


// Note: this routine should be called under lock - it doesn't
// acquire the lock itself.
void AbundanceRecorder::record_tracking_data(const string& locus_id, vector<shared_ptr<SampleAbundances> >& abundances)
{
    assert (abundances.size() == _orig_workers);
    
    // Just verify that all the loci from each factory match up.
    for (size_t i = 1; i < abundances.size(); ++i)
    {
        const SampleAbundances& curr = *(abundances[i]);
        const SampleAbundances& prev = *(abundances[i-1]);
        
        assert (curr.locus_tag == prev.locus_tag);
        
        const AbundanceGroup& s1 = curr.transcripts;
        const AbundanceGroup& s2 =  prev.transcripts;
        
        assert (s1.abundances().size() == s2.abundances().size());
        
        for (size_t j = 0; j < s1.abundances().size(); ++j)
        {
            assert (s1.abundances()[j]->description() == s2.abundances()[j]->description());
        }
    }
    
#if ENABLE_THREADS
	test_storage_lock.lock();
#endif
    
    vector<AbundanceGroup> lightweight_ab_groups;
    
    // Add all the transcripts, CDS groups, TSS groups, and genes to their
    // respective FPKM tracking table.  Whether this is a time series or an
    // all pairs comparison, we should be calculating and reporting FPKMs for
    // all objects in all samples
	for (size_t i = 0; i < abundances.size(); ++i)
	{
		const AbundanceGroup& ab_group = abundances[i]->transcripts;
        //fprintf(stderr, "[%d] count = %lg\n",i,  ab_group.num_fragments());
		BOOST_FOREACH (shared_ptr<Abundance> ab, ab_group.abundances())
		{
			add_to_tracking_table(i, *ab, _tracking->isoform_fpkm_tracking);
            //assert (_tracking->isoform_fpkm_tracking.num_fragments_by_replicate().empty() == false);
		}
		
		BOOST_FOREACH (AbundanceGroup& ab, abundances[i]->cds)
		{
			add_to_tracking_table(i, ab, _tracking->cds_fpkm_tracking);
		}
		
		BOOST_FOREACH (AbundanceGroup& ab, abundances[i]->primary_transcripts)
		{
			add_to_tracking_table(i, ab, _tracking->tss_group_fpkm_tracking);
		}
		
		BOOST_FOREACH (AbundanceGroup& ab, abundances[i]->genes)
		{
			add_to_tracking_table(i, ab, _tracking->gene_fpkm_tracking);
		}
        
        abundances[i]->transcripts.clear_non_serialized_data();
        lightweight_ab_groups.push_back(abundances[i]->transcripts);
	}
    
    _ab_group_tracking_table[locus_id] = lightweight_ab_groups;
    
#if ENABLE_THREADS
    test_storage_lock.unlock();
#endif
    
}

void AbundanceRecorder::record_finished_loci()
{
#if ENABLE_THREADS
	boost::mutex::scoped_lock lock(_recorder_lock);
#endif
    
    recorder_sample_table::iterator itr = _samples.begin();
    while(itr != _samples.end())
    {
        if (all_samples_reported_in(itr->second))
        {
            // In some abundance runs, we don't actually want to perform testing
            // (eg initial quantification before bias correction).
            // _tests and _tracking will be NULL in these cases.
            if (_tracking != NULL)
            {
                if (_p_bar)
                {
                    verbose_msg("Testing for differential expression and regulation in locus [%s]\n", itr->second.front()->locus_tag.c_str());
                    _p_bar->update(itr->second.front()->locus_tag.c_str(), 1);
                }
            }
            record_tracking_data(itr->first, itr->second);
            
            // Removes the samples that have already been tested and transferred to the tracking tables,
            itr = _samples.erase(itr);
        }
        else
        {
            
            ++itr;
        }
    }
}


shared_ptr<AbundanceRecorder> abundance_recorder;

void sample_worker(const RefSequenceTable& rt,
                   ReplicatedBundleFactory& sample_factory,
                   shared_ptr<SampleAbundances> abundance,
                   size_t factory_id,
                   shared_ptr<AbundanceRecorder> recorder)
{
#if ENABLE_THREADS
	boost::this_thread::at_thread_exit(decr_pool_count);
#endif
    
    HitBundle bundle;
    bool non_empty = sample_factory.next_bundle(bundle);
    
    char bundle_label_buf[2048];
    sprintf(bundle_label_buf,
            "%s:%d-%d",
            rt.get_name(bundle.ref_id()),
            bundle.left(),
            bundle.right());
    string locus_tag = bundle_label_buf;
    
    if (!non_empty || (bias_run && bundle.ref_scaffolds().size() != 1)) // Only learn on single isoforms
    {
#if !ENABLE_THREADS
        // If Cuffdiff was built without threads, we need to manually invoke
        // the testing functor, which will check to see if all the workers
        // are done, and if so, perform the cross sample testing.
        recorder->abundance_avail(locus_tag, abundance, factory_id);
        recorder->record_finished_loci();
        //launcher();
#endif
    	return;
    }
    
    abundance->cluster_mass = bundle.mass();
    
    recorder->register_locus(locus_tag);
    
    abundance->locus_tag = locus_tag;
    
    bool perform_cds_analysis = false;
    bool perform_tss_analysis = false;
    
    BOOST_FOREACH(shared_ptr<Scaffold> s, bundle.ref_scaffolds())
    {
        if (s->annotated_tss_id() != "")
        {
            perform_tss_analysis = final_est_run;
        }
        if (s->annotated_protein_id() != "")
        {
            perform_cds_analysis = final_est_run;
        }
    }
    
    set<shared_ptr<ReadGroupProperties const> > rg_props;
    for (size_t i = 0; i < sample_factory.factories().size(); ++i)
    {
        shared_ptr<BundleFactory> bf = sample_factory.factories()[i];
        rg_props.insert(bf->read_group_properties());
    }
    
    sample_abundance_worker(boost::cref(locus_tag),
                            boost::cref(rg_props),
                            boost::ref(*abundance),
                            &bundle,
                            perform_cds_analysis,
                            perform_tss_analysis);
    
    ///////////////////////////////////////////////
    
    
    BOOST_FOREACH(shared_ptr<Scaffold> ref_scaff,  bundle.ref_scaffolds())
    {
        ref_scaff->clear_hits();
    }
    
    recorder->abundance_avail(locus_tag, abundance, factory_id);
    recorder->record_finished_loci();
}

bool quantitate_next_locus(const RefSequenceTable& rt,
                           vector<shared_ptr<ReplicatedBundleFactory> >& bundle_factories,
                           shared_ptr<AbundanceRecorder> recorder)
{
    for (size_t i = 0; i < bundle_factories.size(); ++i)
    {
        shared_ptr<SampleAbundances> s_ab = shared_ptr<SampleAbundances>(new SampleAbundances);
        
#if ENABLE_THREADS					
        while(1)
        {
            locus_thread_pool_lock.lock();
            if (locus_curr_threads < locus_num_threads)
            {
                break;
            }
            
            locus_thread_pool_lock.unlock();
            
            boost::this_thread::sleep(boost::posix_time::milliseconds(5));
            
        }
        
        locus_curr_threads++;
        locus_thread_pool_lock.unlock();
        
        thread quantitate(sample_worker,
                          boost::ref(rt),
                          boost::ref(*(bundle_factories[i])),
                          s_ab,
                          i,
                          recorder);
#else
        sample_worker(boost::ref(rt),
                      boost::ref(*(bundle_factories[i])),
                      s_ab,
                      i,
                      recorder);
#endif
    }
    return true;
}

void parse_norm_standards_file(FILE* norm_standards_file)
{
    char pBuf[10 * 1024];
    size_t non_blank_lines_read = 0;
    
    shared_ptr<map<string, LibNormStandards> > norm_standards(new map<string, LibNormStandards>);
    
    while (fgets(pBuf, 10*1024, norm_standards_file))
    {
        if (strlen(pBuf) > 0)
        {
            char* nl = strchr(pBuf, '\n');
            if (nl)
                *nl = 0;
            
            string pBufstr = pBuf;
            string trimmed = boost::trim_copy(pBufstr);
            
            if (trimmed.length() > 0 && trimmed[0] != '#')
            {
                non_blank_lines_read++;
                vector<string> columns;
                tokenize(trimmed, "\t", columns);
                
                if (non_blank_lines_read == 1)
                    continue;
                
                if (columns.size() < 1) // 
                {
                    continue;
                }
                
                string gene_id = columns[0];
                LibNormStandards L;
                norm_standards->insert(make_pair(gene_id, L));
            }
        }
    }
    lib_norm_standards = norm_standards;
}

shared_ptr<AbundanceRecorder> abx_recorder;

void driver(FILE* ref_gtf, FILE* mask_gtf, FILE* norm_standards_file, vector<string>& sam_hit_filename_lists, Outfiles& outfiles)
{

	ReadTable it;
	RefSequenceTable rt(true, false);
    
	vector<shared_ptr<Scaffold> > ref_mRNAs;
	
	vector<shared_ptr<ReplicatedBundleFactory> > bundle_factories;
    vector<shared_ptr<ReadGroupProperties> > all_read_groups;
    vector<shared_ptr<HitFactory> > all_hit_factories;
    
	for (size_t i = 0; i < sam_hit_filename_lists.size(); ++i)
	{
        vector<string> sam_hit_filenames;
        tokenize(sam_hit_filename_lists[i], ",", sam_hit_filenames);
        
        vector<shared_ptr<BundleFactory> > replicate_factories;
        
        string condition_name = sample_labels[i];
        
        for (size_t j = 0; j < sam_hit_filenames.size(); ++j)
        {
            shared_ptr<HitFactory> hs;
            try
            {
                hs = shared_ptr<HitFactory>(new BAMHitFactory(sam_hit_filenames[j], it, rt));
            }
            catch (std::runtime_error& e) 
            {
                try
                {
                    fprintf(stderr, "File %s doesn't appear to be a valid BAM file, trying SAM...\n",
                            sam_hit_filenames[j].c_str());
                    hs = shared_ptr<HitFactory>(new SAMHitFactory(sam_hit_filenames[j], it, rt));
                }
                catch (std::runtime_error& e)
                {
                    fprintf(stderr, "Error: cannot open alignment file %s for reading\n",
                            sam_hit_filenames[j].c_str());
                    exit(1);
                }
            }
            
            all_hit_factories.push_back(hs);
            
            shared_ptr<BundleFactory> hf(new BundleFactory(hs, REF_DRIVEN));
            shared_ptr<ReadGroupProperties> rg_props(new ReadGroupProperties);
            
            if (global_read_properties)
            {
                *rg_props = *global_read_properties;
            }
            else 
            {
                *rg_props = hs->read_group_properties();
            }
            
            rg_props->condition_name(condition_name);
            rg_props->replicate_num(j);
            rg_props->file_path(sam_hit_filenames[j]);
            
            all_read_groups.push_back(rg_props);
            
            hf->read_group_properties(rg_props);
            
            replicate_factories.push_back(hf);
            //replicate_factories.back()->set_ref_rnas(ref_mRNAs);
        }
        
        bundle_factories.push_back(shared_ptr<ReplicatedBundleFactory>(new ReplicatedBundleFactory(replicate_factories, condition_name)));
	}
    
    ::load_ref_rnas(ref_gtf, rt, ref_mRNAs, corr_bias, false);
    if (ref_mRNAs.empty())
        return;
    
    vector<shared_ptr<Scaffold> > mask_rnas;
    if (mask_gtf)
    {
        ::load_ref_rnas(mask_gtf, rt, mask_rnas, false, false);
    }
    
    BOOST_FOREACH (shared_ptr<ReplicatedBundleFactory> fac, bundle_factories)
    {
        fac->set_ref_rnas(ref_mRNAs);
        if (mask_gtf) 
            fac->set_mask_rnas(mask_rnas);
    }
    
    if (norm_standards_file != NULL)
    {
        parse_norm_standards_file(norm_standards_file);
    }
    
    
#if ENABLE_THREADS
    locus_num_threads = num_threads;
#endif
    
    // Validate the dispersion method the user's chosen.
    int most_reps = -1;
    int most_reps_idx = 0;
    
    bool single_replicate_fac = false;
    
    for (size_t i = 0; i < bundle_factories.size(); ++i)
    {
        ReplicatedBundleFactory& fac = *(bundle_factories[i]);
        if (fac.num_replicates() > most_reps)
        {
            most_reps = fac.num_replicates();
            most_reps_idx = i;
        }
        if (most_reps == 1)
        {
            single_replicate_fac = true;
            if (dispersion_method == PER_CONDITION)
            {
                fprintf(stderr, "Error: Dispersion method 'per-condition' requires that all conditions have at least 2 replicates.  Please use either 'pooled' or 'blind'\n");
                exit(1);
            }
        }
    }
    
    dispersion_method = POISSON;
    
	int tmp_min_frag_len = numeric_limits<int>::max();
	int tmp_max_frag_len = 0;
	
	ProgressBar p_bar("Inspecting maps and determining fragment length distributions.",0);
	BOOST_FOREACH (shared_ptr<ReplicatedBundleFactory> fac, bundle_factories)
    {
#if ENABLE_THREADS	
        while(1)
        {
            locus_thread_pool_lock.lock();
            if (locus_curr_threads < locus_num_threads)
            {
                break;
            }
            
            locus_thread_pool_lock.unlock();
            
            boost::this_thread::sleep(boost::posix_time::milliseconds(5));
        }
        
        locus_curr_threads++;
        locus_thread_pool_lock.unlock();
        
        thread inspect(inspect_map_worker,
                       boost::ref(*fac),
                       boost::ref(tmp_min_frag_len),
                       boost::ref(tmp_max_frag_len));  
#else
        inspect_map_worker(boost::ref(*fac),
                           boost::ref(tmp_min_frag_len),
                           boost::ref(tmp_max_frag_len));
#endif
    }
    
    // wait for the workers to finish up before reporting everthing.
#if ENABLE_THREADS	
    while(1)
    {
        locus_thread_pool_lock.lock();
        if (locus_curr_threads == 0)
        {
            locus_thread_pool_lock.unlock();
            break;
        }
        locus_thread_pool_lock.unlock();
        
        boost::this_thread::sleep(boost::posix_time::milliseconds(5));
    }
#endif
    
    normalize_counts(all_read_groups);
    
    for (size_t i = 0; i < all_read_groups.size(); ++i)
    {
        shared_ptr<ReadGroupProperties> rg = all_read_groups[i];
        fprintf(stderr, "> Map Properties:\n");
        
        fprintf(stderr, ">\tNormalized Map Mass: %.2Lf\n", rg->normalized_map_mass());
        fprintf(stderr, ">\tRaw Map Mass: %.2Lf\n", rg->total_map_mass());
        if (corr_multi)
            fprintf(stderr,">\tNumber of Multi-Reads: %zu (with %zu total hits)\n", rg->multi_read_table()->num_multireads(), rg->multi_read_table()->num_multihits()); 
        
        if (rg->frag_len_dist()->source() == LEARNED)
        {
            fprintf(stderr, ">\tFragment Length Distribution: Empirical (learned)\n");
            fprintf(stderr, ">\t              Estimated Mean: %.2f\n", rg->frag_len_dist()->mean());
            fprintf(stderr, ">\t           Estimated Std Dev: %.2f\n", rg->frag_len_dist()->std_dev());
        }
        else
        {
            if (rg->frag_len_dist()->source() == USER)
                fprintf(stderr, ">\tFragment Length Distribution: Truncated Gaussian (user-specified)\n");
            else //rg->frag_len_dist()->source == FLD::DEFAULT
                fprintf(stderr, ">\tFragment Length Distribution: Truncated Gaussian (default)\n");
            fprintf(stderr, ">\t              Default Mean: %d\n", def_frag_len_mean);
            fprintf(stderr, ">\t           Default Std Dev: %d\n", def_frag_len_std_dev);
        }
    }
    
    long double total_norm_mass = 0.0;
    long double total_mass = 0.0;
    BOOST_FOREACH (shared_ptr<ReadGroupProperties> rg_props, all_read_groups)
    {
        total_norm_mass += rg_props->normalized_map_mass();
        total_mass += rg_props->total_map_mass();
    }

	min_frag_len = tmp_min_frag_len;
    max_frag_len = tmp_max_frag_len;
	
	final_est_run = false;
	
	double num_bundles = (double)bundle_factories[0]->num_bundles();
    
    p_bar = ProgressBar("Calculating preliminary abundance estimates", num_bundles);
    
    Tracking tracking;
    
    abundance_recorder = shared_ptr<AbundanceRecorder>(new AbundanceRecorder(bundle_factories.size(), &tracking, &p_bar));
    
	if (model_mle_error || corr_bias || corr_multi) // Only run initial estimation if correcting bias or multi-reads
	{
		while (1) 
		{
			shared_ptr<vector<shared_ptr<SampleAbundances> > > abundances(new vector<shared_ptr<SampleAbundances> >());
			quantitate_next_locus(rt, bundle_factories, abundance_recorder);
			bool more_loci_remain = false;
            BOOST_FOREACH (shared_ptr<ReplicatedBundleFactory> rep_fac, bundle_factories) 
            {
                if (rep_fac->bundles_remain())
                {
                    more_loci_remain = true;
                    break;
                }
            }
            
			if (!more_loci_remain)
            {
                // wait for the workers to finish up before breaking out.
#if ENABLE_THREADS	
                while(1)
                {
                    locus_thread_pool_lock.lock();
                    if (locus_curr_threads == 0)
                    {
                        locus_thread_pool_lock.unlock();
                        break;
                    }
                    
                    locus_thread_pool_lock.unlock();
                    
                    boost::this_thread::sleep(boost::posix_time::milliseconds(5));
                    
                }
#endif
				break;
            }
		}
        
        BOOST_FOREACH (shared_ptr<ReplicatedBundleFactory> rep_fac, bundle_factories)
		{
			rep_fac->reset();
        }
        
		p_bar.complete();
	}
    if (corr_bias)
    {
        bias_run = true;
        p_bar = ProgressBar("Learning bias parameters.", 0);
		BOOST_FOREACH (shared_ptr<ReplicatedBundleFactory> rep_fac, bundle_factories)
		{
			BOOST_FOREACH (shared_ptr<BundleFactory> fac, rep_fac->factories())
			{
#if ENABLE_THREADS	
				while(1)
				{
					locus_thread_pool_lock.lock();
					if (locus_curr_threads < locus_num_threads)
					{
						break;
					}
					
					locus_thread_pool_lock.unlock();
					
					boost::this_thread::sleep(boost::posix_time::milliseconds(5));
				}
				locus_curr_threads++;
				locus_thread_pool_lock.unlock();
				
				thread bias(learn_bias_worker, fac);
#else
				learn_bias_worker(fac);
#endif
			}
    	}
    
    // wait for the workers to finish up before reporting everthing.
#if ENABLE_THREADS	
		while(1)
		{
			locus_thread_pool_lock.lock();
			if (locus_curr_threads == 0)
			{
				locus_thread_pool_lock.unlock();
				break;
			}
			
			locus_thread_pool_lock.unlock();
			
			boost::this_thread::sleep(boost::posix_time::milliseconds(5));
		}
#endif
        BOOST_FOREACH (shared_ptr<ReplicatedBundleFactory> rep_fac, bundle_factories)
		{
			rep_fac->reset();
        }
        bias_run = false;
	}
    
    fprintf(outfiles.bias_out, "condition_name\treplicate_num\tparam\tpos_i\tpos_j\tvalue\n");
    BOOST_FOREACH (shared_ptr<ReadGroupProperties> rg_props, all_read_groups)
    {
        if (rg_props->bias_learner())
            rg_props->bias_learner()->output(outfiles.bias_out, rg_props->condition_name(), rg_props->replicate_num());
    }
    
    
    // Allow the multiread tables to do their thing...
    BOOST_FOREACH (shared_ptr<ReadGroupProperties> rg_props, all_read_groups)
    {
        rg_props->multi_read_table()->valid_mass(true);
    }
    
    abundance_recorder = shared_ptr<AbundanceRecorder>(new AbundanceRecorder(bundle_factories.size(), &tracking, &p_bar));
    
	final_est_run = true;
	p_bar = ProgressBar("Testing for differential expression and regulation in locus.", num_bundles);
                                                     
	while (true)
	{
        //shared_ptr<vector<shared_ptr<SampleAbundances> > > abundances(new vector<shared_ptr<SampleAbundances> >());
        quantitate_next_locus(rt, bundle_factories, abundance_recorder);
        bool more_loci_remain = false;
        BOOST_FOREACH (shared_ptr<ReplicatedBundleFactory> rep_fac, bundle_factories) 
        {
            if (rep_fac->bundles_remain())
            {
                more_loci_remain = true;
                break;
            }
        }
        if (!more_loci_remain)
        {
            // wait for the workers to finish up before doing the cross-sample testing.
#if ENABLE_THREADS	
            while(1)
            {
                locus_thread_pool_lock.lock();
                if (locus_curr_threads == 0)
                {
                    locus_thread_pool_lock.unlock();
                    break;
                }
                
                locus_thread_pool_lock.unlock();
                
                boost::this_thread::sleep(boost::posix_time::milliseconds(5));
                
            }
#endif
            break;
        }
    }
	
	p_bar.complete();

    
    string expression_cxb_filename = output_dir + "/abundances.cxb";
    std::ofstream ofs(expression_cxb_filename.c_str());
    boost::archive::text_oarchive oa(ofs);
    
    map<string, AbundanceGroup> single_sample_tracking;
    
    const light_ab_group_tracking_table& sample_table = abundance_recorder->get_sample_table();
    for (light_ab_group_tracking_table::const_iterator itr = sample_table.begin(); itr != sample_table.end(); ++itr)
    {
        
        assert (itr->second.size() == 1);
        
        single_sample_tracking[itr->first] = itr->second[0];
    }
    
    oa << single_sample_tracking;
    
    // FPKM tracking
    
	FILE* fiso_fpkm_tracking =  outfiles.isoform_fpkm_tracking_out;
	fprintf(stderr, "Writing isoform-level FPKM tracking\n");
	print_FPKM_tracking(fiso_fpkm_tracking,tracking.isoform_fpkm_tracking); 
	
	FILE* ftss_fpkm_tracking =  outfiles.tss_group_fpkm_tracking_out;
	fprintf(stderr, "Writing TSS group-level FPKM tracking\n");
	print_FPKM_tracking(ftss_fpkm_tracking,tracking.tss_group_fpkm_tracking);
	
	FILE* fgene_fpkm_tracking =  outfiles.gene_fpkm_tracking_out;
	fprintf(stderr, "Writing gene-level FPKM tracking\n");
	print_FPKM_tracking(fgene_fpkm_tracking,tracking.gene_fpkm_tracking);
	
	FILE* fcds_fpkm_tracking =  outfiles.cds_fpkm_tracking_out;
	fprintf(stderr, "Writing CDS-level FPKM tracking\n");
	print_FPKM_tracking(fcds_fpkm_tracking,tracking.cds_fpkm_tracking);

    // Count tracking
    
    FILE* fiso_count_tracking =  outfiles.isoform_count_tracking_out;
	fprintf(stderr, "Writing isoform-level count tracking\n");
	print_count_tracking(fiso_count_tracking,tracking.isoform_fpkm_tracking); 
	
	FILE* ftss_count_tracking =  outfiles.tss_group_count_tracking_out;
	fprintf(stderr, "Writing TSS group-level count tracking\n");
	print_count_tracking(ftss_count_tracking,tracking.tss_group_fpkm_tracking);
	
	FILE* fgene_count_tracking =  outfiles.gene_count_tracking_out;
	fprintf(stderr, "Writing gene-level count tracking\n");
	print_count_tracking(fgene_count_tracking,tracking.gene_fpkm_tracking);
	
	FILE* fcds_count_tracking =  outfiles.cds_count_tracking_out;
	fprintf(stderr, "Writing CDS-level count tracking\n");
	print_count_tracking(fcds_count_tracking,tracking.cds_fpkm_tracking);
    
    // Run info
    FILE* frun_info =  outfiles.run_info_out;
	fprintf(stderr, "Writing run info\n");
	print_run_info(frun_info);
}

int main(int argc, char** argv)
{
//    boost::serialization::void_cast_register<TranscriptAbundance, Abundance>(
//                                                                             static_cast<TranscriptAbundance *>(NULL),
//                                                                             static_cast<Abundance *>(NULL)
//                                                                             );
    
    for (int i = 0; i < argc; ++i)
    {
        cmd_str += string(argv[i]) + " ";
    }
    
    init_library_table();
    init_dispersion_method_table();
    init_lib_norm_method_table();
    
    min_isoform_fraction = 1e-5;
    
	int parse_ret = parse_options(argc,argv);
    if (parse_ret)
        return parse_ret;
	
    if (!use_total_mass && !use_compat_mass)
    {
        use_total_mass = false;
        use_compat_mass = true;   
    }
    
	if(optind >= argc)
    {
        print_usage();
        return 1;
    }
    
    if (!no_update_check)
        check_version(PACKAGE_VERSION);
    
    string ref_gtf_filename = argv[optind++];
    vector<string> sam_hit_filenames;
    
    if(optind < argc)
    {
        string sam_hits_file_name = argv[optind++];
        sam_hit_filenames.push_back(sam_hits_file_name);
    }
    
    if (sample_labels.size() == 0)
    {
        for (size_t i = 1; i < sam_hit_filenames.size() + 1; ++i)
        {
            char buf[256];
            sprintf(buf, "q%lu", i);
            sample_labels.push_back(buf);
        }
    }
    	
	while (sam_hit_filenames.size() < 1)
    {
        fprintf(stderr, "Error: cuffquant requires exactly 1 SAM/BAM file\n");
        exit(1);
    }
	
    
    if (sam_hit_filenames.size() != sample_labels.size())
    {
        fprintf(stderr, "Error: number of labels must match number of conditions\n");
        exit(1);
    }
    
    if (random_seed == -1)
        random_seed = time(NULL);
    
	// seed the random number generator - we'll need it for the importance
	// sampling during MAP estimation of the gammas
	srand48(random_seed);
	
	FILE* ref_gtf = NULL;
	if (ref_gtf_filename != "")
	{
		ref_gtf = fopen(ref_gtf_filename.c_str(), "r");
		if (!ref_gtf)
		{
			fprintf(stderr, "Error: cannot open reference GTF file %s for reading\n",
					ref_gtf_filename.c_str());
			exit(1);
		}
	}
	
	FILE* mask_gtf = NULL;
	if (mask_gtf_filename != "")
	{
		mask_gtf = fopen(mask_gtf_filename.c_str(), "r");
		if (!mask_gtf)
		{
			fprintf(stderr, "Error: cannot open mask GTF file %s for reading\n",
					mask_gtf_filename.c_str());
			exit(1);
		}
	}

    FILE* norm_standards_file = NULL;
	if (norm_standards_filename != "")
	{
		norm_standards_file = fopen(norm_standards_filename.c_str(), "r");
		if (!norm_standards_file)
		{
			fprintf(stderr, "Error: cannot open contrast file %s for reading\n",
					norm_standards_filename.c_str());
			exit(1);
		}
	}
    

	// Note: we don't want the assembly filters interfering with calculations 
	// here
	
	pre_mrna_fraction = 0.0;
    olap_radius = 0;
	
	Outfiles outfiles;
	
    if (output_dir != "")
    {
        int retcode = mkpath(output_dir.c_str(), 0777);
        if (retcode == -1)
        {
            if (errno != EEXIST)
            {
                fprintf (stderr, 
                         "Error: cannot create directory %s\n", 
                         output_dir.c_str());
                exit(1);
            }
        }
    }
    
    static const int filename_buf_size = 2048;
    
    char out_file_prefix[filename_buf_size];
    sprintf(out_file_prefix, "%s/", output_dir.c_str());
    char iso_out_file_name[filename_buf_size];
    sprintf(iso_out_file_name, "%sisoform_exp.diff", out_file_prefix);
    FILE* iso_out = fopen(iso_out_file_name, "w");
    if (!iso_out)
    {
        fprintf(stderr, "Error: cannot open differential isoform transcription file %s for writing\n",
                iso_out_file_name);
        exit(1);
    }
    
	char isoform_fpkm_tracking_name[filename_buf_size];
	sprintf(isoform_fpkm_tracking_name, "%s/isoforms.fpkm_tracking", output_dir.c_str());
	FILE* isoform_fpkm_out = fopen(isoform_fpkm_tracking_name, "w");
	if (!isoform_fpkm_out)
	{
		fprintf(stderr, "Error: cannot open isoform-level FPKM tracking file %s for writing\n",
				isoform_fpkm_tracking_name);
		exit(1);
	}
	outfiles.isoform_fpkm_tracking_out = isoform_fpkm_out;

	char tss_group_fpkm_tracking_name[filename_buf_size];
	sprintf(tss_group_fpkm_tracking_name, "%s/tss_groups.fpkm_tracking", output_dir.c_str());
	FILE* tss_group_fpkm_out = fopen(tss_group_fpkm_tracking_name, "w");
	if (!tss_group_fpkm_out)
	{
		fprintf(stderr, "Error: cannot open TSS group-level FPKM tracking file %s for writing\n",
				tss_group_fpkm_tracking_name);
		exit(1);
	}
	outfiles.tss_group_fpkm_tracking_out = tss_group_fpkm_out;

	char cds_fpkm_tracking_name[filename_buf_size];
	sprintf(cds_fpkm_tracking_name, "%s/cds.fpkm_tracking", output_dir.c_str());
	FILE* cds_fpkm_out = fopen(cds_fpkm_tracking_name, "w");
	if (!cds_fpkm_out)
	{
		fprintf(stderr, "Error: cannot open CDS level FPKM tracking file %s for writing\n",
				cds_fpkm_tracking_name);
		exit(1);
	}
	outfiles.cds_fpkm_tracking_out = cds_fpkm_out;
	
	char gene_fpkm_tracking_name[filename_buf_size];
	sprintf(gene_fpkm_tracking_name, "%s/genes.fpkm_tracking", output_dir.c_str());
	FILE* gene_fpkm_out = fopen(gene_fpkm_tracking_name, "w");
	if (!gene_fpkm_out)
	{
		fprintf(stderr, "Error: cannot open gene-level FPKM tracking file %s for writing\n",
				gene_fpkm_tracking_name);
		exit(1);
	}
	outfiles.gene_fpkm_tracking_out = gene_fpkm_out;

    char isoform_count_tracking_name[filename_buf_size];
	sprintf(isoform_count_tracking_name, "%s/isoforms.count_tracking", output_dir.c_str());
	FILE* isoform_count_out = fopen(isoform_count_tracking_name, "w");
	if (!isoform_count_out)
	{
		fprintf(stderr, "Error: cannot open isoform-level count tracking file %s for writing\n",
				isoform_count_tracking_name);
		exit(1);
	}
	outfiles.isoform_count_tracking_out = isoform_count_out;
    
	char tss_group_count_tracking_name[filename_buf_size];
	sprintf(tss_group_count_tracking_name, "%s/tss_groups.count_tracking", output_dir.c_str());
	FILE* tss_group_count_out = fopen(tss_group_count_tracking_name, "w");
	if (!tss_group_count_out)
	{
		fprintf(stderr, "Error: cannot open TSS group-level count tracking file %s for writing\n",
				tss_group_count_tracking_name);
		exit(1);
	}
	outfiles.tss_group_count_tracking_out = tss_group_count_out;
    
	char cds_count_tracking_name[filename_buf_size];
	sprintf(cds_count_tracking_name, "%s/cds.count_tracking", output_dir.c_str());
	FILE* cds_count_out = fopen(cds_count_tracking_name, "w");
	if (!cds_count_out)
	{
		fprintf(stderr, "Error: cannot open CDS level count tracking file %s for writing\n",
				cds_count_tracking_name);
		exit(1);
	}
	outfiles.cds_count_tracking_out = cds_count_out;
	
	char gene_count_tracking_name[filename_buf_size];
	sprintf(gene_count_tracking_name, "%s/genes.count_tracking", output_dir.c_str());
	FILE* gene_count_out = fopen(gene_count_tracking_name, "w");
	if (!gene_count_out)
	{
		fprintf(stderr, "Error: cannot open gene-level count tracking file %s for writing\n",
				gene_count_tracking_name);
		exit(1);
	}
	outfiles.gene_count_tracking_out = gene_count_out;
    
    char run_info_name[filename_buf_size];
	sprintf(run_info_name, "%s/run.info", output_dir.c_str());
	FILE* run_info_out = fopen(run_info_name, "w");
	if (!run_info_out)
	{
		fprintf(stderr, "Error: cannot open run info file %s for writing\n",
				run_info_name);
		exit(1);
	}
	outfiles.run_info_out = run_info_out;

    char bias_name[filename_buf_size];
	sprintf(bias_name, "%s/bias_params.info", output_dir.c_str());
	FILE* bias_out = fopen(bias_name, "w");
	if (!bias_out)
	{
		fprintf(stderr, "Error: cannot open run info file %s for writing\n",
				bias_name);
		exit(1);
	}
	outfiles.bias_out = bias_out;
    
    driver(ref_gtf, mask_gtf, norm_standards_file, sam_hit_filenames, outfiles);
    
	return 0;
}

