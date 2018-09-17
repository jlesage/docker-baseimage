#!/usr/bin/python3

import os
import sys
import argparse
import yaml

DEFS = None

def loop_flavors(callback):
    """Loop through all flavors."""
    for os in DEFS['flavors']:
        for os_var in [ os ] + DEFS['flavors'][os]['variants']:
            for rel in DEFS['flavors'][os]['releases']:
                if os != os_var:
                    flavor = '{}-{}-{}'.format(os, rel, os_var)
                else:
                    flavor = '{}-{}'.format(os, rel)
                callback(flavor, os, os_var, rel)

def list_flavors(args):
    """List all Docker image flavors."""
    def callback(flavor, os, os_var, rel):
        print(flavor)
    loop_flavors(callback)

def list_archs(args):
    """List all Docker image architectures."""
    for arch in DEFS['architectures']:
        print(arch)

def print_build_env(args):
    """Print the build environment associated to Docker image flavor."""
    def callback(flavor, os, os_var, rel):
        if flavor == args.flavor:
            arch = args.arch
            baseimage_arch = arch
            if 'baseimage_arch' in DEFS['architectures'][arch] and os in DEFS['architectures'][arch]['baseimage_arch']:
                baseimage_arch = DEFS['architectures'][arch]['baseimage_arch'][os]
            glibc_arch = DEFS['architectures'][arch].get('glibc_arch', arch)
            qemu_arch = DEFS['architectures'][arch].get('qemu_arch', arch)

            print("DOCKER_IMAGE_BUILD_ARG_ARCH={}".format(arch))
            print("DOCKER_IMAGE_BUILD_ARG_DOCKERFILE={}".format(DEFS['flavors'][os]['dockerfile']))
            print("DOCKER_IMAGE_BUILD_ARG_BASEIMAGE={}/{}".format(baseimage_arch, DEFS['flavors'][os]['releases'][rel]['baseimage']))
            print("DOCKER_IMAGE_BUILD_ARG_GLIBC={}".format('1' if os_var == 'glibc' else '0'))
            print("DOCKER_IMAGE_BUILD_ARG_GLIBC_ARCH={}".format(glibc_arch))
            print("DOCKER_IMAGE_BUILD_ARG_QEMU_ARCH={}".format(qemu_arch))
            sys.exit(0)

    if not args.arch in DEFS['architectures']:
        print("Invalid Docker image architecture.")
        exit(1)

    loop_flavors(callback)
    print("Invalid Docker image flavor.")
    sys.exit(1)

def print_travis_matrix(args):
    """Print the Travis matrix to builld all Docker image flavors under all
       architectures."""
    def callback(flavor, os, os_var, rel):
        print("  - DOCKER_IMAGE_ARCH={:8s} DOCKER_IMAGE_FLAVOR={}".format(arch, flavor))
    for arch in DEFS['architectures']:
        loop_flavors(callback)

def get_qemu_arch(args):
    """Get the QEMU arch."""
    print("{}".format(DEFS['architectures'][args.arch].get('qemu_arch', args.arch)))

def main():
    # Define the argument parser.
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers()

    list_flavors_parser = subparsers.add_parser('list-flavors')
    list_flavors_parser.set_defaults(func=list_flavors)

    list_archs_parser = subparsers.add_parser('list-archs')
    list_archs_parser.set_defaults(func=list_archs)

    print_build_env_parser = subparsers.add_parser('print-build-env')
    print_build_env_parser.add_argument('flavor',  help='flavor of the Docker image')
    print_build_env_parser.add_argument('arch',  help='architecture of the Docker image')
    print_build_env_parser.set_defaults(func=print_build_env)

    print_travis_matrix_parser = subparsers.add_parser('print-travis-matrix')
    print_travis_matrix_parser.set_defaults(func=print_travis_matrix)

    get_qemu_arch_parser = subparsers.add_parser('get-qemu-arch')
    get_qemu_arch_parser.add_argument('arch',  help='architecture of the Docker image')
    get_qemu_arch_parser.set_defaults(func=get_qemu_arch)

    # Parse arguments.
    if len(sys.argv) == 1:
        parser.print_help()
        sys.exit()
    else:
        args = parser.parse_args()

    # Load definitions.
    baseimage_defs = os.path.dirname(os.path.realpath(__file__)) + '/baseimage_defs.yaml'
    with open(baseimage_defs, 'r') as stream:
        try:
            global DEFS
            DEFS = yaml.load(stream)
        except yaml.YAMLError as exc:
            print(exc)
            exit(1)

    # Invoke command's function.
    args.func(args)

if __name__ == "__main__":
    main()

# vim:ts=4:sw=4:et:sts=4
