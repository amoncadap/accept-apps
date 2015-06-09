#!/usr/bin/env python

# script for parallelizing PERFECT suite error injection experiments on cluster

# inputs:
# output directory: directory where all experiment output directories are created

import sys
import os
import subprocess

import cw.client
import cw.slurm

import genConfigs
import perfectExperiments


# generate set of configuration files for kernel
# needs to be run once per set of experiments for kernel
# returns path to configuration files directory
def buildConfigs(kernel, cfoutdir):
    if not os.path.exists(cfoutdir):
        os.makedirs(cfoutdir)
    genConfigs.genConfigs(kernel, cfoutdir)

# invoke run-kernel.sh
def runexp(app, kernel, config, outdir, ofname, ifname, runOctave):
    intRunOctave = 1 if runOctave else 0
    if ifname == None:
        print('{0} {1} {2} {3} {4} {5}'.format(app,kernel,config,outdir,ofname,intRunOctave))
        subprocess.call(["./run-kernel.sh", app, kernel, config, outdir, ofname, str(intRunOctave)])
    else: 
        print('{0} {1} {2} {3} {4} {5} {6}'.format(app,kernel,config,outdir,ofname,intRunOctave,ifname))
        subprocess.call(["./run-kernel.sh", app, kernel, config, outdir, ofname, str(intRunOctave), ifname])

def completion(jobid, output):
    print(u'finished running job {0}'.format(jobid))


# main routine: run all experiments
# args:
# 1. outdir: output directory for all experiments
def main(outdir, n):
    
    print(outdir)

    # kernels describes the app, kernel, input, output, precise-output, for each kernel
    kernels = perfectExperiments.getKernels();

    # fire up worker cluster
    cw.slurm.start(nworkers=n)
    # setup the client
    client = cw.client.ClientThread(completion, cw.slurm.master_host())
    client.start()

    for (a,k,ifname,ofname,runOctave) in kernels:
        print('{0} {1} {2} {3} {4}'.format(a,k,ifname,ofname,runOctave))

        # generate path for base output directory for this app/kernel
        koutdir = os.path.join(os.path.join(outdir, a), k)

        # generate path for configuration files & build configuration files
        cfoutdir = os.path.join(koutdir, 'inject_configs')
        buildConfigs(k,cfoutdir)
        
        # run one experiment per configuration
        for (dirpath, dirnames, filenames) in os.walk(cfoutdir):
            print(filenames)
            for cf in sorted(filenames):
                cffile = os.path.join(cfoutdir,cf)
                jobid = cw.randid()
                print(u'submitting job {0} on config {1}'.format(jobid, cf))
                client.submit(jobid, runexp, a,k,cffile,koutdir,ofname,ifname,runOctave)

    # wait for all jobs to finish
    client.wait()

    # shut down worker cluster
    cw.slurm.stop()


def usage():
    print('usage: python run.py outdir nworkers')
    print('outdir: base output directory')
    print('nworkers: number of worker threads')
    exit(0)

if __name__ == "__main__":
    if len(sys.argv) == 3:
        main(sys.argv[1], int(sys.argv[2]))
    else:
        usage()
