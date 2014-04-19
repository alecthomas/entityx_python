#!/usr/bin/python

import os
import sys


def options(opt):
    opt.load('compiler_cxx boost waf_unit_test')
    opt.add_option('--no-shared', action='store_true', help='Do not build shared libraries')
    opt.add_option('--mode', action='store', default='release', help='Compile mode: release or debug')


def configure(conf):
    if conf.options.mode == 'release':
        conf.env.append_value('CXXFLAGS', ['-O3'])
    else:
        conf.env.append_value('CXXFLAGS', ['-g', '-O0'])
        conf.env.append_value('LDFLAGS', ['-g'])

    if sys.platform == 'darwin' and not conf.env.CXX:
        conf.env.CXX = 'clang++'
        conf.env.append_value('CXXFLAGS', ['-std=c++11', '-stdlib=libc++', '-Wall'])
        conf.env.append_value('LDFLAGS', ['-std=c++11', '-stdlib=libc++'])

    conf.load('compiler_cxx boost waf_unit_test')
    conf.check_boost(lib='python')
    conf.check_cfg(package='python', path='python-config',
                   args=['--includes', '--libs'],
                   uselib_store='PYTHON')
    conf.check(features='cxx cxxprogram',
               msg='Setting test data location.',
               defines=['ENTITYX_PYTHON_TEST_DATA="%s"' % os.path.join(os.getcwd(), 'entityx/python')],
               uselib_store='TEST_DATA')
    conf.check_cxx(features='cxx cxxprogram',
                   lib=['entityx'],
                   cflags=['-Wall'],
                   uselib_store='ENTITYX')
    conf.env.append_value('INCLUDES', ['#/'])
    conf.define('ENTITYX_INSTALLED_PYTHON_PACKAGE_DIR',
                '{PREFIX}/share/entityx/python'.format(PREFIX=conf.env.PREFIX))
    conf.write_config_header('entityx/python/config.h', top=True)


def build(bld):
    bld.stlib(target='entityx_python',
              features='cxx',
              source='entityx/python/PythonSystem.cc',
              use='BOOST PYTHON ENTITYX',
              install_path='${PREFIX}/lib')
    from waflib.Tools import waf_unit_test
    bld.add_post_fun(waf_unit_test.summary)
    if not bld.options.no_shared:
        bld.shlib(target='entityx_python',
                  features='cxx',
                  source='entityx/python/PythonSystem.cc',
                  use='BOOST PYTHON ENTITYX',
                  install_path='${PREFIX}/lib')
    bld.program(features='cxx cxxprogram test',
                source='entityx/python/PythonSystem_test.cc',
                includes='.',
                target='python_test',
                use='entityx_python TEST_DATA')

    bld.install_files('${PREFIX}/include/entityx/python',
                      ['entityx/python/PythonSystem.h',
                       'entityx/python/config.h'])
    bld.install_as('${PREFIX}/share/entityx/python/entityx.py',
                   'entityx/python/entityx/__init__.py')
