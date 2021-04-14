from waflib.Tools.compiler_cxx import cxx_compiler
#from scripts.waf import utils

# import subprocess
import os
import sys



APPNAME = 'minervafs' #TODO: REPLACE
VERSION = '0.0.1' #TODO: REPLACE 

cxx_compiler['linux'] = ['clang++']

def options(opt) :
    opt.load('compiler_cxx')

def configure(cnf) :
    cnf.load('compiler_cxx')

    link_flags = ['-pthread', '-lz']
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

    libs = ['fuse3']

    if sys.platform == 'linux' or sys.platform == 'linux2':
        libs.append('stdc++fs')
    
    bld(name = '{}-includes'.format(APPNAME),
        includes='./src',
        export_includes='./src')

    bld.stlib(name = '{}'.format(APPNAME),
        features = 'cxx cxxprogram',
        target='{}'.format(APPNAME),
        includes='../src',
        source=bld.path.ant_glob('src/{}r/**/*.cpp'.format(APPNAME)),
        lib = libs, 
        use=['tartarus_includes', 'tartarus', 'codewrapper_includes', 'codewrapper',
             'harpocrates_includes', 'harpocrates',
             'minerva_includes', 'minerva', '{}-includes'.format(APPNAME)]
    )
    
    # Build Test
    # bld.recurse('test/TEST_NAME')
#    bld.recurse('tests/test_structure')

