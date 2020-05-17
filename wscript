from waflib.Tools.compiler_cxx import cxx_compiler
#from scripts.waf import utils

# import subprocess
import os
import sys
from waf_au_extensions import utils as au_utility


APPNAME = 'minerva-safefs-layer' #TODO: REPLACE
VERSION = '0.0.1' #TODO: REPLACE 

cxx_compiler['linux'] = ['clang++']

def options(opt) :
    opt.load('compiler_cxx')

def configure(cnf) :
    cnf.load('compiler_cxx')

    link_flags = ['-pthread']
    cxx_flags = ['-std=c++17', '-g', '-Wall', '-Werror', '-Wextra', '-O3', '-D_FILE_OFFSET_BITS=64']
    
    if sys.platform == 'linux' or sys.platform == 'linux2':
        link_flags.append('-lstdc++fs')
        link_flags.append('-lfuse3')

    if sys.platform == 'darwin':
        link_flags.append('-L/usr/local/opt/llvm/lib')
        cxx_flags.append('-stdlib=libc++')
        link_flags.append('-lfuse3')

    cnf.env.append_value('CXXFLAGS', cxx_flags)        
    cnf.env.append_value('LINKFLAGS',
                         link_flags)
    
def build(bld):

    libs = ['cryptopp', 'fuse3']

    if sys.platform == 'linux' or sys.platform == 'linux2':
        libs.append('stdc++fs')
        
    if sys.platform == 'darwin':
        libs.append('c++fs')
        
    bld(name = 'minerva-safefs-layer-includes',
        includes='./src',
        export_includes='./src')

    bld.stlib(name = 'minerva-safefs-layer',
        features = 'cxx cxxstlib',
        target='minerva-safefs-layer',
        includes='../src',
        source=bld.path.ant_glob('src/minerva-safefs-layer/**/*.cpp'),
        lib = libs, 
        use=['tartarus_includes', 'tartarus', 'codewrapper_includes', 'codewrapper',
             'harpocrates_includes', 'harpocrates',
             'minerva_includes', 'minerva', 'minerva-safefs-layer-includes']
    )

    
    bld.shlib(name = 'minerva-safefs-layer-shared',                    
        features = 'cxx',                                              
        target='minerva-safefs-layer-shared',                                 
        includes='../src',                                             
        source=bld.path.ant_glob('src/minerva-safefs-layer/**/*.cpp'),
        lib = libs,               
        link_flags=['-Wl', '--whole-archive'],
        use=['tartarus_includes', 'tartarus_shared',
             'codewrapper_includes', 'codewrapper_shared',
             'harpocrates_includes', 'harpocrates',
             'minerva_includes', 'minerva' 'minerva-safefs-layer-includes']                          
    )                                                                  
    
    # Build Examples
    bld.recurse('examples/minervafs-example')

    # Build Test
    # bld.recurse('test/TEST_NAME')

def test(t):
    au_utility.run_tests('build/test')

def doc(dc):
    au_utility.generate_documentation()
