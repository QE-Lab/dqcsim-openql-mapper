#!/usr/bin/env python3

import os, platform, shutil, sys, subprocess
from distutils.command.bdist import bdist as _bdist
from distutils.command.sdist import sdist as _sdist
from distutils.command.build import build as _build
from distutils.command.clean import clean as _clean
from setuptools.command.egg_info import egg_info as _egg_info
import distutils.cmd
import distutils.log
from setuptools import setup, Extension, find_packages
from wheel.bdist_wheel import bdist_wheel as _bdist_wheel

debug = 'DQCSIM_DEBUG' in os.environ

target_dir = os.getcwd() + "/target"
py_target_dir = target_dir + "/python"
include_dir = target_dir + "/include"
output_dir = target_dir + ("/debug" if debug else "/release")
build_dir = py_target_dir + "/build"
dist_dir = py_target_dir + "/dist"

def read(fname):
    with open(os.path.join(os.path.dirname(__file__), fname)) as f:
        return f.read()

class clean(_clean):
    def run(self):
        _clean.run(self)
        shutil.rmtree(target_dir)

class build(_build):
    def initialize_options(self):
        _build.initialize_options(self)
        self.build_base = build_dir

    def run(self):
        from plumbum import local, FG

        local['mkdir']("-p", output_dir)

        with local.cwd(output_dir):
            if debug:
                local['cmake']['../..']['-DCMAKE_BUILD_TYPE=Debug'] & FG
            else:
                local['cmake']['../..'] & FG
            local['make']['-j']['4'] & FG

        _build.run(self)

class bdist(_bdist):
    def finalize_options(self):
        _bdist.finalize_options(self)
        self.dist_dir = dist_dir

class sdist(_sdist):
    def finalize_options(self):
        _sdist.finalize_options(self)
        self.dist_dir = dist_dir

class egg_info(_egg_info):
    def initialize_options(self):
        _egg_info.initialize_options(self)
        self.egg_base = py_target_dir


setup(
    name = 'dqcsim-openql-mapper',
    version = '0.0.2',

    author = 'Quantum Computer Architectures, Quantum & Computer Engineering, QuTech, Delft University of Technology',
    author_email = '',
    description = 'The OpenQL mapper wrapped in a DQCsim operator',
    license = "Apache-2.0",
    url = "https://github.com/QE-Lab/dqcsim-openql-mapper",
    project_urls = {
        "Source Code": "https://github.com/QE-Lab/dqcsim-openql-mapper",
    },

    long_description = read('README.md'),
    long_description_content_type = 'text/markdown',

    classifiers = [
        "License :: OSI Approved :: Apache Software License",

        "Operating System :: POSIX :: Linux",

        "Programming Language :: C",
        "Programming Language :: C++",

        "Topic :: Scientific/Engineering"
    ],

    data_files = [
        ('bin', [
            output_dir + '/dqcsopopenql-mapper',
        ]),
    ],

    packages = find_packages('python'),
    package_dir = {
        '': 'python',
    },

    entry_points = {
        'console_scripts': [
            'platform2gates = dqcsim_openql_mapper:platform2gates_cli',
        ],
    },

    cmdclass = {
        'bdist': bdist,
        'build': build,
        'clean': clean,
        'egg_info': egg_info,
        'sdist': sdist,
    },

    setup_requires = [
        'plumbum',
        'delocate; platform_system == "Darwin"',
    ],

    install_requires = [
        'dqcsim>=0.0.13',
    ],

    tests_require = [
        'nose',
        'dqcsim-qx'
    ],
    test_suite = 'nose.collector',

    zip_safe = False,
)
