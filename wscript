from waflib.Tools.compiler_cxx import cxx_compiler
#from scripts.waf import utils

# import subprocess
import os
from waf_au_extensions import utils as au_utility


APPNAME = 'minerva-safefs-layer' #TODO: REPLACE
VERSION = '0.0.1' #TODO: REPLACE 

cxx_compiler['linux'] = ['clang++']

def options(opt) :
    opt.load('compiler_cxx')

def configure(cnf) :
    cnf.load('compiler_cxx')
    cnf.env.append_value('CXXFLAGS', ['-std=c++17', '-Wall', '-Werror', '-Wextra'])
    cnf.env.append_value('LINKFLAGS',
                         ['-pthread'])


def build(bld):
    # REPLACE PROJECT NAME 
    bld(name = 'minerva-safefs-layer',
        includes='./src',
        export_includes='./src')
        
    # Build Examples
    # bld.recurse('examples/EXAMPLE_NAME')

    # Build Test
    # bld.recurse('test/TEST_NAME')

def test(t):
    au_utility.run_tests('build/test')

def doc(dc):
    au_utility.generate_documentation()
