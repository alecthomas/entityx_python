import os
import sys


def options(opt):
    opt.load('compiler_cxx boost unittest_gtest')


def configure(conf):
    if sys.platform == 'darwin' and not conf.env.CXX:
        conf.env.CXX = 'clang++'

    conf.env.append_value('CXXFLAGS',
                          ['-DGTEST_USE_OWN_TR1_TUPLE=1', '-DBOOST_NO_CXX11_NUMERIC_LIMITS=1'])
    conf.load('compiler_cxx boost unittest_gtest')
    conf.env.append_value('CXXFLAGS',
                          ['-std=c++11', '-stdlib=libc++', '-g', '-Wall'])
    conf.env.append_value('LDFLAGS',
                          ['-std=c++11', '-stdlib=libc++', '-g'])
    conf.check_boost(lib='python', static='onlystatic')
    conf.check_cfg(package='python', path='python-config',
                   args=['--includes', '--libs'],
                   uselib_store='PYTHON')
    conf.check(features='cxx cxxprogram',
               lib=['entityx'],
               cflags=['-Wall'],
               defines=['ENTITYX_PYTHON_TEST_DATA="%s"' % os.getcwd()],
               uselib_store='ENTITYX')


def build(bld):
    bld.stlib(target='entityx_python',
              features=['cxx'],
              source='PythonSystem.cc',
              use='BOOST PYTHON ENTITYX')
    bld.shlib(target='entityx_python',
              features=['cxx'],
              source='PythonSystem.cc',
              use='BOOST PYTHON ENTITYX')
    bld.program(features='gtest',
                source='PythonSystem_test.cc',
                includes='.',
                target='python_test',
                use='entityx_python')
